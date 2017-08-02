/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                   Copyright (c) 2014-2017 Datalight, Inc.
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
    comply with the terms of the GPLv2 license may obtain a commercial license
    before incorporating Reliance Edge into proprietary software for
    distribution in any form.  Visit http://www.datalight.com/reliance-edge for
    more information.
*/
/** @brief
    @file FreeRTOS block device implementation; see osbdev.c for details.
*/
#ifndef OSBDEV_FDRIVER_H
#define OSBDEV_FDRIVER_H


#include <api_mdriver.h>


/*  This must be declared and initialized elsewere (e.g., in project code) to
    point at the initialization function for the F_DRIVER block device.
*/
extern const F_DRIVERINIT gpfnRedOsBDevInit;

static F_DRIVER *gapFDriver[REDCONF_VOLUME_COUNT];


/** @brief Initialize a disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    initialized.
    @param mode     The open mode, indicating the type of access required.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS DiskOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    REDSTATUS       ret;

    (void)mode;

    if((gpfnRedOsBDevInit == NULL) || (gapFDriver[bVolNum] != NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        F_DRIVER *pDriver;

        pDriver = gpfnRedOsBDevInit(bVolNum);
        if(pDriver != NULL)
        {
            F_PHY   geom;
            int     iErr;

            /*  Validate that the geometry is consistent with the volume
                configuration.
            */
            iErr = pDriver->getphy(pDriver, &geom);
            if(iErr == 0)
            {
                if(    (geom.bytes_per_sector != gaRedVolConf[bVolNum].ulSectorSize)
                    || (geom.number_of_sectors < gaRedVolConf[bVolNum].ullSectorCount))
                {
                    ret = -RED_EINVAL;
                }
                else
                {
                    gapFDriver[bVolNum] = pDriver;
                    ret = 0;
                }
            }
            else
            {
                ret = -RED_EIO;
            }

            if(ret != 0)
            {
                pDriver->release(pDriver);
            }
        }
        else
        {
            ret = -RED_EIO;
        }
    }

    return ret;
}


/** @brief Uninitialize a disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    uninitialized.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0   Operation was successful.
*/
static REDSTATUS DiskClose(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;

    if(gapFDriver[bVolNum] == NULL)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        gapFDriver[bVolNum]->release(gapFDriver[bVolNum]);
        gapFDriver[bVolNum] = NULL;

        ret = 0;
    }

    return ret;
}


/** @brief Read sectors from a disk.

    @param bVolNum          The volume number of the volume whose block device
                            is being read from.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to read.
    @param pBuffer          The buffer into which to read the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS DiskRead(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    void       *pBuffer)
{
    REDSTATUS   ret = 0;
    F_DRIVER   *pDriver = gapFDriver[bVolNum];

    if(pDriver == NULL)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        uint8_t    *pbBuffer = CAST_VOID_PTR_TO_UINT8_PTR(pBuffer);
        uint32_t    ulSectorSize = gaRedVolConf[bVolNum].ulSectorSize;
        uint32_t    ulSectorIdx;
        int         iErr;

        for(ulSectorIdx = 0U; ulSectorIdx < ulSectorCount; ulSectorIdx++)
        {
            iErr = pDriver->readsector(pDriver, &pbBuffer[ulSectorIdx * ulSectorSize],
                                       CAST_ULONG(ullSectorStart + ulSectorIdx));
            if(iErr != 0)
            {
                ret = -RED_EIO;
                break;
            }
        }
    }

    return ret;
}


#if REDCONF_READ_ONLY == 0

/** @brief Write sectors to a disk.

    @param bVolNum          The volume number of the volume whose block device
                            is being written to.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to write.
    @param pBuffer          The buffer from which to write the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL The block device is not open.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS DiskWrite(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    const void *pBuffer)
{
    REDSTATUS   ret = 0;
    F_DRIVER   *pDriver = gapFDriver[bVolNum];

    if(pDriver == NULL)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        const uint8_t  *pbBuffer = CAST_VOID_PTR_TO_CONST_UINT8_PTR(pBuffer);
        uint32_t        ulSectorSize = gaRedVolConf[bVolNum].ulSectorSize;
        uint32_t        ulSectorIdx;
        int             iErr;

        for(ulSectorIdx = 0U; ulSectorIdx < ulSectorCount; ulSectorIdx++)
        {
            /*  We have to cast pbBuffer to non-const since the writesector
                prototype is flawed, using a non-const pointer for the buffer.
            */
            iErr = pDriver->writesector(pDriver, CAST_AWAY_CONST(uint8_t, &pbBuffer[ulSectorIdx * ulSectorSize]),
                                        CAST_ULONG(ullSectorStart + ulSectorIdx));
            if(iErr != 0)
            {
                ret = -RED_EIO;
                break;
            }
        }
    }

    return ret;
}


/** @brief Flush any caches beneath the file system.

    @param bVolNum  The volume number of the volume whose block device is being
                    flushed.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0   Operation was successful.
*/
static REDSTATUS DiskFlush(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;

    if(gapFDriver[bVolNum] == NULL)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        /*  The F_DRIVER interface does not include a flush function, so to be
            reliable the F_DRIVER implementation must use synchronous writes.
        */
        ret = 0;
    }

    return ret;
}


#if REDCONF_DISCARDS == 1
/** @brief Discard sectors on a disk.

    @param bVolNum          The volume number of the volume whose block device
                            is being accessed.
    @param ullSectorStart   The starting sector number.
    @param ullSectorCount   The number of sectors to discard.
*/
static void DiskDiscard(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint64_t    ullSectorCount)
{
#error "F_DRIVER block device interface does not support discards."
}
#endif /* REDCONF_DISCARDS == 1 */

#endif /* REDCONF_READ_ONLY == 0 */


#endif /* OSBDEV_FDRIVER_H */

