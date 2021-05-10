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
#include <windows.h>

#include <stdlib.h>
#include <string.h>

#include <redfs.h>
#include <redvolume.h>
#include <redbdev.h>


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
    HANDLE          hDevice;    /* Handle for file and raw disks. */
} WINBDEV;


static bool IsDriveSpec(const char *pszPathSpec);
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
static REDSTATUS RawDiskOpen(uint8_t bVolNum, BDEVOPENMODE mode);
static REDSTATUS RawDiskClose(uint8_t bVolNum);
static REDSTATUS RawDiskGetGeometry(uint8_t bVolNum, BDEVINFO *pInfo);
static REDSTATUS RawDiskRead(uint8_t bVolNum, uint64_t ullSectorStart, uint32_t ulSectorCount, void *pBuffer);
#if REDCONF_READ_ONLY == 0
static REDSTATUS RawDiskWrite(uint8_t bVolNum, uint64_t ullSectorStart, uint32_t ulSectorCount, const void *pBuffer);
static REDSTATUS RawDiskFlush(uint8_t bVolNum);
#endif


static WINBDEV gaDisk[REDCONF_VOLUME_COUNT];


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

        if(IsDriveSpec(pszBDevSpec))
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
    bool        fIsDrive = false;

    /*  The "\\.\" prefix indicates the Win32 device namespace.
    */
    if(RedStrNCmp("\\\\.\\", pszPathSpec, 4U) == 0)
    {
        const char *pszDevice = &pszPathSpec[4U];

        /*  Subsequent to the prefix, look for a drive spec like "X:" or a disk
            spec like "PhysicalDriveX".
        */
        if(    (    ((pszDevice[0U] >= 'A') && (pszDevice[0U] <= 'Z'))
                 || ((pszDevice[0U] >= 'a') && (pszDevice[0U] <= 'z')))
            && (pszDevice[1U] == ':')
            && (pszDevice[2U] == '\0'))
        {
            fIsDrive = true;
        }
        else if((_strnicmp(pszDevice, "PhysicalDrive", 13U) == 0) && (pszDevice[13U] != '\0'))
        {
            const char *pszDiskNum = &pszDevice[13U];

            /*  Manually verify that pszDiskNum starts with a number, since
                strtol() will skip over leading white space.
            */
            if((pszDiskNum[0U] >= '0') && (pszDiskNum[0U] <= '9'))
            {
                const char *pszEndPtr;
                long lDiskNum;

                lDiskNum = strtol(pszDiskNum, (char **)&pszEndPtr, 10);
                if(lDiskNum == 0)
                {
                    /*  strtol() returns zero when no conversion could be
                        performed, but zero is also a valid disk number, so
                        check if pszDiskNum is "0".
                    */
                    if((pszDiskNum[0U] == '0') && (pszDiskNum[1U] == '\0'))
                    {
                        fIsDrive = true;
                    }
                }
                else if((lDiskNum > 0) && (lDiskNum != LONG_MAX) && (pszEndPtr[0U] == '\0'))
                {
                    fIsDrive = true;
                }
                else
                {
                    /*  Characters subsequent to "PhysicalDrive" are not a valid
                        integer, so the string is not a drive path.
                    */
                }
            }
        }
        else
        {
            /*  Characters subsequent to the "\\.\" prefix do not appear to name
                a disk, so the string is not a drive path.
            */
        }
    }

    return fIsDrive;
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

    if((bVolNum >= REDCONF_VOLUME_COUNT) || !gaDisk[bVolNum].fOpen || (pInfo == NULL))
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

            case BDEVTYPE_RAW_DISK:
                ret = RawDiskGetGeometry(bVolNum, pInfo);
                break;

            default:
                REDERROR();
                ret = -RED_EINVAL;
                break;
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

            case BDEVTYPE_RAW_DISK:
                ret = RawDiskRead(bVolNum, ullSectorStart, ulSectorCount, pBuffer);
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

            case BDEVTYPE_RAW_DISK:
                ret = RawDiskWrite(bVolNum, ullSectorStart, ulSectorCount, pBuffer);
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


