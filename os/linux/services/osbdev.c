/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                   Copyright (c) 2014-2015 Datalight, Inc.
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
#include <redfs.h>
#include <redvolume.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

typedef enum
{
    BDEVTYPE_RAM_DISK = 0,  /* Default: must be zero. */
    BDEVTYPE_FILE_DISK = 1,
    BDEVTYPE_RAW_DISK = 2
} BDEVTYPE;

typedef struct
{
    bool            fOpen;      /* The block device is open. */
    BDEVOPENMODE    mode;       /* Acess mode. */
    BDEVTYPE        type;       /* Disk type: ram disk, file disk, raw disk. */
    uint8_t        *pbRamDisk;  /* Buffer for RAM disks. */
    const char     *pszSpec;    /* Path for file and raw disks. */
    int             hDevice;    /* Handle for file and raw disks. */
} LINBDEV;

static bool IsDriveSpec(const char *pszPathSpec);
static REDSTATUS RamDiskOpen(uint8_t bVolNum, BDEVOPENMODE mode);
static REDSTATUS RamDiskClose(uint8_t bVolNum);
static REDSTATUS RamDiskRead(uint8_t bVolNum, uint64_t ullSectorStart,
                             uint32_t ulSectorCount, void *pBuffer);
#if REDCONF_READ_ONLY == 0
static REDSTATUS RamDiskWrite(uint8_t bVolNum, uint64_t ullSectorStart,
                              uint32_t ulSectorCount, const void *pBuffer);
static REDSTATUS RamDiskFlush(uint8_t bVolNum);
#endif
static REDSTATUS FileDiskOpen(uint8_t bVolNum, BDEVOPENMODE mode);
static REDSTATUS FileDiskClose(uint8_t bVolNum);
static REDSTATUS FileDiskRead(uint8_t bVolNum, uint64_t ullSectorStart,
                              uint32_t ulSectorCount, void *pBuffer);
#if REDCONF_READ_ONLY == 0
static REDSTATUS FileDiskWrite(uint8_t bVolNum, uint64_t ullSectorStart,
                               uint32_t ulSectorCount, const void *pBuffer);
static REDSTATUS FileDiskFlush(uint8_t bVolNum);
#endif
static REDSTATUS RawDiskOpen(uint8_t bVolNum, BDEVOPENMODE mode);
static REDSTATUS RawDiskClose(uint8_t bVolNum);
static REDSTATUS RawDiskRead(uint8_t bVolNum, uint64_t ullSectorStart,
                             uint32_t ulSectorCount, void *pBuffer);
#if REDCONF_READ_ONLY == 0
static REDSTATUS RawDiskWrite(uint8_t bVolNum, uint64_t ullSectorStart,
                              uint32_t ulSectorCount, const void *pBuffer);
static REDSTATUS RawDiskFlush(uint8_t bVolNum);
#endif

static LINBDEV gaDisk[REDCONF_VOLUME_COUNT];

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
    REDSTATUS   ret = 0;

    if(bVolNum >= REDCONF_VOLUME_COUNT)
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
        else if(IsDriveSpec(pszBDevSpec))
        {
            gaDisk[bVolNum].type = BDEVTYPE_RAW_DISK;
        }
        else
        {
            gaDisk[bVolNum].type = BDEVTYPE_FILE_DISK;
        }

        ret = 0;
    }

    return ret;
}

