/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2021 Tuxera US Inc.
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
    comply with the terms of the GPLv2 license must obtain a commercial license
    before incorporating Reliance Edge into proprietary software for
    distribution in any form.  Visit http://www.datalight.com/reliance-edge for
    more information.
*/
/** @brief
    @file FreeRTOS block device implementation; see osbdev.c for details.
*/
#ifndef OSBDEV_FATFS_H
#define OSBDEV_FATFS_H


#include <task.h>
#include <diskio.h>

/*  disk_read() and disk_write() use an unsigned 8-bit value to specify the
    sector count, so no transfer can be larger than 255 sectors.
*/
#define MAX_SECTOR_TRANSFER UINT8_MAX


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
    DSTATUS         status;
    uint32_t        ulTries;
    REDSTATUS       ret = 0;

    /*  With some implementations of disk_initialize(), such as the one
        implemented by Atmel for the ASF, the first time the disk is opened, the
        SD card can take a while to get ready, in which time disk_initialize()
        returns an error.  Try numerous times, waiting half a second after each
        failure.  Empirically, this has been observed to succeed on the second
        try, so trying 10x more than that provides a margin of error.
    */
    for(ulTries = 0U; ulTries < 20U; ulTries++)
    {
        /*  Assuming that the volume number is also the correct drive number.
            If this is not the case in your environment, a static constant array
            can be declared to map volume numbers to the correct driver number.
        */
        status = disk_initialize(bVolNum);
        if(status == 0)
        {
            break;
        }

        vTaskDelay(500U / portTICK_PERIOD_MS);
    }

    if(status != 0)
    {
        ret = -RED_EIO;
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
    (void)bVolNum;
    return 0;
}


/** @brief Return the disk geometry.

    @param bVolNum  The volume number of the volume whose block device geometry
                    is being queried.
    @param pInfo    On successful return, populated with the geometry of the
                    block device.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O or driver error occurred.
*/
static REDSTATUS DiskGetGeometry(
    uint8_t     bVolNum,
    BDEVINFO   *pInfo)
{
    REDSTATUS   ret;
    WORD        wSectorSize;
    DWORD       dwSectorCount;
    DRESULT     result;

    result = disk_ioctl(bVolNum, GET_SECTOR_SIZE, &wSectorSize);
    if(result == RES_OK)
    {
        result = disk_ioctl(bVolNum, GET_SECTOR_COUNT, &dwSectorCount);
        if(result == RES_OK)
        {
            pInfo->ulSectorSize = wSectorSize;
            pInfo->ullSectorCount = dwSectorCount;
        }
        else
        {
            ret = -RED_EIO;
        }
    }
    else
    {
        ret = -RED_EIO;
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
    uint32_t    ulSectorIdx = 0U;
    uint32_t    ulSectorSize = gaRedBdevInfo[bVolNum].ulSectorSize;
    uint8_t    *pbBuffer = CAST_VOID_PTR_TO_UINT8_PTR(pBuffer);

    while(ulSectorIdx < ulSectorCount)
    {
        uint32_t    ulTransfer = REDMIN(ulSectorCount - ulSectorIdx, MAX_SECTOR_TRANSFER);
        DRESULT     result;

        result = disk_read(bVolNum, &pbBuffer[ulSectorIdx * ulSectorSize], (DWORD)(ullSectorStart + ulSectorIdx), (BYTE)ulTransfer);
        if(result != RES_OK)
        {
            ret = -RED_EIO;
            break;
        }

        ulSectorIdx += ulTransfer;
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
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS DiskWrite(
    uint8_t         bVolNum,
    uint64_t        ullSectorStart,
    uint32_t        ulSectorCount,
    const void     *pBuffer)
{
    REDSTATUS       ret = 0;
    uint32_t        ulSectorIdx = 0U;
    uint32_t        ulSectorSize = gaRedBdevInfo[bVolNum].ulSectorSize;
    const uint8_t  *pbBuffer = CAST_VOID_PTR_TO_CONST_UINT8_PTR(pBuffer);

    while(ulSectorIdx < ulSectorCount)
    {
        uint32_t    ulTransfer = REDMIN(ulSectorCount - ulSectorIdx, MAX_SECTOR_TRANSFER);
        DRESULT     result;

        result = disk_write(bVolNum, &pbBuffer[ulSectorIdx * ulSectorSize], (DWORD)(ullSectorStart + ulSectorIdx), (BYTE)ulTransfer);
        if(result != RES_OK)
        {
            ret = -RED_EIO;
            break;
        }

        ulSectorIdx += ulTransfer;
    }

    return ret;
}


/** @brief Flush any caches beneath the file system.

    @param bVolNum  The volume number of the volume whose block device is being
                    flushed.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS DiskFlush(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;
    DRESULT     result;

    result = disk_ioctl(bVolNum, CTRL_SYNC, NULL);
    if(result == RES_OK)
    {
        ret = 0;
    }
    else
    {
        ret = -RED_EIO;
    }

    return ret;
}

#endif /* REDCONF_READ_ONLY == 0 */


#endif /* OSBDEV_FATFS_H */

