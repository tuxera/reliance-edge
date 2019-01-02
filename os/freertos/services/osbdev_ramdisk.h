/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                   Copyright (c) 2014-2019 Datalight, Inc.
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
#ifndef OSBDEV_RAMDISK_H
#define OSBDEV_RAMDISK_H


#include <stdlib.h> /* For ALLOCATE_CLEARED_MEMORY(), which expands to calloc(). */


static uint8_t *gapbRamDisk[REDCONF_VOLUME_COUNT];


/** @brief Initialize a disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    initialized.
    @param mode     The open mode, indicating the type of access required.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL Invalid sector geometry for a RAM disk.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS DiskOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    REDSTATUS       ret = 0;

    (void)mode;

    if(gaRedVolConf[bVolNum].ullSectorOffset > 0U)
    {
        /*  A sector offset makes no sense for a RAM disk.  The feature exists
            to enable partitioning, but we don't support having more than one
            file system on a RAM disk.  Thus, having a sector offset would only
            waste memory by making the RAM disk bigger.
        */
        ret = -RED_EINVAL;
    }
    else if(gapbRamDisk[bVolNum] == NULL)
    {
        gapbRamDisk[bVolNum] = ALLOCATE_CLEARED_MEMORY(gaRedVolume[bVolNum].ulBlockCount, REDCONF_BLOCK_SIZE);
        if(gapbRamDisk[bVolNum] == NULL)
        {
            ret = -RED_EIO;
        }
    }
    else
    {
        /*  RAM disk already exists, nothing to do.
        */
    }

    return ret;
}


/** @brief Uninitialize a disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    uninitialized.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL RAM disk has not been created.
*/
static REDSTATUS DiskClose(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;

    if(gapbRamDisk[bVolNum] == NULL)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        /*  This implementation uses dynamically allocated memory, but must
            retain previously written data after the block device is closed, and
            thus the memory cannot be freed and will remain allocated until
            reboot.
        */
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
    @retval -RED_EINVAL RAM disk has not been created.
*/
static REDSTATUS DiskRead(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    void       *pBuffer)
{
    REDSTATUS   ret;

    if(gapbRamDisk[bVolNum] == NULL)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        uint64_t ullByteOffset = ullSectorStart * gaRedVolConf[bVolNum].ulSectorSize;
        uint32_t ulByteCount = ulSectorCount * gaRedVolConf[bVolNum].ulSectorSize;

        RedMemCpy(pBuffer, &gapbRamDisk[bVolNum][ullByteOffset], ulByteCount);

        ret = 0;
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
    @retval -RED_EINVAL RAM disk has not been created.
*/
static REDSTATUS DiskWrite(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    const void *pBuffer)
{
    REDSTATUS   ret;

    if(gapbRamDisk[bVolNum] == NULL)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        uint64_t ullByteOffset = ullSectorStart * gaRedVolConf[bVolNum].ulSectorSize;
        uint32_t ulByteCount = ulSectorCount * gaRedVolConf[bVolNum].ulSectorSize;

        RedMemCpy(&gapbRamDisk[bVolNum][ullByteOffset], pBuffer, ulByteCount);

        ret = 0;
    }

    return ret;
}


/** @brief Flush any caches beneath the file system.

    @param bVolNum  The volume number of the volume whose block device is being
                    flushed.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL RAM disk has not been created.
*/
static REDSTATUS DiskFlush(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;

    if(gapbRamDisk[bVolNum] == NULL)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        ret = 0;
    }

    return ret;
}

#endif /* REDCONF_READ_ONLY == 0 */


#endif /* OSBDEV_RAMDISK_H */