/** Determine whether a path names a drive or disk device.

    Drive paths are expected to use the Win32 device namespace; "C:" by itself
    would not be recognized as a drive, but "\\.\C:" would.

    @param pszPathSpec  The path to examine.

    @return Whether @p pszPathSpec appears to name a drive or disk device.
*/
static bool IsDriveSpec(
    const char *pszPathSpec)
{
    struct stat fileStat;

    if (stat(pszPathSpec, &fileStat) == 0) {
        if (fileStat.st_mode & S_IFBLK) {
            return true;
        }
    }

    return false;
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
    REDSTATUS       ret = 0;

    if(bVolNum >= REDCONF_VOLUME_COUNT)
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

            case BDEVTYPE_RAW_DISK:
                ret = RawDiskOpen(bVolNum, mode);
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

    @retval 0       Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number.
*/
REDSTATUS RedOsBDevClose(
    uint8_t     bVolNum)
{
    REDSTATUS   ret = 0;

    if(bVolNum >= REDCONF_VOLUME_COUNT)
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

            case BDEVTYPE_RAW_DISK:
                ret = RawDiskClose(bVolNum);
                break;

            default:
                REDERROR();
                ret = -RED_EINVAL;
                break;
        }

        if(ret == 0)
        {
            gaDisk[bVolNum].fOpen = false;
            gaDisk[bVolNum].mode = BDEV_O_RDONLY;
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
    REDSTATUS   ret = 0;

    if(    (bVolNum >= REDCONF_VOLUME_COUNT)
        || !gaDisk[bVolNum].fOpen
        || (gaDisk[bVolNum].mode == BDEV_O_WRONLY)
        || (ullSectorStart >= gaRedVolConf[bVolNum].ullSectorCount)
        || ((gaRedVolConf[bVolNum].ullSectorCount - ullSectorStart) < ulSectorCount)
        || (pBuffer == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        switch(gaDisk[bVolNum].type)
        {
            case BDEVTYPE_RAM_DISK:
                ret = RamDiskRead(bVolNum, ullSectorStart, ulSectorCount,
                                  pBuffer);
                break;

            case BDEVTYPE_FILE_DISK:
                ret = FileDiskRead(bVolNum, ullSectorStart, ulSectorCount,
                                   pBuffer);
                break;

            case BDEVTYPE_RAW_DISK:
                ret = RawDiskRead(bVolNum, ullSectorStart, ulSectorCount,
                                  pBuffer);
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
    REDSTATUS   ret = 0;

    if(    (bVolNum >= REDCONF_VOLUME_COUNT)
        || !gaDisk[bVolNum].fOpen
        || (gaDisk[bVolNum].mode == BDEV_O_RDONLY)
        || (ullSectorStart >= gaRedVolConf[bVolNum].ullSectorCount)
        || ((gaRedVolConf[bVolNum].ullSectorCount - ullSectorStart) < ulSectorCount)
        || (pBuffer == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        switch(gaDisk[bVolNum].type)
        {
            case BDEVTYPE_RAM_DISK:
                ret = RamDiskWrite(bVolNum, ullSectorStart, ulSectorCount,
                                   pBuffer);
                break;

            case BDEVTYPE_FILE_DISK:
                ret = FileDiskWrite(bVolNum, ullSectorStart, ulSectorCount,
                                    pBuffer);
                break;

            case BDEVTYPE_RAW_DISK:
                ret = RawDiskWrite(bVolNum, ullSectorStart, ulSectorCount,
                                   pBuffer);
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
    REDSTATUS   ret = 0;

    if(    (bVolNum >= REDCONF_VOLUME_COUNT)
        || !gaDisk[bVolNum].fOpen
        || (gaDisk[bVolNum].mode == BDEV_O_RDONLY))
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

            case BDEVTYPE_RAW_DISK:
                ret = RawDiskFlush(bVolNum);
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

/** @brief Initialize a RAM disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    initialized.
    @param mode     The open mode, indicating the type of access required.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS RamDiskOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    REDSTATUS       ret = 0;

    if(gaDisk[bVolNum].pbRamDisk == NULL)
    {
        gaDisk[bVolNum].pbRamDisk = calloc(gaRedVolume[bVolNum].ulBlockCount,
                                           REDCONF_BLOCK_SIZE);
        if(gaDisk[bVolNum].pbRamDisk == NULL)
        {
            ret = -RED_EIO;
        }
    }

    return ret;
}

/** @brief Uninitialize a RAM disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    uninitialized.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0   Operation was successful.
*/
static REDSTATUS RamDiskClose(
    uint8_t     bVolNum)
{
    /*  This implementation uses dynamically allocated memory, but must retain
        previously written data after the block device is closed, and thus the
        memory cannot be freed and will remain allocated until the program
        exits.
    */

    return 0;
}

/** @brief Read sectors from a RAM disk.

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
    uint64_t    ullByteOffset = ullSectorStart
                                * gaRedVolConf[bVolNum].ulSectorSize;
    uint32_t    ulByteCount = ulSectorCount
                              * gaRedVolConf[bVolNum].ulSectorSize;

    memcpy(pBuffer, &gaDisk[bVolNum].pbRamDisk[ullByteOffset], ulByteCount);

    return 0;
}

#if REDCONF_READ_ONLY == 0
/** @brief Write sectors to a RAM disk.

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
    uint64_t    ullByteOffset = ullSectorStart
                                * gaRedVolConf[bVolNum].ulSectorSize;
    uint32_t    ulByteCount = ulSectorCount
                              * gaRedVolConf[bVolNum].ulSectorSize;

    memcpy(&gaDisk[bVolNum].pbRamDisk[ullByteOffset], pBuffer, ulByteCount);

    return 0;
}

/** @brief Flush any caches beneath the file system.

    @param bVolNum  The volume number of the volume whose block device is being
                    flushed.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0   Operation was successful.
*/
static REDSTATUS RamDiskFlush(
    uint8_t     bVolNum)
{
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
    @retval -RED_EROFS  The file disk is a preexisting read-only file and write
                        access was requested.
*/
static REDSTATUS FileDiskOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    LINBDEV        *pDisk = &gaDisk[bVolNum];
    REDSTATUS       ret = 0;
    int             fileFlags = O_RDONLY;

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

    fileFlags |= O_CREAT;

    pDisk->hDevice = open(pDisk->pszSpec, fileFlags, S_IRUSR | S_IWUSR);

    if(pDisk->hDevice == -1)
    {
        perror("FileDiskOpen");
        ret = -RED_EIO;
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
static REDSTATUS FileDiskClose(
    uint8_t     bVolNum)
{
    REDSTATUS   ret = 0;

    /*  Flush before closing.  This is primarily for the tools, so that all the
        data is really committed to the media when the tool exits.
    */
    if(gaDisk[bVolNum].mode != BDEV_O_RDONLY)
    {
        if(fsync(gaDisk[bVolNum].hDevice))
        {
            ret = -RED_EIO;
        }
    }

    if(ret == 0)
    {
        if(close(gaDisk[bVolNum].hDevice))
        {
            ret = -RED_EIO;
        }

        gaDisk[bVolNum].hDevice = -1;
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
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS FileDiskRead(
    uint8_t         bVolNum,
    uint64_t        ullSectorStart,
    uint32_t        ulSectorCount,
    void           *pBuffer)
{
    uint32_t        ulSectorSize = gaRedVolConf[bVolNum].ulSectorSize;
    REDSTATUS       ret = 0;

    if (lseek(gaDisk[bVolNum].hDevice, ullSectorStart * ulSectorSize,
              SEEK_SET) == -1)
    {
        ret = -RED_EIO;
    }

    if (ret == 0) {
        size_t count;

        count = read(gaDisk[bVolNum].hDevice, pBuffer,
                     ulSectorCount * ulSectorSize);

        if (count != ulSectorCount * ulSectorSize)
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
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS FileDiskWrite(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    const void *pBuffer)
{
    uint32_t        ulSectorSize = gaRedVolConf[bVolNum].ulSectorSize;
    REDSTATUS       ret = 0;

    if (lseek(gaDisk[bVolNum].hDevice, ullSectorStart * ulSectorSize,
              SEEK_SET) == -1)
    {
        ret = -RED_EIO;
    }

    if (ret == 0) {
        size_t count;

        count = write(gaDisk[bVolNum].hDevice, pBuffer,
                      ulSectorCount * ulSectorSize);

        if (count != (ulSectorCount * ulSectorSize))
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
    /*  In theory, we could flush the file disk, but there isn't a strong need.
        File disks are used for two things: the image builder and tests.  The
        host Windows system is not expected to crash, and if it does, the image
        builder or tests will be starting over anyway.

        The downside to flushing is that when testing a file disk, it makes the
        tests much slower since it generates lots of disk I/O on the host hard
        drive.
    */
    return 0;
}
#endif /* REDCONF_READ_ONLY == 0 */

/** @brief Initialize a raw disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    initialized.
    @param mode     The open mode, indicating the type of access required.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
    @retval -RED_EBUSY  The device could not be locked.
    @retval -RED_EROFS  The device is read-only media and write access was
                        requested.
*/
static REDSTATUS RawDiskOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    LINBDEV        *pDisk = &gaDisk[bVolNum];
    REDSTATUS       ret = 0;
    int             fileFlags = O_RDONLY;

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

    pDisk->hDevice = open(pDisk->pszSpec, fileFlags);

    if(pDisk->hDevice == -1)
    {
        ret = -RED_EIO;
    }

    return ret;
}

/** @brief Uninitialize a raw disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    uninitialized.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS RawDiskClose(
    uint8_t     bVolNum)
{
    REDSTATUS   ret = 0;

    /*  Flush before closing.  This is primarily for the tools, so that all the
        data is really committed to the media when the tool exits.
    */
    if(gaDisk[bVolNum].mode != BDEV_O_RDONLY)
    {
        if(fsync(gaDisk[bVolNum].hDevice))
        {
            ret = -RED_EIO;
        }
    }

    if(ret == 0)
    {
        if(close(gaDisk[bVolNum].hDevice))
        {
            ret = -RED_EIO;
        }
        else
        {
            gaDisk[bVolNum].hDevice = -1;
        }
    }

    return ret;
}

/** @brief Read sectors from a raw disk.

    @param bVolNum          The volume number of the volume whose block device
                            is being read from.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to read.
    @param pBuffer          The buffer into which to read the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS RawDiskRead(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    void       *pBuffer)
{
    uint32_t        ulSectorSize = gaRedVolConf[bVolNum].ulSectorSize;
    REDSTATUS       ret = 0;

    if (lseek(gaDisk[bVolNum].hDevice, ullSectorStart * ulSectorSize,
              SEEK_SET) == -1)
    {
        ret = -RED_EIO;
    }

    if (ret == 0) {
        size_t count;

        count = read(gaDisk[bVolNum].hDevice, pBuffer,
                     ulSectorCount * ulSectorSize);

        if (count != ulSectorCount * ulSectorSize)
        {
            ret = -RED_EIO;
        }
    }

    return ret;
}

#if REDCONF_READ_ONLY == 0
/** @brief Write sectors to a raw disk.

    @param bVolNum          The volume number of the volume whose block device
                            is being written to.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to write.
    @param pBuffer          The buffer from which to write the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS RawDiskWrite(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    const void *pBuffer)
{
    uint32_t        ulSectorSize = gaRedVolConf[bVolNum].ulSectorSize;
    REDSTATUS       ret = 0;

    if (lseek(gaDisk[bVolNum].hDevice, ullSectorStart * ulSectorSize,
              SEEK_SET) == -1)
    {
        ret = -RED_EIO;
    }

    if (ret == 0) {
        size_t count;

        count = write(gaDisk[bVolNum].hDevice, pBuffer,
                      ulSectorCount * ulSectorSize);

        if (count != (ulSectorCount * ulSectorSize))
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
static REDSTATUS RawDiskFlush(
    uint8_t bVolNum)
{
    REDSTATUS       ret = 0;

    if (fdatasync(gaDisk[bVolNum].hDevice))
    {
        ret = -RED_EIO;
    }

    return ret;
}
#endif /* REDCONF_READ_ONLY == 0 */
