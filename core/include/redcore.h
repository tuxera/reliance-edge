/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2025 Tuxera US Inc.
                      All Rights Reserved Worldwide.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; use version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but "AS-IS," WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, see <https://www.gnu.org/licenses/>.
*/
/*  Businesses and individuals that for commercial or other reasons cannot
    comply with the terms of the GPLv2 license must obtain a commercial
    license before incorporating Reliance Edge into proprietary software
    for distribution in any form.

    Visit https://www.tuxera.com/products/tuxera-edge-fs/ for more information.
*/
/** @file
*/
#ifndef REDCORE_H
#define REDCORE_H


#include <redstat.h>
#include <redformat.h>
#include <redvolume.h>
#include "rednodes.h"
#include "redcoremacs.h"
#include "redcorevol.h"


#define META_SIG_MASTER     (0x5453414DU)   /* 'MAST' */
#define META_SIG_METAROOT   (0x4154454DU)   /* 'META' */
#define META_SIG_IMAP       (0x50414D49U)   /* 'IMAP' */
#define META_SIG_INODE      (0x444F4E49U)   /* 'INOD' */
#define META_SIG_DINDIR     (0x494C4244U)   /* 'DBLI' */
#define META_SIG_INDIR      (0x49444E49U)   /* 'INDI' */
#define META_SIG_DIRECTORY  (0x44524944U)   /* 'DIRD' */


REDSTATUS RedIoRead(uint8_t bVolNum, uint32_t ulBlockStart, uint32_t ulBlockCount, void *pBuffer);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedIoWrite(uint8_t bVolNum, uint32_t ulBlockStart, uint32_t ulBlockCount, const void *pBuffer);
REDSTATUS RedIoFlush(uint8_t bVolNum);
#endif


/** Indicates a block buffer is dirty (its contents are different than the
    contents of the corresponding block on disk); or, when passed into
    RedBufferGet(), indicates that the buffer should be marked dirty.
*/
#define BFLAG_DIRTY             ((uint16_t) 0x0001U)

/** Tells RedBufferGet() that the buffer is for a newly allocated block, and its
    contents should be zeroed instead of being read from disk.  Always used in
    combination with BFLAG_DIRTY.
*/
#define BFLAG_NEW               ((uint16_t) 0x0002U)

/** Indicates that a block buffer is a master block (MASTERBLOCK) metadata node.
*/
#define BFLAG_META_MASTER       ((uint16_t)(0x0004U | BFLAG_META))

/** Indicates that a block buffer is an imap (IMAPNODE) metadata node.
*/
#define BFLAG_META_IMAP         ((uint16_t)(0x0008U | BFLAG_META))

/** Indicates that a block buffer is an inode (INODE) metadata node.
*/
#define BFLAG_META_INODE        ((uint16_t)(0x0010U | BFLAG_META))

/** Indicates that a block buffer is an indirect (INDIR) metadata node.
*/
#define BFLAG_META_INDIR        ((uint16_t)(0x0020U | BFLAG_META))

/** Indicates that a block buffer is a double indirect (DINDIR) metadata node.
*/
#define BFLAG_META_DINDIR       ((uint16_t)(0x0040U | BFLAG_META))

/** Indicates that a block buffer is a directory data block.  Only used with
    on-disk layouts where directory blocks have metadata headers.
*/
#define BFLAG_META_DIRECTORY    ((uint16_t)(0x0080U | BFLAG_META))

/** Indicates that a block buffer is a metadata node.  Callers of RedBufferGet()
    should not use this flag; instead, use one of the BFLAG_META_* flags.
*/
#define BFLAG_META              ((uint16_t) 0x8000U)


