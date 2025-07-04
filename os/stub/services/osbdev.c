/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2025 Tuxera US Inc.
                      All Rights Reserved Worldwide.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; use version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but "AS-IS," WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, see <https://www.gnu.org/licenses/>.
*/
/*  Businesses and individuals that for commercial or other reasons cannot
    comply with the terms of the GPLv2 license must obtain a commercial
    license before incorporating Reliance Edge into proprietary software
    for distribution in any form.

    Visit https://www.tuxera.com/products/tuxera-edge-fs/ for more information.
*/
/** @file
    @brief Implements block device I/O.
*/
#include <redfs.h>
#include <redvolume.h>
#include <redbdev.h>


/** @brief Configure a block device.

    In some operating environments, block devices need to be configured with
    run-time context information that is only available at higher layers.
    For example, a block device might need to be associated with a block
    device handle or a device string.  This API allows that OS-specific
    context information to be passed down from the higher layer (e.g., a
    VFS implementation) to the block device OS service, which can save it
    for later use.

    Not all OS ports will call RedOsBDevConfig().  If called, it will be called
    while the block device is closed, prior to calling RedOsBDevOpen().

    @param bVolNum  The volume number of the volume to configure.
    @param context  OS-specific block device context information.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is not a valid volume number.
*/
REDSTATUS RedOsBDevConfig(
    uint8_t     bVolNum,
    REDBDEVCTX  context)
{
    REDSTATUS   ret;

    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        (void)context;

        ret = -RED_ENOSYS;
    }

    return ret;
}


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
REDSTATUS RedOsBDevOpen(
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
        (void)mode;

        ret = -RED_ENOSYS;
    }

    return ret;
}


/** @brief Uninitialize a block device.

    This function is called when the file system no longer needs access to a
    block device.  If any resource were allocated by RedOsBDevOpen() to service
    block device requests, they should be freed at this time.

    Upon successful return, the block device must be in such a state that it
    can be opened again.

    The behavior of calling this function on a block device which is already
    closed is undefined.

    @param bVolNum  The volume number of the volume whose block device is being
                    uninitialized.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number.
*/
REDSTATUS RedOsBDevClose(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;

    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        REDERROR();
        ret = -RED_ENOSYS;
    }

    return ret;
}


/** @brief Return the block device geometry.

    The behavior of calling this function is undefined if the block device is
    closed.

    @param bVolNum  The volume number of the volume whose block device geometry
                    is being queried.
    @param pInfo    On successful return, populated with the geometry of the
                    block device.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0               Operation was successful.
    @retval -RED_EINVAL     @p bVolNum is an invalid volume number, or @p pInfo
                            is `NULL`.
    @retval -RED_EIO        A disk I/O error occurred.
    @retval -RED_ENOTSUPP   The geometry cannot be queried on this block device.
*/
REDSTATUS RedOsBDevGetGeometry(
    uint8_t     bVolNum,
    BDEVINFO   *pInfo)
{
    REDSTATUS   ret;

    if((bVolNum >= REDCONF_VOLUME_COUNT) || (pInfo == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        REDERROR();
        ret = -RED_ENOSYS;
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
REDSTATUS RedOsBDevRead(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    void       *pBuffer)
{
    REDSTATUS   ret = 0;

    if(    (bVolNum >= REDCONF_VOLUME_COUNT)
        || !VOLUME_SECTOR_RANGE_IS_VALID(bVolNum, ullSectorStart, ulSectorCount)
        || (pBuffer == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        REDERROR();
        ret = -RED_ENOSYS;
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
REDSTATUS RedOsBDevWrite(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    const void *pBuffer)
{
    REDSTATUS   ret = 0;

    if(    (bVolNum >= REDCONF_VOLUME_COUNT)
        || !VOLUME_SECTOR_RANGE_IS_VALID(bVolNum, ullSectorStart, ulSectorCount)
        || (pBuffer == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        REDERROR();
        ret = -RED_ENOSYS;
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
REDSTATUS RedOsBDevFlush(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;

    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        REDERROR();
        ret = -RED_ENOSYS;
    }

    return ret;
}
#endif /* REDCONF_READ_ONLY == 0 */
