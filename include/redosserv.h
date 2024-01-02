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
    @brief Defines types and prototypes for the OS services.

    The OS services are used by cross-platform code to interact with the
    operating environment in a portable way.  The functions defined here are
    implemented for each OS in `os/[osname]/services`.
*/
#ifndef REDOSSERV_H
#define REDOSSERV_H


#include <redostypes.h>


/** @brief Type of access requested when opening a block device.
*/
typedef enum
{
    BDEV_O_RDONLY,  /**< Open block device for read access. */
    BDEV_O_WRONLY,  /**< Open block device for write access. */
    BDEV_O_RDWR     /**< Open block device for read and write access. */
} BDEVOPENMODE;

/** @brief Block device geometry information.
*/
typedef struct
{
    /** The sector size for the block device: the basic unit for reading and
        writing to the storage media.  This value is either taken from the
        ::VOLCONF, or queried from the block device.
    */
    uint32_t    ulSectorSize;

    /** The number of sectors in this block device.
    */
    uint64_t    ullSectorCount;
} BDEVINFO;

REDSTATUS RedOsBDevConfig(uint8_t bVolNum, REDBDEVCTX context);
REDSTATUS RedOsBDevOpen(uint8_t bVolNum, BDEVOPENMODE mode);
REDSTATUS RedOsBDevGetGeometry(uint8_t bVolNum, BDEVINFO *pInfo);
REDSTATUS RedOsBDevClose(uint8_t bVolNum);
REDSTATUS RedOsBDevRead(uint8_t bVolNum, uint64_t ullSectorStart, uint32_t ulSectorCount, void *pBuffer);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedOsBDevWrite(uint8_t bVolNum, uint64_t ullSectorStart, uint32_t ulSectorCount, const void *pBuffer);
REDSTATUS RedOsBDevFlush(uint8_t bVolNum);
#endif


#if REDCONF_TASK_COUNT > 1U
REDSTATUS RedOsMutexInit(void);
REDSTATUS RedOsMutexUninit(void);
void RedOsMutexAcquire(void);
void RedOsMutexRelease(void);
#endif

#if (REDCONF_TASK_COUNT > 1U) && (REDCONF_API_POSIX == 1)
uint32_t RedOsTaskId(void);
#endif

#if (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1)
uint32_t RedOsUserId(void);
uint32_t RedOsGroupId(void);
bool RedOsIsGroupMember(uint32_t ulGid);
bool RedOsIsPrivileged(void);
#if REDOSCONF_FAKE_UID_GID == 1
void RedOsFakeUidGid(uint32_t ulUid, uint32_t ulGid);
#endif
#if REDOSCONF_PERM_OVERRIDE == 1
REDSTATUS RedOsPermCheck(uint8_t bAccess, uint16_t uMode, uint32_t ulUid, uint32_t ulGid);
REDSTATUS RedOsPermCheckUnlink(uint16_t uPMode, uint32_t ulPUid, uint32_t ulPGid, uint32_t ulFUid);
#endif
#endif

REDSTATUS RedOsClockInit(void);
REDSTATUS RedOsClockUninit(void);
uint32_t RedOsClockGetTime(void);

REDSTATUS RedOsTimestampInit(void);
REDSTATUS RedOsTimestampUninit(void);
REDTIMESTAMP RedOsTimestamp(void);
uint64_t RedOsTimePassed(REDTIMESTAMP tsSince);

#if REDCONF_OUTPUT == 1
void RedOsOutputString(const char *pszString);
#endif

#if REDCONF_ASSERTS == 1
void RedOsAssertFail(const char *pszFileName, uint32_t ulLineNum);
#endif


#endif