void RedBufferInit(void);
REDSTATUS RedBufferGet(uint32_t ulBlock, uint16_t uFlags, void **ppBuffer);
void RedBufferPut(const void *pBuffer);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedBufferFlushRange(uint32_t ulBlockStart, uint32_t ulBlockCount);
void RedBufferDirty(const void *pBuffer);
void RedBufferBranch(const void *pBuffer, uint32_t ulBlockNew);
#endif
void RedBufferDiscard(const void *pBuffer);
REDSTATUS RedBufferDiscardRange(uint32_t ulBlockStart, uint32_t ulBlockCount);
REDSTATUS RedBufferReadRange(uint32_t ulBlockStart, uint32_t ulBlockCount, uint8_t *pbDataBuffer);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedBufferWriteRange(uint32_t ulBlockStart, uint32_t ulBlockCount, const uint8_t *pbDataBuffer);
#endif


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
#if REDCONF_READ_ONLY == 0
REDSTATUS RedImapIBlockSet(uint32_t ulBlock, bool fAllocated);
REDSTATUS RedImapIBlockFindFree(uint32_t ulBlock, uint32_t *pulFreeBlock);
#endif
#endif

#if REDCONF_IMAP_EXTERNAL == 1
REDSTATUS RedImapEBlockGet(uint8_t bMR, uint32_t ulBlock, bool *pfAllocated);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedImapEBlockSet(uint32_t ulBlock, bool fAllocated);
REDSTATUS RedImapEBlockFindFree(uint32_t ulBlock, uint32_t *pulFreeBlock);
#endif
uint32_t RedImapNodeBlock(uint8_t bMR, uint32_t ulImapNode);
#endif


/** @brief Cached inode structure.
*/
typedef struct
{
    uint32_t    ulInode;        /**< The inode number of the cached inode. */
  #if REDCONF_API_POSIX == 1
    bool        fDirectory;     /**< True if the inode is a directory. */
  #endif
  #if REDCONF_READ_ONLY == 0
    bool        fBranched;      /**< True if the inode is branched (writeable). */
    bool        fDirty;         /**< True if the inode buffer is dirty. */
  #endif
    bool        fCoordInited;   /**< True after the first seek. */

    INODE      *pInodeBuf;      /**< Pointer to the inode buffer. */
    uint16_t    uInodeEntry;    /**< Which inode entry to traverse to reach ulLogicalBlock. */

  #if DINDIRS_EXIST
    DINDIR     *pDindir;        /**< Pointer to the double indirect node buffer. */
    uint32_t    ulDindirBlock;  /**< Physical block number of the double indirect node. */
    uint16_t    uDindirEntry;   /**< Which double indirect entry to traverse to reach ulLogicalBlock. */
  #endif

  #if INDIRS_EXIST
    INDIR      *pIndir;         /**< Pointer to the indirect node buffer. */
    uint32_t    ulIndirBlock;   /**< Physical block number of the indirect node. */
    uint16_t    uIndirEntry;    /**< Which indirect entry to traverse to reach ulLogicalBlock. */
  #endif

    uint8_t    *pbData;         /**< Pointer to the data block buffer. */
    uint32_t    ulDataBlock;    /**< Physical block number of the file data block. */
    uint32_t    ulLogicalBlock; /**< Logical block offset into the inode. */
} CINODE;

#define CINODE_IS_MOUNTED(pInode)   (((pInode) != NULL) && INODE_IS_VALID((pInode)->ulInode) && ((pInode)->pInodeBuf != NULL))
#define CINODE_IS_DIRTY(pInode)     (CINODE_IS_MOUNTED(pInode) && (pInode)->fDirty)


#define IPUT_UPDATE_ATIME   (0x01U)
#define IPUT_UPDATE_MTIME   (0x02U)
#define IPUT_UPDATE_CTIME   (0x04U)
#define IPUT_UPDATE_MASK    (IPUT_UPDATE_ATIME|IPUT_UPDATE_MTIME|IPUT_UPDATE_CTIME)