/** @brief Read sectors from a RAM disk.

    @param bVolNum          The volume number of the volume whose block device
                            is being read from.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to read.
    @param pBuffer          The buffer into which to read the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    If discards and discard verification are enabled, this
                        indicates that a discarded sector is being read.
*/
static REDSTATUS RamDiskRead(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    void       *pBuffer)
{
    REDSTATUS   ret = 0;
    uint64_t    ullByteOffset = ullSectorStart * gaRedVolConf[bVolNum].ulSectorSize;
    uint32_t    ulByteCount = ulSectorCount * gaRedVolConf[bVolNum].ulSectorSize;

    memcpy(pBuffer, &gaDisk[bVolNum].pbRamDisk[ullByteOffset], ulByteCount);

    return ret;
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
    uint64_t    ullByteOffset = ullSectorStart * gaRedVolConf[bVolNum].ulSectorSize;
    uint32_t    ulByteCount = ulSectorCount * gaRedVolConf[bVolNum].ulSectorSize;

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
    @retval -RED_EIO    A disk I/O error occurred; or, automatic size detection
                        is specified and the file disk does not exist.
    @retval -RED_EROFS  The file disk is a preexisting read-only file and write
                        access was requested.
*/
static REDSTATUS FileDiskOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    WINBDEV        *pDisk = &gaDisk[bVolNum];
    REDSTATUS       ret = 0;

    if(mode != BDEV_O_RDONLY)
    {
        DWORD  dwAttr;

        /*  The media needs to be writeable.
        */
        dwAttr = GetFileAttributesA(pDisk->pszSpec);
        if((dwAttr != INVALID_FILE_ATTRIBUTES) && ((dwAttr & FILE_ATTRIBUTE_READONLY) != 0U))
        {
            ret = -RED_EROFS;
        }
    }

    if(ret == 0)
    {
        DWORD dwDesiredAccess = GENERIC_READ;
        DWORD dwCreationDisposition = OPEN_EXISTING;

      #if REDCONF_READ_ONLY == 0
        if(mode != BDEV_O_RDONLY)
        {
            /*  Open with GENERIC_READ, even if mode is BDEV_O_WRONLY, to avoid
                failures that sometimes happen when opening write-only.
            */
            dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;

            /*  If the sector count is to be automatically detected, the file
                disk must already exist.
            */
            if(gaRedVolConf[bVolNum].ullSectorCount != SECTOR_COUNT_AUTO)
            {
                dwCreationDisposition = OPEN_ALWAYS;
            }
        }
      #endif

        pDisk->hDevice = CreateFileA(pDisk->pszSpec, dwDesiredAccess, FILE_SHARE_READ,
                                     NULL, dwCreationDisposition, 0U, NULL);

        if(pDisk->hDevice == INVALID_HANDLE_VALUE)
        {
            ret = -RED_EIO;
        }
        else
        {
            ret = 0;
        }
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
        if(!FlushFileBuffers(gaDisk[bVolNum].hDevice))
        {
            ret = -RED_EIO;
        }
    }

    if(ret == 0)
    {
        if(!CloseHandle(gaDisk[bVolNum].hDevice))
        {
            ret = -RED_EIO;
        }
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

    @retval 0               Operation was successful.
    @retval -RED_EIO        A disk I/O error occurred.
    @retval -RED_ENOTSUPP   The sector size is not specified in the volume
                            config.
*/
static REDSTATUS FileDiskGetGeometry(
    uint8_t     bVolNum,
    BDEVINFO   *pInfo)
{
    REDSTATUS   ret = 0;

    if(gaRedVolConf[bVolNum].ulSectorSize == SECTOR_SIZE_AUTO)
    {
        /*  If the sector size isn't specified, any valid value will do.  Thus,
            use 512 bytes (the most common value) or the block size, whichever
            is less.
        */
        pInfo->ulSectorSize = REDMIN(512U, REDCONF_BLOCK_SIZE);
    }
    else
    {
        pInfo->ulSectorSize = gaRedVolConf[bVolNum].ulSectorSize;
    }

    if(gaRedVolConf[bVolNum].ullSectorCount == SECTOR_COUNT_AUTO)
    {
        LARGE_INTEGER   fileSize;
        BOOL            fResult = GetFileSizeEx(gaDisk[bVolNum].hDevice, &fileSize);

        if(fResult)
        {
            pInfo->ullSectorCount = fileSize.QuadPart / pInfo->ulSectorSize;
        }
        else
        {
            ret = -RED_EIO;
        }
    }
    else
    {
        pInfo->ullSectorCount = gaRedVolConf[bVolNum].ullSectorOffset + gaRedVolConf[bVolNum].ullSectorCount;
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
    ULARGE_INTEGER  position;
    OVERLAPPED      overlap = {{0}};
    uint32_t        ulSectorSize = gaRedBdevInfo[bVolNum].ulSectorSize;
    DWORD           dwRead;
    BOOL            fResult;
    REDSTATUS       ret;

    position.QuadPart = ullSectorStart * ulSectorSize;
    overlap.Offset = position.LowPart;
    overlap.OffsetHigh = position.HighPart;

    fResult = ReadFile(gaDisk[bVolNum].hDevice, pBuffer, (uint64_t)ulSectorCount * ulSectorSize, &dwRead, &overlap);
    if(fResult && (dwRead == ((uint64_t)ulSectorCount * ulSectorSize)))
    {
        ret = 0;
    }
    else
    {
        ret = -RED_EIO;
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
    ULARGE_INTEGER  position;
    OVERLAPPED      overlap = {{0}};
    uint32_t        ulSectorSize = gaRedBdevInfo[bVolNum].ulSectorSize;
    DWORD           dwWritten;
    BOOL            fResult;
    REDSTATUS       ret;

    position.QuadPart = ullSectorStart * ulSectorSize;
    overlap.Offset = position.LowPart;
    overlap.OffsetHigh = position.HighPart;

    fResult = WriteFile(gaDisk[bVolNum].hDevice, pBuffer, (uint64_t)ulSectorCount * ulSectorSize, &dwWritten, &overlap);
    if(fResult && (dwWritten == ((uint64_t)ulSectorCount * ulSectorSize)))
    {
        ret = 0;
    }
    else
    {
        ret = -RED_EIO;
    }

    return ret;
}


/** @brief Flush any caches beneath the file system.

    @param bVolNum  The volume number of the volume whose block device is being
                    flushed.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0   Operation was successful.
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
    uint32_t        ulTry;
    DWORD           dwUnused;
    WINBDEV        *pDisk = &gaDisk[bVolNum];
    REDSTATUS       ret = 0;
    DWORD           dwDesiredAccess = GENERIC_READ;

  #if REDCONF_READ_ONLY == 0
    if(mode != BDEV_O_RDONLY)
    {
        /*  Open with GENERIC_READ, even if mode is BDEV_O_WRONLY, to avoid
            failures that sometimes happen when opening write-only.
        */
        dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
    }
  #endif

    for(ulTry = 0U; ulTry <= 20U; ulTry++)
    {
        /*  Disable caching.  It would be preferable to flush the block device
            handle when needed, but attempting to do so results in an error.

            Enable both FILE_FLAG_WRITE_THROUGH and FILE_FLAG_NO_BUFFERING, so
            that system caching is not in effect, then the data is immediately
            flushed to disk without going through the Windows system cache.
            The operating system also requests a write-through of the disk's
            local hardware cache to persistent media.
        */
        pDisk->hDevice = CreateFileA(pDisk->pszSpec, dwDesiredAccess,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);

        if(pDisk->hDevice != INVALID_HANDLE_VALUE)
        {
            break;
        }

        Sleep(500U);
    }

    if(pDisk->hDevice == INVALID_HANDLE_VALUE)
    {
        ret = -RED_EIO;
    }

    if(ret == 0)
    {
        /*  Lock the volume for exclusive use.  We are purposely ignoring
            errors here, the lock volume may fail if there are open handles to
            the volume, however, the dismount of the volume will force those
            handles invalid if possible.  If the volume dismount fails, an
            application is holding a lock on the disk and we should fail.

            Note that after the dismount of the volume, the validity of the
            handle is at discretion of the original file system so we must
            clear up any ambiguity with a second call to open the volume while
            there is no file system mounted.
        */
        (void)DeviceIoControl(pDisk->hDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwUnused, NULL);
        if(!DeviceIoControl(pDisk->hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dwUnused, NULL))
        {
            ret = -RED_EBUSY;
        }
    }

    if(ret == 0)
    {
        /*  Close and reopen the handle, since the dismount may have invalidated
            the original handle.
        */
        (void)CloseHandle(pDisk->hDevice);

        for(ulTry = 0U; ulTry <= 20U; ulTry++)
        {
            pDisk->hDevice = CreateFileA(pDisk->pszSpec, dwDesiredAccess,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);

            if(pDisk->hDevice != INVALID_HANDLE_VALUE)
            {
                break;
            }

            Sleep(500U);
        }
    }

    if(ret == 0)
    {
        bool fLocked = false;

        /*  This has been observed to fail on the first attempt and succeed on
            a subsequent attempt.
        */
        for(ulTry = 0U; ulTry <= 20U; ulTry++)
        {
            if(DeviceIoControl(pDisk->hDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwUnused, NULL))
            {
                fLocked = true;
                break;
            }

            Sleep(500U);
        }

        if(!fLocked)
        {
            ret = -RED_EBUSY;
        }
    }

  #if REDCONF_READ_ONLY == 0
    if(ret == 0)
    {
        bool fWriteProtected = false;

        if(!DeviceIoControl(pDisk->hDevice, IOCTL_DISK_IS_WRITABLE, NULL, 0, NULL, 0, &dwUnused, NULL))
        {
            DWORD   dwError = GetLastError();

            if(dwError == ERROR_WRITE_PROTECT)
            {
                fWriteProtected = true;
            }
            else
            {
                ret = -RED_EIO;
            }
        }

        if((ret == 0) && fWriteProtected && (mode != BDEV_O_RDONLY))
        {
            ret = -RED_EROFS;
        }
    }
  #endif

    if((ret != 0) && (pDisk->hDevice != INVALID_HANDLE_VALUE))
    {
        (void)CloseHandle(pDisk->hDevice);
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
    REDSTATUS   ret;

    if(CloseHandle(gaDisk[bVolNum].hDevice))
    {
        ret = 0;
    }
    else
    {
        ret = -RED_EIO;
    }

    return ret;
}


/** @brief Return the block device geometry.

    @param bVolNum  The volume number of the volume whose block device geometry
                    is being queried.
    @param pInfo    On successful return, populated with the geometry of the
                    block device.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS RawDiskGetGeometry(
    uint8_t             bVolNum,
    BDEVINFO           *pInfo)
{
    REDSTATUS           ret = 0;
    DISK_GEOMETRY_EX    geo;
    DWORD               dwUnused;

    if(!DeviceIoControl(gaDisk[bVolNum].hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geo, sizeof(geo), &dwUnused, NULL))
    {
        ret = -RED_EIO;
    }
    else
    {
        PARTITION_INFORMATION_EX    partInfo;

        pInfo->ulSectorSize = geo.Geometry.BytesPerSector;

        /*  Try querying the partition info.  If the specified drive is a
            partition, this should succeed and provide an accurate length.
            Otherwise, the a physical drive was specified, not a partition.
        */
        if(DeviceIoControl(gaDisk[bVolNum].hDevice, IOCTL_DISK_GET_PARTITION_INFO_EX, NULL, 0, &partInfo, sizeof(partInfo), &dwUnused, NULL))
        {
            pInfo->ullSectorCount = partInfo.PartitionLength.QuadPart / pInfo->ulSectorSize;
        }
        else
        {
            /*  NOTE: There are issues with both methods of calculating the
                      sector count.

                      The first method may result in a sector count which
                      exceeds the number of sectors Windows thinks the media
                      has. When these purportedly non-existent sectors are read
                      or written, the I/O operation fails with a bad parameter
                      error. This behavior has shown up on numerous flash
                      drives and SD cards.

                      The second (and original) method is not known to result in
                      any I/O failures, but it can result in a sector count
                      which renders the disk much smaller than it should be;
                      this is known to have affected partitioned media.
            */
          #if 0
            pInfo->ullSectorCount  = geo.DiskSize.QuadPart;
          #else
            pInfo->ullSectorCount  = geo.Geometry.Cylinders.QuadPart;
            pInfo->ullSectorCount *= geo.Geometry.TracksPerCylinder;
            pInfo->ullSectorCount *= geo.Geometry.SectorsPerTrack;
          #endif
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
    return FileDiskRead(bVolNum, ullSectorStart, ulSectorCount, pBuffer);
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
    return FileDiskWrite(bVolNum, ullSectorStart, ulSectorCount, pBuffer);
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
    /*  Caching is disabled, nothing to flush.
    */
    return 0;
}
#endif /* REDCONF_READ_ONLY == 0 */

