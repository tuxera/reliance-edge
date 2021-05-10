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
/** @file
    @brief Implements block device I/O.
*/

/*  Enable stat64 and make off_t 64 bits.
*/
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <features.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <strings.h>
#include <inttypes.h>

#include <redfs.h>
#include <redvolume.h>
#include <redbdev.h>


typedef enum
{
    BDEVTYPE_RAM_DISK = 0,  /* Default: must be zero. */
    BDEVTYPE_FILE_DISK = 1
} BDEVTYPE;

typedef struct
{
    bool            fOpen;      /* The block device is open. */
    BDEVOPENMODE    mode;       /* Acess mode. */
    BDEVTYPE        type;       /* Disk type: ram disk or file disk. */
    uint8_t        *pbRamDisk;  /* Buffer for RAM disks. */
    const char     *pszSpec;    /* Path for file and raw disks. */
    int             fd;         /* File descriptor for file disks. */
    bool            fIsBDev;    /* Whether file disk is a block device. */
} LINUXBDEV;


static REDSTATUS RamDiskOpen(uint8_t bVolNum, BDEVOPENMODE mode);
static REDSTATUS RamDiskClose(uint8_t bVolNum);
static REDSTATUS RamDiskGetGeometry(uint8_t bVolNum, BDEVINFO *pInfo);
static REDSTATUS RamDiskRead(uint8_t bVolNum, uint64_t ullSectorStart, uint32_t ulSectorCount, void *pBuffer);
#if REDCONF_READ_ONLY == 0
static REDSTATUS RamDiskWrite(uint8_t bVolNum, uint64_t ullSectorStart, uint32_t ulSectorCount, const void *pBuffer);
static REDSTATUS RamDiskFlush(uint8_t bVolNum);
#endif
static REDSTATUS FileDiskOpen(uint8_t bVolNum, BDEVOPENMODE mode);
static REDSTATUS FileDiskClose(uint8_t bVolNum);
static REDSTATUS FileDiskGetGeometry(uint8_t bVolNum, BDEVINFO *pInfo);
static REDSTATUS FileDiskRead(uint8_t bVolNum, uint64_t ullSectorStart, uint32_t ulSectorCount, void *pBuffer);
#if REDCONF_READ_ONLY == 0
static REDSTATUS FileDiskWrite(uint8_t bVolNum, uint64_t ullSectorStart, uint32_t ulSectorCount, const void *pBuffer);
static REDSTATUS FileDiskFlush(uint8_t bVolNum);
#endif


static LINUXBDEV gaDisk[REDCONF_VOLUME_COUNT];


