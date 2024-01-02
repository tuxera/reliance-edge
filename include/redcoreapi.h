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
*/
#ifndef REDCOREAPI_H
#define REDCOREAPI_H


#if REDCONF_CHECKER == 1
#include <stdio.h>
#endif

#include <redstat.h>
#include <redformat.h>


REDSTATUS RedCoreInit(void);
REDSTATUS RedCoreUninit(void);

REDSTATUS RedCoreVolSetCurrent(uint8_t bVolNum);

#if FORMAT_SUPPORTED
REDSTATUS RedCoreVolFormat(const REDFMTOPT *pOptions);
#endif
#if REDCONF_CHECKER == 1
REDSTATUS RedCoreVolCheck(FILE *pOutputFile, char *pszOutputBuffer, uint32_t nOutputBufferSize);
#endif
REDSTATUS RedCoreVolMount(uint32_t ulFlags);
REDSTATUS RedCoreVolUnmount(void);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedCoreVolTransact(void);
REDSTATUS RedCoreVolRollback(void);
#endif
REDSTATUS RedCoreVolStat(REDSTATFS *pStatFS);
#if DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1)
REDSTATUS RedCoreVolFreeOrphans(uint32_t ulCount);
#endif

#if (REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX == 1) || (REDCONF_API_FSE_TRANSMASKSET == 1))
REDSTATUS RedCoreTransMaskSet(uint32_t ulEventMask);
#endif
#if (REDCONF_API_POSIX == 1) || (REDCONF_API_FSE_TRANSMASKGET == 1)
REDSTATUS RedCoreTransMaskGet(uint32_t *pulEventMask);
#endif

#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1)
REDSTATUS RedCoreCreate(uint32_t ulPInode, const char *pszName, uint16_t uMode, uint32_t *pulInode);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_LINK == 1)
REDSTATUS RedCoreLink(uint32_t ulPInode, const char *pszName, uint32_t ulInode);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && ((REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1))
REDSTATUS RedCoreUnlink(uint32_t ulPInode, const char *pszName, bool fOrphan);
#endif
#if DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1)
REDSTATUS RedCoreFreeOrphan(uint32_t ulInode);
#endif
#if REDCONF_API_POSIX == 1
REDSTATUS RedCoreLookup(uint32_t ulPInode, const char *pszName, uint32_t *pulInode);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_RENAME == 1)
REDSTATUS RedCoreRename(uint32_t ulSrcPInode, const char *pszSrcName, uint32_t ulDstPInode, const char *pszDstName, bool fOrphan);
#endif
#if REDCONF_API_POSIX == 1
REDSTATUS RedCoreStat(uint32_t ulInode, REDSTAT *pStat);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1)
REDSTATUS RedCoreChmod(uint32_t ulInode, uint16_t uMode);
REDSTATUS RedCoreChown(uint32_t ulInode, uint32_t ulUID, uint32_t ulGID);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && (REDCONF_INODE_TIMESTAMPS == 1)
REDSTATUS RedCoreUTimes(uint32_t ulInode, const uint32_t *pulTimes);
#endif
#if REDCONF_API_FSE == 1
REDSTATUS RedCoreFileSizeGet(uint32_t ulInode, uint64_t *pullSize);
#endif

REDSTATUS RedCoreFileRead(uint32_t ulInode, uint64_t ullStart, uint32_t *pulLen, void *pBuffer);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedCoreFileWrite(uint32_t ulInode, uint64_t ullStart, uint32_t *pulLen, const void *pBuffer);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FRESERVE == 1)
REDSTATUS RedCoreFileWriteReserved(uint32_t ulInode, uint64_t ullStart, uint32_t *pulLen, const void *pBuffer);
#endif
#if TRUNCATE_SUPPORTED
REDSTATUS RedCoreFileTruncate(uint32_t ulInode, uint64_t ullSize);
#endif

#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FRESERVE == 1)
REDSTATUS RedCoreFileReserve(uint32_t ulInode, uint64_t ullOffset, uint64_t ullLen);
REDSTATUS RedCoreFileUnreserve(uint32_t ulInode, uint64_t ullOffset);
#endif

#if REDCONF_API_POSIX == 1
REDSTATUS RedCoreDirRead(uint32_t ulInode, uint32_t *pulPos, char *pszName, uint32_t *pulInode);
REDSTATUS RedCoreDirParent(uint32_t ulInode, uint32_t *pulPInode);
#endif


#endif
