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
*/
#ifndef REDCORE_H
#define REDCORE_H


#include <redstat.h>
#include "rednodes.h"
#include "redcoremacs.h"
#include "redcorevol.h"
#include "redexclude.h"


#define META_SIG_MASTER     (0x5453414DU)   /* 'MAST' */
#define META_SIG_METAROOT   (0x4154454DU)   /* 'META' */
#define META_SIG_IMAP       (0x50414D49U)   /* 'IMAP' */
#define META_SIG_INODE      (0x444F4E49U)   /* 'INOD' */
#define META_SIG_DINDIR     (0x494C4244U)   /* 'DBLI' */
#define META_SIG_INDIR      (0x49444E49U)   /* 'INDI' */


REDSTATUS RedIoRead(uint8_t bVolNum, uint32_t ulBlockStart, uint32_t ulBlockCount, void *pBuffer);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedIoWrite(uint8_t bVolNum, uint32_t ulBlockStart, uint32_t ulBlockCount, const void *pBuffer);
REDSTATUS RedIoFlush(uint8_t bVolNum);
#endif


#define BFLAG_DIRTY         ((uint16_t) 0x0001U)
#define BFLAG_NEW           ((uint16_t) 0x0002U)
#define BFLAG_META_MASTER   ((uint16_t)(0x0004U | BFLAG_META))
#define BFLAG_META_IMAP     ((uint16_t)(0x0008U | BFLAG_META))
#define BFLAG_META_INODE    ((uint16_t)(0x0010U | BFLAG_META))
#define BFLAG_META_INDIR    ((uint16_t)(0x0020U | BFLAG_META))
#define BFLAG_META_DINDIR   ((uint16_t)(0x0040U | BFLAG_META))
#define BFLAG_META          ((uint16_t) 0x8000U)


void RedBufferInit(void);
REDSTATUS RedBufferGet(uint32_t ulBlock, uint16_t uFlags, void **ppBuffer);
void RedBufferPut(const void *pBuffer);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedBufferFlush(uint32_t ulBlockStart, uint32_t ulBlockCount);
void RedBufferDirty(const void *pBuffer);
void RedBufferBranch(const void *pBuffer, uint32_t ulBlockNew);
#if (REDCONF_API_POSIX == 1) || (REDCONF_IMAGE_BUILDER == 1)
void RedBufferDiscard(const void *pBuffer);
#endif
#endif
REDSTATUS RedBufferDiscardRange(uint32_t ulBlockStart, uint32_t ulBlockCount);


/** @brief Allocation state of a block.
*/
typedef enum
{
    ALLOCSTATE_FREE,    /**< Free and may be allocated; writeable. */
    ALLOCSTATE_USED,    /**< In-use and transacted; not writeable. */
    ALLOCSTATE_NEW,     /**< In-use but not transacted; writeable. */
    ALLOCSTATE_AFREE    /**< Will become free after a transaction; not writeable. */
} ALLOCSTATE;

REDSTATUS RedImapBlockGet(uint8_t bMR, uint32_t ulBlock, bool *pfAllocated);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedImapBlockSet(uint32_t ulBlock, bool fAllocated);
REDSTATUS RedImapAllocBlock(uint32_t *pulBlock);
#endif
REDSTATUS RedImapBlockState(uint32_t ulBlock, ALLOCSTATE *pState);

#if REDCONF_IMAP_INLINE == 1
REDSTATUS RedImapIBlockGet(uint8_t bMR, uint32_t ulBlock, bool *pfAllocated);
REDSTATUS RedImapIBlockSet(uint32_t ulBlock, bool fAllocated);
#endif

#if REDCONF_IMAP_EXTERNAL == 1
REDSTATUS RedImapEBlockGet(uint8_t bMR, uint32_t ulBlock, bool *pfAllocated);
REDSTATUS RedImapEBlockSet(uint32_t ulBlock, bool fAllocated);
uint32_t RedImapNodeBlock(uint8_t bMR, uint32_t ulImapNode);
#endif


