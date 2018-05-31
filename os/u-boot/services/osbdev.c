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
/** @file
    @brief Implements block device I/O.
*/
#include <common.h>
#include <blk.h>
#include <redfs_uboot.h>


#include <redfs.h>
#include <redvolume.h>
#include <redosdeviations.h>
#include <redutils.h>
#include <redosbdev.h>


/*  This array holds the block device handles and partition information for
    the various redfs volumes.
*/
typedef struct {
    struct blk_desc * block_dev;
    disk_partition_t * fs_partition;
} UBOOT_DEV;
static UBOOT_DEV gaDisk[REDCONF_VOLUME_COUNT];


/** @brief Configure a block device.

    @note   This is a non-standard block device API!  The standard block device
            APIs are designed for implementations running on targets with block
            devices that are known in advance and can be statically defined by
            the implementation.  However, that model does not work well for
            u-boot, so we implement this API to allow the name of the block
            device to be specified at run-time.

    @param bVolNum      The volume number of the volume to configure.
    @param pszBDevSpec  Drive or file to associate with the volume.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is not a valid volume number; or
                        @p pszBDevSpec is `NULL` or an empty string.
*/
REDSTATUS RedOsBDevConfig2(
    uint8_t bVolNum,
    struct blk_desc * block_dev,
    disk_partition_t * fs_partition)
{
    REDSTATUS ret = 0;


    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        ret = RED_EINVAL;
    }
    else
    {
        gaDisk[bVolNum].block_dev = block_dev;
        gaDisk[bVolNum].fs_partition = fs_partition;
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
    @retval -RED_EINVAL @p bVolNum is an invalid volume number; or the block
                        device is already open.
    @retval -RED_EIO    A disk I/O error occurred.
*/
REDSTATUS RedOsBDevOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    return 0;
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
    @retval -RED_EINVAL @p bVolNum is an invalid volume number; or the block
                        device is already open.
*/
REDSTATUS RedOsBDevClose(
    uint8_t     bVolNum)
{
    return 0;
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
    ulong count;
    REDSTATUS ret = 0;


    count = blk_dread(gaDisk[bVolNum].block_dev, ullSectorStart, ulSectorCount, pBuffer);
    if(count != ulSectorCount)
    {
        ret = -RED_EIO;
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
    return -RED_EINVAL;
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
    return -RED_EINVAL;
}


#if REDCONF_DISCARDS == 1
/** @brief Discard (trim) sectors on a physical block device.

    This function alerts the block device that the given sectors no longer
    contain information that is important to the filesystem.

    The behavior of calling this function is undefined if the block device is
    closed or if it was opened with ::BDEV_O_RDONLY.

    If discarding fails, the integrity of the volume should not be affected
    and there is nothing that needs to be done to recover or report the error.
    So no errors are returned from this function.

    @param bVolNum          The volume number of the volume whose block device
                            is being accessed.
    @param ullSectorStart   The starting sector number.
    @param ullSectorCount   The number of sectors to discard.
*/
void RedOsBDevDiscard(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint64_t    ullSectorCount)
{
}
#endif /* REDCONF_DISCARDS == 1 */
#endif /* REDCONF_READ_ONLY == 0 */