/** @brief Configure a block device.

    @note   This is a non-standard block device API!  The standard block device
            APIs are designed for implementations running on targets with block
            devices that are known in advance and can be statically defined by
            the implementation.  However, this implementation is intended for
            host systems, and it needs to support writing to raw disks (like
            "H:" etc.) and file disks which are supplied on the command line.

    @param bVolNum      The volume number of the volume to configure.
    @param pszBDevSpec  Drive or file to associate with the volume.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is not a valid volume number; or
                        @p pszBDevSpec is `NULL` or an empty string.
*/
REDSTATUS RedOsBDevConfig(
    uint8_t     bVolNum,
    const char *pszBDevSpec)
{
    REDSTATUS   ret;

    if((bVolNum >= REDCONF_VOLUME_COUNT) || gaDisk[bVolNum].fOpen || (pszBDevSpec == NULL) || (pszBDevSpec[0U] == '\0'))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        RedMemSet(&gaDisk[bVolNum], 0U, sizeof(gaDisk[bVolNum]));

        gaDisk[bVolNum].pszSpec = pszBDevSpec;

        if(strcasecmp(pszBDevSpec, "ram") == 0)
        {
            gaDisk[bVolNum].type = BDEVTYPE_RAM_DISK;
        }
        else
        {
            gaDisk[bVolNum].type = BDEVTYPE_FILE_DISK;
        }

        ret = 0;
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
    @retval -RED_EINVAL @p bVolNum is an invalid volume number; or invalid
                        sector geometry.
    @retval -RED_EIO    A disk I/O error occurred.
*/
REDSTATUS RedOsBDevOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    REDSTATUS       ret;

    if((bVolNum >= REDCONF_VOLUME_COUNT) || gaDisk[bVolNum].fOpen)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        switch(gaDisk[bVolNum].type)
        {
            case BDEVTYPE_RAM_DISK:
                ret = RamDiskOpen(bVolNum, mode);
                break;

            case BDEVTYPE_FILE_DISK:
                ret = FileDiskOpen(bVolNum, mode);
                break;

            default:
                REDERROR();
                ret = -RED_EINVAL;
                break;
        }

        if(ret == 0)
        {
            gaDisk[bVolNum].fOpen = true;
            gaDisk[bVolNum].mode = mode;
        }
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

    if((bVolNum >= REDCONF_VOLUME_COUNT) || !gaDisk[bVolNum].fOpen)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        switch(gaDisk[bVolNum].type)
        {
            case BDEVTYPE_RAM_DISK:
                ret = RamDiskClose(bVolNum);
                break;

            case BDEVTYPE_FILE_DISK:
                ret = FileDiskClose(bVolNum);
                break;

            default:
                REDERROR();
                ret = -RED_EINVAL;
                break;
        }

        if(ret == 0)
        {
            gaDisk[bVolNum].fOpen = false;
        }
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
    @retval -RED_EINVAL     @p bVolNum is an invalid volume number; or @p pInfo
                            is `NULL`; or invalid sector geometry.
    @retval -RED_EIO        A disk I/O error occurred.
    @retval -RED_ENOTSUPP   The geometry cannot be queried on this block device.
*/
REDSTATUS RedOsBDevGetGeometry(
    uint8_t     bVolNum,
    BDEVINFO   *pInfo)
{
    REDSTATUS   ret;

    if((bVolNum >= REDCONF_VOLUME_COUNT) || (pInfo == NULL) || !gaDisk[bVolNum].fOpen)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        switch(gaDisk[bVolNum].type)
        {
            case BDEVTYPE_RAM_DISK:
                ret = RamDiskGetGeometry(bVolNum, pInfo);
                break;

            case BDEVTYPE_FILE_DISK:
                ret = FileDiskGetGeometry(bVolNum, pInfo);
                break;

            default:
                REDERROR();
                ret = -RED_EINVAL;
                break;
        }

        if((ret == 0) && (pInfo->ullSectorCount > (UINT64_MAX / pInfo->ulSectorSize)))
        {
            /*  Both the RAM disk and the file disk will access the sectors by
                byte address.  If the Sector*SectorSize multiplication will
                result in unsigned integer wraparound, detect it here rather
                than accessing the wrong sector.
            */
            fprintf(stderr, "Error: sector geometry for volume %d too large to be byte-addressable\n", (int)bVolNum);
            ret = -RED_EINVAL;
        }
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
    REDSTATUS   ret;

    if(    (bVolNum >= REDCONF_VOLUME_COUNT)
        || !gaDisk[bVolNum].fOpen
        || (gaDisk[bVolNum].mode == BDEV_O_WRONLY)
        || !VOLUME_SECTOR_RANGE_IS_VALID(bVolNum, ullSectorStart, ulSectorCount)
        || (pBuffer == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        switch(gaDisk[bVolNum].type)
        {
            case BDEVTYPE_RAM_DISK:
                ret = RamDiskRead(bVolNum, ullSectorStart, ulSectorCount, pBuffer);
                break;

            case BDEVTYPE_FILE_DISK:
                ret = FileDiskRead(bVolNum, ullSectorStart, ulSectorCount, pBuffer);
                break;

            default:
                REDERROR();
                ret = -RED_EINVAL;
                break;
        }
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
    REDSTATUS   ret;

    if(    (bVolNum >= REDCONF_VOLUME_COUNT)
        || !gaDisk[bVolNum].fOpen
        || (gaDisk[bVolNum].mode == BDEV_O_RDONLY)
        || !VOLUME_SECTOR_RANGE_IS_VALID(bVolNum, ullSectorStart, ulSectorCount)
        || (pBuffer == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        switch(gaDisk[bVolNum].type)
        {
            case BDEVTYPE_RAM_DISK:
                ret = RamDiskWrite(bVolNum, ullSectorStart, ulSectorCount, pBuffer);
                break;

            case BDEVTYPE_FILE_DISK:
                ret = FileDiskWrite(bVolNum, ullSectorStart, ulSectorCount, pBuffer);
                break;

            default:
                REDERROR();
                ret = -RED_EINVAL;
                break;
        }
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

    if((bVolNum >= REDCONF_VOLUME_COUNT) || !gaDisk[bVolNum].fOpen || (gaDisk[bVolNum].mode == BDEV_O_RDONLY))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        switch(gaDisk[bVolNum].type)
        {
            case BDEVTYPE_RAM_DISK:
                ret = RamDiskFlush(bVolNum);
                break;

            case BDEVTYPE_FILE_DISK:
                ret = FileDiskFlush(bVolNum);
                break;

            default:
                REDERROR();
                ret = -RED_EINVAL;
                break;
        }
    }

    return ret;
}
#endif /* REDCONF_READ_ONLY == 0 */


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
    @retval -RED_EINVAL Invalid sector geometry for a RAM disk.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS RamDiskOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    REDSTATUS       ret = 0;

    (void)mode;

    if(    (gaRedVolConf[bVolNum].ulSectorSize == SECTOR_SIZE_AUTO)
        || (gaRedVolConf[bVolNum].ullSectorCount == SECTOR_COUNT_AUTO))
    {
        /*  Automatic detection of sector size and sector count are not
            supported by the RAM disk.
        */
        ret = -RED_ENOTSUPP;
    }
    else if(gaRedVolConf[bVolNum].ullSectorOffset > 0U)
    {
        /*  A sector offset makes no sense for a RAM disk.  The feature exists
            to enable partitioning, but we don't support having more than one
            file system on a RAM disk.  Thus, having a sector offset would only
            waste memory by making the RAM disk bigger.
        */
        ret = -RED_EINVAL;
    }
    else if(gaDisk[bVolNum].pbRamDisk == NULL)
    {
        /*  Make sure the sector count fits into a size_t, for the calloc()
            parameter.
        */
        if(gaRedVolConf[bVolNum].ullSectorCount > SIZE_MAX)
        {
            ret = -RED_EINVAL;
        }
        else
        {
            gaDisk[bVolNum].pbRamDisk = calloc((size_t)gaRedVolConf[bVolNum].ullSectorCount, gaRedVolConf[bVolNum].ulSectorSize);
            if(gaDisk[bVolNum].pbRamDisk == NULL)
            {
                ret = -RED_EIO;
            }
        }
    }
    else
    {
        /*  RAM disk already exists, nothing to do.
        */
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

    @retval 0   Operation was successful.
*/
static REDSTATUS RamDiskClose(
    uint8_t bVolNum)
{
    /*  This implementation uses dynamically allocated memory, but must retain
        previously written data after the block device is closed, and thus the
        memory cannot be freed and will remain allocated until the program
        exits.
    */
    (void)bVolNum;

    return 0;
}


/** @brief Return the block device geometry.

    Not supported on RAM disks.  Geometry must be specified in redconf.c.

    @param bVolNum          The volume number of the volume whose block device
                            geometry is being queried.
    @param pulSectorSize    The starting sector number.
    @param pullSectorCount  The number of sectors to read.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval -RED_ENOTSUPP   The geometry cannot be queried on this block device.
*/
static REDSTATUS RamDiskGetGeometry(
    uint8_t     bVolNum,
    BDEVINFO   *pInfo)
{
    (void)bVolNum;
    (void)pInfo;

    /*  The RAM disk requires the geometry to be specified in the volume
        configuration at compile-time; it cannot be detected at run-time.
    */
    return -RED_ENOTSUPP;
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

    @retval 0   Operation was successful.
*/
static REDSTATUS RamDiskRead(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    void       *pBuffer)
{
    uint64_t ullByteOffset = ullSectorStart * gaRedVolConf[bVolNum].ulSectorSize;
    uint32_t ulByteCount   = ulSectorCount  * gaRedVolConf[bVolNum].ulSectorSize;

    memcpy(pBuffer, &gaDisk[bVolNum].pbRamDisk[ullByteOffset], ulByteCount);

    return 0;
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

    @retval 0   Operation was successful.
*/
static REDSTATUS RamDiskWrite(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    const void *pBuffer)
{
    uint64_t ullByteOffset = ullSectorStart * gaRedVolConf[bVolNum].ulSectorSize;
    uint32_t ulByteCount   = ulSectorCount  * gaRedVolConf[bVolNum].ulSectorSize;

    memcpy(&gaDisk[bVolNum].pbRamDisk[ullByteOffset], pBuffer, ulByteCount);

    return 0;
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

    @retval 0   Operation was successful.
*/
static REDSTATUS RamDiskFlush(
    uint8_t bVolNum)
{
    (void)bVolNum;

    return 0;
}
#endif /* REDCONF_READ_ONLY == 0 */


/** @brief Initialize a file disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    initialized.
    @param mode     The open mode, indicating the type of access required.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
    @retval -RED_EINVAL File disk not compatible with sector geometry.
*/
static REDSTATUS FileDiskOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    LINUXBDEV      *pDisk = &gaDisk[bVolNum];
    REDSTATUS       ret = 0;
    struct stat64   stat;

    if(stat64(pDisk->pszSpec, &stat) != 0)
    {
        /*  If file doesn't exist, we'll create it.  If file isn't accessible,
            error out.
        */
        if(errno != ENOENT)
        {
            perror("Error getting block device file stats");
            ret = -RED_EIO;
        }
    }

    if(ret == 0)
    {
        int fileFlags = O_RDONLY;

      #if REDCONF_READ_ONLY == 0
        switch(mode)
        {
            case BDEV_O_RDWR:
                fileFlags = O_RDWR;
                break;

            case BDEV_O_WRONLY:
                fileFlags = O_WRONLY;
                break;

            default:
                break;
        }
      #endif

        if(gaRedVolConf[bVolNum].ullSectorCount != SECTOR_COUNT_AUTO)
        {
            fileFlags |= O_CREAT;
        }

        pDisk->fd = open(pDisk->pszSpec, fileFlags, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

        if(pDisk->fd == -1)
        {
            perror("Error opening file as block device");
            ret = -RED_EIO;
        }

        pDisk->fIsBDev = S_ISBLK(stat.st_mode);
    }

    return ret;
}


/** @brief Uninitialize a file disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    uninitialized.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS FileDiskClose(uint8_t bVolNum)
{
    LINUXBDEV  *pDisk = &gaDisk[bVolNum];
    REDSTATUS   ret = 0;

  #if REDCONF_READ_ONLY == 0
    /*  Flush before closing.  This is primarily for the tools, so that all the
        data is really committed to the media when the tool exits.
    */
    if(gaDisk[bVolNum].mode != BDEV_O_RDONLY)
    {
        ret = FileDiskFlush(bVolNum);
    }
  #endif

    if(ret == 0)
    {
        if(close(pDisk->fd) == -1)
        {
            ret = -RED_EIO;
        }

        pDisk->fd = -1;
    }

    return ret;
}


/** @brief Return the block device geometry.

    Supported only on existing file disks.  Sector size must be spepcified in
    the volume config.

    @param bVolNum          The volume number of the volume whose block device
                            geometry is being queried.
    @param pulSectorSize    The starting sector number.
    @param pullSectorCount  The number of sectors to read.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS FileDiskGetGeometry(
    uint8_t         bVolNum,
    BDEVINFO       *pInfo)
{
    LINUXBDEV      *pDisk = &gaDisk[bVolNum];
    REDSTATUS       ret = 0;
    struct stat64   stat;

    if(stat64(pDisk->pszSpec, &stat) != 0)
    {
        perror("Error getting block device file stats");
        ret = -RED_EIO;
    }
    else if(pDisk->fIsBDev)
    {
        uint64_t    ullDevSize;
        int         iSectorSize;

        if(ioctl(pDisk->fd, BLKGETSIZE64, &ullDevSize) == -1)
        {
            perror("Error getting block device size");
            ret = -RED_EIO;
        }
        else if(ioctl(pDisk->fd, BLKSSZGET, &iSectorSize) == -1)
        {
            perror("Error getting block device sector size");
            ret = -RED_EIO;
        }
        else
        {
            pInfo->ulSectorSize = (uint32_t)iSectorSize;
            pInfo->ullSectorCount = ullDevSize / iSectorSize;
        }
    }
    else
    {
        if(gaRedVolConf[bVolNum].ulSectorSize == SECTOR_SIZE_AUTO)
        {
            /*  If the sector size isn't specified, any valid value will do.
                Thus, use 512 bytes (the most common value) or the block size,
                whichever is less.
            */
            pInfo->ulSectorSize = REDMIN(512U, REDCONF_BLOCK_SIZE);
        }
        else
        {
            pInfo->ulSectorSize = gaRedVolConf[bVolNum].ulSectorSize;
        }

        if(gaRedVolConf[bVolNum].ullSectorCount == SECTOR_COUNT_AUTO)
        {
            pInfo->ullSectorCount = stat.st_size / pInfo->ulSectorSize;
        }
        else
        {
            pInfo->ullSectorCount = gaRedVolConf[bVolNum].ullSectorOffset + gaRedVolConf[bVolNum].ullSectorCount;
        }
    }

    return ret;
}


/** @brief Read sectors from a file disk.

    @param bVolNum          The volume number of the volume whose block device
                            is being read from.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to read.
    @param pBuffer          The buffer into which to read the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL Read length too long.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS FileDiskRead(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    void       *pBuffer)
{
    LINUXBDEV  *pDisk = &gaDisk[bVolNum];
    uint32_t    ulVolSecSize = gaRedBdevInfo[bVolNum].ulSectorSize;
    REDSTATUS   ret = 0;

    if(((uint64_t)ulVolSecSize * ulSectorCount) > (uint64_t)SSIZE_MAX)
    {
        /*  It is assumed that a user would never allocate a buffer larger than
            2GiB and ask us to read into it, so report an error if it happens.
        */
        ret = -RED_EINVAL;
    }
    else if(lseek(pDisk->fd, (off_t)(ullSectorStart * ulVolSecSize), SEEK_SET) == -1)
    {
        ret = -RED_EIO;
    }
    else
    {
        size_t readlen = (size_t)(ulVolSecSize * ulSectorCount);
        ssize_t result;

        result = read(pDisk->fd, pBuffer, readlen);

        if(result != (ssize_t)readlen)
        {
            ret = -RED_EIO;
        }
    }

    return ret;
}


#if REDCONF_READ_ONLY == 0
/** @brief Write sectors to a file disk.

    @param bVolNum          The volume number of the volume whose block device
                            is being written to.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to write.
    @param pBuffer          The buffer from which to write the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL Write length too long.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS FileDiskWrite(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    const void *pBuffer)
{
    LINUXBDEV  *pDisk = &gaDisk[bVolNum];
    uint32_t    ulVolSecSize = gaRedBdevInfo[bVolNum].ulSectorSize;
    REDSTATUS   ret = 0;

    if(((uint64_t)ulVolSecSize * ulSectorCount) > (uint64_t)SSIZE_MAX)
    {
        /*  It is assumed that a user would never allocate a buffer larger than
            2GiB and ask us to read into it, so report an error if it happens.
        */
        ret = -RED_EINVAL;
    }
    else if(lseek(pDisk->fd, (off_t)(ullSectorStart * ulVolSecSize), SEEK_SET) == -1)
    {
        ret = -RED_EIO;
    }
    else
    {
        size_t writelen = (size_t)(ulVolSecSize * ulSectorCount);
        ssize_t result;

        result = write(pDisk->fd, pBuffer, writelen);

        if(result != (ssize_t)writelen)
        {
            ret = -RED_EIO;
        }
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
static REDSTATUS FileDiskFlush(
    uint8_t     bVolNum)
{
    LINUXBDEV  *pDisk = &gaDisk[bVolNum];
    REDSTATUS   ret = 0;

    /*  Flush block devices, but not normal file disks.  In theory, we could
        flush the file disk, but there isn't a strong need.  File disks are used
        for two things: the image builder and tests.  The host Linux system is
        not expected to crash, and if it does, the image builder or tests will
        be starting over anyway.

        The downside to flushing is that when testing a file disk, it makes the
        tests much slower since it generates lots of disk I/O on the host hard
        drive.
    */
    if(pDisk->fIsBDev)
    {
        if(fsync(pDisk->fd) != 0)
        {
            ret = -RED_EIO;
        }
    }

    return ret;
}
#endif /* REDCONF_READ_ONLY == 0 */