REDSTATUS RedInodeMount(CINODE *pInode, FTYPE type, bool fBranch);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedInodeBranch(CINODE *pInode);
#endif
#if (REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX == 1) || FORMAT_SUPPORTED)
REDSTATUS RedInodeCreate(CINODE *pInode, CINODE *pPInode, uint16_t uMode);
#endif
#if DELETE_SUPPORTED
REDSTATUS RedInodeLinkDec(CINODE *pInode, bool fOrphan);
#if REDCONF_DELETE_OPEN == 1
REDSTATUS RedInodeFreeOrphan(CINODE *pInode);
#endif
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1)
REDSTATUS RedInodeFree(CINODE *pInode);
#endif
void RedInodePut(CINODE *pInode, uint8_t bTimeFields);
void RedInodePutCoord(CINODE *pInode);
#if DINDIRS_EXIST
void RedInodePutDindir(CINODE *pInode);
#endif
#if INDIRS_EXIST
void RedInodePutIndir(CINODE *pInode);
#endif
void RedInodePutData(CINODE *pInode);
#if REDCONF_CHECKER == 1
REDSTATUS RedInodeIsFree(uint32_t ulInode, bool *pfFree);
#endif
#if REDCONF_CHECKER == 1
REDSTATUS RedInodeBitGet(uint8_t bMR, uint32_t ulInode, uint8_t bWhich, bool *pfAllocated);
#endif

REDSTATUS RedInodeDataRead(CINODE *pInode, uint64_t ullStart, uint32_t *pulLen, void *pBuffer);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedInodeDataWrite(CINODE *pInode, uint64_t ullStart, uint32_t *pulLen, const void *pBuffer);
#if DELETE_SUPPORTED || TRUNCATE_SUPPORTED
REDSTATUS RedInodeDataTruncate(CINODE *pInode, uint64_t ullSize);
#endif
#if (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FRESERVE == 1)
REDSTATUS RedInodeDataReserve(CINODE *pInode, uint64_t ullOffset, uint64_t ullLen);
REDSTATUS RedInodeDataUnreserve(CINODE *pInode, uint64_t ullOffset);
#endif
#endif
REDSTATUS RedInodeDataSeekAndRead(CINODE *pInode, uint32_t ulBlock);

#if REDCONF_API_POSIX == 1
#if REDCONF_READ_ONLY == 0
REDSTATUS RedDirEntryCreate(CINODE *pPInode, const char *pszName, uint32_t ulInode);
#endif
#if DELETE_SUPPORTED
REDSTATUS RedDirEntryDelete(CINODE *pPInode, CINODE *pInode, uint32_t ulDeleteIdx);
#endif
REDSTATUS RedDirEntryLookup(CINODE *pPInode, const char *pszName, uint32_t *pulEntryIdx, uint32_t *pulInode);
REDSTATUS RedDirEntryRead(CINODE *pPInode, uint32_t *pulIdx, char *pszName, uint32_t *pulInode);
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_RENAME == 1)
REDSTATUS RedDirEntryRename(CINODE *pSrcPInode, const char *pszSrcName, CINODE *pSrcInode, CINODE *pDstPInode, const char *pszDstName, CINODE *pDstInode);
#endif
#endif

REDSTATUS RedVolInitBlockGeometry(void);
REDSTATUS RedVolInitBlockLayout(void);
REDSTATUS RedVolMount(uint32_t ulFlags);
#if REDCONF_CHECKER == 1
REDSTATUS RedVolMountMaster(uint32_t ulFlags);
REDSTATUS RedVolMountMetaroot(uint32_t ulFlags);
#endif
#if REDCONF_READ_ONLY == 0
REDSTATUS RedVolTransact(void);
REDSTATUS RedVolRollback(void);
#endif
uint32_t RedVolFreeBlockCount(void);
#if DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1)
REDSTATUS RedVolFreeOrphans(uint32_t ulCount);
#endif
void RedVolCriticalError(const char *pszFileName, uint32_t ulLineNum);
REDSTATUS RedVolSeqNumIncrement(uint8_t bVolNum);

#if FORMAT_SUPPORTED
REDSTATUS RedVolFormat(const REDFMTOPT *pOptions);
#endif


#endif
