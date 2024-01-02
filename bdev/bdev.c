/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2024 Tuxera US Inc.
                      All Rights Reserved Worldwide.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; use version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but "AS-IS," WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
/*  Businesses and individuals that for commercial or other reasons cannot
    comply with the terms of the GPLv2 license must obtain a commercial
    license before incorporating Reliance Edge into proprietary software
    for distribution in any form.

    Visit https://www.tuxera.com/products/reliance-edge/ for more information.
*/
/** @file
    @brief Implements the block device abstraction of the file system.
*/
#include <redfs.h>
#include <redvolume.h>
#include <redbdev.h>


BDEVINFO gaRedBdevInfo[REDCONF_VOLUME_COUNT];


/** @brief Initialize a block device.

    This function is called when the file system needs access to a block
    device.

    Upon successful return, the block device should be fully initialized and
    ready to service read/write/flush/close requests.

    The behavior of calling this function on a block device which is already
    open is undefined.

    @param bVolNum  The volume number of the volume whose block device is being
                    initialized.
    @param mode     The open mode, indicating the type of access required.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number.
    @retval -RED_EIO    A disk I/O error occurred.
*/
REDSTATUS RedBDevOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    REDSTATUS       ret;

    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        ret = RedOsBDevOpen(bVolNum, mode);
    }

    if(ret == 0)
    {
        BDEVINFO       *pBdevInfo = &gaRedBdevInfo[bVolNum];
        const VOLCONF  *pVolConf = &gaRedVolConf[bVolNum];
        BDEVINFO        info;

        if(    (pVolConf->ullSectorCount == SECTOR_COUNT_AUTO)
            || (pVolConf->ulSectorSize == SECTOR_SIZE_AUTO))
        {
            ret = RedOsBDevGetGeometry(bVolNum, &info);
            if(ret == 0)
            {
                if(    (pVolConf->ullSectorCount != SECTOR_COUNT_AUTO)
                    && (pVolConf->ullSectorCount != info.ullSectorCount))
                {
                    REDERROR();
                    ret = -RED_EINVAL;
                }
                else if(    (pVolConf->ulSectorSize != SECTOR_COUNT_AUTO)
                         && (pVolConf->ulSectorSize != info.ulSectorSize))
                {
                    REDERROR();
                    ret = -RED_EINVAL;
                }
                else if(pVolConf->ullSectorOffset >= info.ullSectorCount)
                {
                    REDERROR();
                    ret = -RED_EINVAL;
                }
                else
                {
                    *pBdevInfo = info;

                    /*  Volumes which begin at a sector offset and are of
                        automatically-detected size, extend from the sector
                        offset to the end of the media.  The block device
                        will return the total size of the media, so the
                        adjustment happens here.
                    */
                    pBdevInfo->ullSectorCount -= pVolConf->ullSectorOffset;
                }
            }
        }
        else
        {
            pBdevInfo->ullSectorCount = pVolConf->ullSectorCount;
            pBdevInfo->ulSectorSize = pVolConf->ulSectorSize;

            /*  Query the geometry (if supported) to validate that the
                statically configured geometry is compatible with the
                block device.
            */
            ret = RedOsBDevGetGeometry(bVolNum, &info);
            if(ret == 0)
            {
                if(!VOLUME_SECTOR_GEOMETRY_IS_VALID(bVolNum, info.ulSectorSize, info.ullSectorCount))
                {
                    /*  The statically configured geometry is incompatible with
                        the reported geometry.
                    */
                    ret = -RED_EINVAL;
                }
            }
            else if(ret == -RED_ENOTSUPP)
            {
                /*  Querying the geometry is not supported, so we can't
                    validate it.
                */
                ret = 0;
            }
            else
            {
                /*  Unexpected error.
                */
            }
        }

        if(ret != 0)
        {
            (void)RedOsBDevClose(bVolNum);
        }
    }

    return ret;
}


/** @brief Uninitialize a block device.

    This function is called when the file system no longer needs access to a
    block device.  If any resource were allocated by RedBDevOpen() to service
    block device requests, they should be freed at this time.

    Upon successful return, the block device must be in such a state that it
    can be opened again.

    The behavior of calling this function on a block device which is already
    closed is undefined.

    @param bVolNum  The volume number of the volume whose block device is being
                    uninitialized.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0       Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number.
*/
REDSTATUS RedBDevClose(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;

    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        ret = RedOsBDevClose(bVolNum);
    }

    return ret;
}


/** @brief Read sectors from a physical block device.

    The behavior of calling this function is undefined if the block device is
    closed or if it was opened with ::BDEV_O_WRONLY.

    @param bVolNum          The volume number of the volume whose block device
                            is being read from.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to read.
    @param pBuffer          The buffer into which to read the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number, @p pBuffer is
                        `NULL`, or @p ullStartSector and/or @p ulSectorCount
                        refer to an invalid range of sectors.
    @retval -RED_EIO    A disk I/O error occurred.
*/
REDSTATUS RedBDevRead(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    void       *pBuffer)
{
    REDSTATUS   ret;

    if(    (bVolNum >= REDCONF_VOLUME_COUNT)
        || !VOLUME_SECTOR_RANGE_IS_VALID(bVolNum, ullSectorStart, ulSectorCount)
        || (pBuffer == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        ret = RedOsBDevRead(bVolNum, ullSectorStart, ulSectorCount, pBuffer);
    }

    return ret;
}


#if REDCONF_READ_ONLY == 0
/** @brief Write sectors to a physical block device.

    The behavior of calling this function is undefined if the block device is
    closed or if it was opened with ::BDEV_O_RDONLY.

    @param bVolNum          The volume number of the volume whose block device
                            is being written to.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to write.
    @param pBuffer          The buffer from which to write the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number, @p pBuffer is
                        `NULL`, or @p ullStartSector and/or @p ulSectorCount
                        refer to an invalid range of sectors.
    @retval -RED_EIO    A disk I/O error occurred.
*/
REDSTATUS RedBDevWrite(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    const void *pBuffer)
{
    REDSTATUS   ret;

    if(    (bVolNum >= REDCONF_VOLUME_COUNT)
        || !VOLUME_SECTOR_RANGE_IS_VALID(bVolNum, ullSectorStart, ulSectorCount)
        || (pBuffer == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        ret = RedOsBDevWrite(bVolNum, ullSectorStart, ulSectorCount, pBuffer);
    }

    return ret;
}


/** @brief Flush any caches beneath the file system.

    This function must synchronously flush all software and hardware caches
    beneath the file system, ensuring that all sectors written previously are
    committed to permanent storage.

    If the environment has no caching beneath the file system, the
    implementation of this function can do nothing and return success.

    The behavior of calling this function is undefined if the block device is
    closed or if it was opened with ::BDEV_O_RDONLY.

    @param bVolNum  The volume number of the volume whose block device is being
                    flushed.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number.
    @retval -RED_EIO    A disk I/O error occurred.
*/
REDSTATUS RedBDevFlush(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;

    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        ret = RedOsBDevFlush(bVolNum);
    }

    return ret;
}
#endif /* REDCONF_READ_ONLY == 0 */