/** @brief Cached inode structure.
*/
typedef struct
{
    uint32_t    ulInode;        /**< The inode number of the cached inode. */
    bool        fDirectory;     /**< True if the inode is a directory. */
  #if REDCONF_READ_ONLY == 0
    bool        fWriteable;     /**< True if the inode is writeable. */
    bool        fDirty;         /**< True if the inode buffer is dirty. */
  #endif
    bool        fCoordInited;   /**< True after the first seek. */

    INODE      *pBuffer;        /**< Pointer to the inode buffer. */
  #if DINDIR_POINTERS > 0U
    DINDIR     *pDindir;        /**< Pointer to the double indirect node buffer. */
  #endif
  #if REDCONF_DIRECT_POINTERS < INODE_ENTRIES
    INDIR      *pIndir;         /**< Pointer to the indirect node buffer. */
  #endif
    uint8_t    *pbData;         /**< Pointer to the data block buffer. */

    uint32_t    ulLogicalBlock; /**< Logical block offset into the inode. */
  #if DINDIR_POINTERS > 0U
    uint32_t    ulDindirBlock;  /**< Physical block number of the double indirect node. */
  #endif
  #if REDCONF_DIRECT_POINTERS < INODE_ENTRIES
    uint32_t    ulIndirBlock;   /**< Physical block number of the indirect node. */
  #endif
    uint32_t    ulDataBlock;    /**< Physical block number of the file data block. */

    uint16_t    uInodeEntry;    /**< Which inode entry to traverse to reach ulLogicalBlock. */
  #if DINDIR_POINTERS > 0U
    uint16_t    uDindirEntry;   /**< Which double indirect entry to traverse to reach ulLogicalBlock. */
  #endif
  #if REDCONF_DIRECT_POINTERS < INODE_ENTRIES
    uint16_t    uIndirEntry;    /**< Which indirect entry to traverse to reach ulLogicalBlock. */
  #endif
} CINODE;


#define IPUT_UPDATE_ATIME   (0x01U)
#define IPUT_UPDATE_MTIME   (0x02U)
#define IPUT_UPDATE_CTIME   (0x04U)
#define IPUT_UPDATE_MASK    (IPUT_UPDATE_ATIME|IPUT_UPDATE_MTIME|IPUT_UPDATE_CTIME)


REDSTATUS RedInodeMount(CINODE *pInode, FTYPE type, bool fBranch);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedInodeBranch(CINODE *pInode);
#endif
#if (REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX == 1) || (REDCONF_IMAGE_BUILDER == 1))
REDSTATUS RedInodeCreate(CINODE *pInode, uint32_t ulPInode, uint16_t uMode);
#endif
#if DELETE_SUPPORTED
REDSTATUS RedInodeDelete(CINODE *pInode);
REDSTATUS RedInodeLinkDec(CINODE *pInode);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1)
REDSTATUS RedInodeFree(CINODE *pInode);
#endif
void RedInodePut(CINODE *pInode, uint8_t bTimeFields);
void RedInodePutCoord(CINODE *pInode);
#if DINDIR_POINTERS > 0U
void RedInodePutDindir(CINODE *pInode);
#endif
#if REDCONF_DIRECT_POINTERS < INODE_ENTRIES
void RedInodePutIndir(CINODE *pInode);
#endif
void RedInodePutData(CINODE *pInode);
#if ((REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX == 1) || (REDCONF_IMAGE_BUILDER == 1))) || (REDCONF_CHECKER == 1)
REDSTATUS RedInodeIsFree(uint32_t ulInode, bool *pfFree);
#endif
REDSTATUS RedInodeBitGet(uint8_t bMR, uint32_t ulInode, uint8_t bWhich, bool *pfAllocated);

REDSTATUS RedInodeDataRead(CINODE *pInode, uint64_t ullStart, uint32_t *pulLen, void *pBuffer);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedInodeDataWrite(CINODE *pInode, uint64_t ullStart, uint32_t *pulLen, const void *pBuffer);
#if DELETE_SUPPORTED || TRUNCATE_SUPPORTED
REDSTATUS RedInodeDataTruncate(CINODE *pInode, uint64_t ullSize);
#endif
#endif
REDSTATUS RedInodeDataSeekAndRead(CINODE *pInode, uint32_t ulBlock);
REDSTATUS RedInodeDataSeek(CINODE *pInode, uint32_t ulBlock);

#if REDCONF_API_POSIX == 1
#if REDCONF_READ_ONLY == 0
REDSTATUS RedDirEntryCreate(CINODE *pPInode, const char *pszName, uint32_t ulInode);
#endif
#if DELETE_SUPPORTED
REDSTATUS RedDirEntryDelete(CINODE *pPInode, uint32_t ulDeleteIdx);
#endif
REDSTATUS RedDirEntryLookup(CINODE *pPInode, const char *pszName, uint32_t *pulEntryIdx, uint32_t *pulInode);
#if (REDCONF_API_POSIX_READDIR == 1) || (REDCONF_CHECKER == 1)
REDSTATUS RedDirEntryRead(CINODE *pPInode, uint32_t *pulIdx, char *pszName, uint32_t *pulInode);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_RENAME == 1)
REDSTATUS RedDirEntryRename(CINODE *pSrcPInode, const char *pszSrcName, CINODE *pSrcInode, CINODE *pDstPInode, const char *pszDstName, CINODE *pDstInode);
#endif
#endif

REDSTATUS RedVolMount(void);
REDSTATUS RedVolMountMaster(void);
REDSTATUS RedVolMountMetaroot(void);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedVolTransact(void);
#endif
void RedVolCriticalError(const char *pszFileName, uint32_t ulLineNum);


#if (REDCONF_READ_ONLY == 0) && (((REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FORMAT == 1)) || (REDCONF_IMAGE_BUILDER == 1))
REDSTATUS RedVolFormat(void);
#endif


#endif

