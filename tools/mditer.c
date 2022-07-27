/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2022 Tuxera US Inc.
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
    @brief Utility for iterating Reliance Edge committed-state metadata.

    This utility will iterate over the committed-state metadata of a Reliance
    Edge volume and invoke a caller-supplied callback routine for each metadata
    block.  Optionally, this utility can also validate each metadata block.

    This utility is used for advanced file system tests that need to examine
    (and possibly modify) every metadata block.

    The metadata is returned in the following order:
      - Master block
      - Metaroots (both, if both are valid)
      - Inode metadata (inodes, double indirects, indirects, directory data),
        from first inode to last, skipping free inodes.  Within each inode, the
        current order is bottom-up, low to high offset.

    This utility is used with the endian-swapping tests, and thus it must be
    endian agnostic.
*/
#include <stdlib.h>
#include <stdio.h>

#include <redfs.h>
#include <redbdev.h>
#include <redvolume.h>
#include <redcoreapi.h>
#include <redcore.h>
#include <redmditer.h>


#define SWAP16(val) ((((val) & 0xFF00U) >> 8U) | (((val) & 0x00FFU) << 8U))
#define SWAP32(val) \
    ((((val) & 0x000000FFU) << 24U) | (((val) & 0x0000FF00U) <<  8U) | \
     (((val) & 0x00FF0000U) >>  8U) | (((val) & 0xFF000000U) >> 24U))


/** @brief Metadata iterator context structure.
*/
typedef struct
{
    /** Parameters.
    */
    const MDITERPARAM *pParam;

    /** On-disk layout version found in the master block.
    */
    uint32_t    ulVersion;

    /** Maximum sequence number found in the master block and the valid
        metaroots.  Used to validate the sequence numbers of other metadata
        nodes.
    */
    uint64_t    ullSeqMax;

    /** Entries from the newer metaroot.  If the external imap is used, this is
        needed to parse it; otherwise it gets used to parse the inodes.
    */
    uint8_t     abMREntries[METAROOT_ENTRY_BYTES];

  #if REDCONF_IMAP_EXTERNAL == 1
    /** In-memory copy of the first part of the imap which is needed to parse
        the inodes.
    */
    uint8_t    *pbInodeImap;

    /** Number of imap nodes which have entries needed to parse the inodes.
    */
    uint32_t    ulInodeImapNodes;
  #endif

    /** This is the maximum number of inodes (files and directories).
    */
    uint32_t    ulInodeCount;
} MDICTX;


static REDSTATUS MDIter(const MDITERPARAM *pParam);
static REDSTATUS MDIterMB(MDICTX *pCtx);
static REDSTATUS MDIterMR(MDICTX *pCtx);
#if REDCONF_IMAP_EXTERNAL == 1
static REDSTATUS MDIterImaps(MDICTX *pCtx);
#endif
static REDSTATUS MDIterInodes(MDICTX *pCtx);
#if DINDIR_POINTERS > 0U
static REDSTATUS MDIterDindir(MDICTX *pCtx, uint32_t ulBlock, uint32_t ulInode, bool fIsDirectory);
#endif
#if REDCONF_DIRECT_POINTERS < INODE_ENTRIES
static REDSTATUS MDIterIndir(MDICTX *pCtx, uint32_t ulBlock, uint32_t ulInode, bool fIsDirectory);
#endif
#if REDCONF_API_POSIX == 1
static REDSTATUS MDIterDirectoryBlock(MDICTX *pCtx, uint32_t ulBlock);
#endif
#if REDCONF_IMAP_EXTERNAL == 1
static uint32_t ImapBlock(MDICTX *pCtx, uint32_t ulImapNode);
#endif
static uint32_t InodeBlock(MDICTX *pCtx, uint32_t ulInode);
static void *AllocBlkBuf(void **ppFreePtr);
static NODEHEADER NodeHdrExtract(const void *pBuffer);
static void MemCpyAndReverse(void *pDst, const void *pSrc, uint32_t ulLen);


/** @brief Iterate the committed-state metadata.

    @param pParam   Metadata iteration parameters.

    @return A negated ::REDSTATUS code indicating the operation result.
*/
REDSTATUS RedMetadataIterate(
    const MDITERPARAM  *pParam)
{
    REDSTATUS           ret;
    REDSTATUS           ret2;

    if(    (pParam == NULL)
        || (pParam->bVolNum >= REDCONF_VOLUME_COUNT)
        || (pParam->pfnCallback == NULL))
    {
        ret = -RED_EINVAL;
        goto Out;
    }

    if(gaRedVolume[pParam->bVolNum].fMounted)
    {
        fprintf(stderr, "RedMetadataIterate() cannot be used on a mounted volume\n");
        ret = -RED_EBUSY;
        goto Out;
    }

    /*  Initialize early on since this also prints the signon message.
    */
    ret = RedCoreInit();
    if(ret != 0)
    {
        fprintf(stderr, "Unexpected error %d from RedCoreInit()\n", (int)ret);
        goto Out;
    }

    if(pParam->pszDevice != NULL)
    {
        const char *pszDrive = pParam->pszDevice;

        ret = RedOsBDevConfig(pParam->bVolNum, pszDrive);
        if(ret != 0)
        {
            fprintf(stderr, "Unexpected error %d from RedOsBDevConfig()\n", (int)ret);
            goto Out;
        }
    }

  #if REDCONF_VOLUME_COUNT > 1U
    ret = RedCoreVolSetCurrent(pParam->bVolNum);
    if(ret != 0)
    {
        fprintf(stderr, "Unexpected error %d from RedCoreVolSetCurrent()\n", (int)ret);
        goto Out;
    }
  #endif

  #if REDCONF_READ_ONLY == 0
    /*  This utility only reads from the block device, but open read-write in
        case the callback writes.
    */
    ret = RedBDevOpen(gbRedVolNum, BDEV_O_RDWR);
  #else
    ret = RedBDevOpen(gbRedVolNum, BDEV_O_RDONLY);
  #endif
    if(ret != 0)
    {
        fprintf(stderr, "Unexpected error %d from RedBDevOpen()\n", (int)ret);
        goto Out;
    }

    /*  Volume geometry needs to be initialized to parse the volume.
    */
    ret = RedVolInitBlockGeometry();
    if(ret != 0)
    {
        fprintf(stderr, "Unexpected error %d from RedVolInitBlockGeometry()\n", (int)ret);
        goto Close;
    }

    ret = MDIter(pParam);

  Close:

    ret2 = RedBDevClose(gbRedVolNum);
    if(ret2 != 0)
    {
        fprintf(stderr, "Unexpected error %d from RedBDevOpen()\n", (int)ret2);

        if(ret == 0)
        {
            ret = ret2;
        }
    }

  Out:

    if(ret != 0)
    {
        fprintf(stderr, "Metadata iteration terminated with error status %d\n", (int)ret);
    }

    return ret;
}


/** @brief Iterate the committed-state metadata.

    @param pParam   Metadata iteration parameters.

    @return A negated ::REDSTATUS code indicating the operation result.
*/
static REDSTATUS MDIter(
    const MDITERPARAM  *pParam)
{
    MDICTX              ctx = {0U};
    REDSTATUS           ret;

    ctx.pParam = pParam;

    ret = MDIterMB(&ctx);

    if(ret == 0)
    {
        ret = MDIterMR(&ctx);
    }

  #if REDCONF_IMAP_EXTERNAL == 1
    if((ret == 0) && !gpRedCoreVol->fImapInline)
    {
        /*  That portion of the imap which covers the inode blocks is saved in
            RAM so we can parse the inodes without rereading the imap blocks.
            Note that each inode has two bits in the imap.
        */
        ctx.ulInodeImapNodes = ((ctx.ulInodeCount * 2U) + (IMAPNODE_ENTRIES - 1U)) / IMAPNODE_ENTRIES;
        ctx.pbInodeImap = malloc((size_t)ctx.ulInodeImapNodes * IMAPNODE_ENTRY_BYTES);
        if(ctx.pbInodeImap == NULL)
        {
            fprintf(stderr, "Failed to allocate imap buffer\n");
            ret = -RED_ENOMEM;
        }
        else
        {
            ret = MDIterImaps(&ctx);
        }
    }
  #endif

    if(ret == 0)
    {
        ret = MDIterInodes(&ctx);
    }

  #if REDCONF_IMAP_EXTERNAL == 1
    if(ctx.pbInodeImap != NULL)
    {
        free(ctx.pbInodeImap);
    }
  #endif

    return ret;
}


/** @brief Iterate the master block.

    @param pCtx Metadata iteration context structure.

    @return A negated ::REDSTATUS code indicating the operation result.
*/
static REDSTATUS MDIterMB(
    MDICTX         *pCtx)
{
    MASTERBLOCK    *pMB;
    void           *pFreePtr;
    REDSTATUS       ret;

    pMB = AllocBlkBuf(&pFreePtr);
    if(pMB == NULL)
    {
        ret = -RED_ENOMEM;
        goto Out;
    }

    ret = RedIoRead(gbRedVolNum, 0U, 1U, pMB);
    if(ret != 0)
    {
        fprintf(stderr, "Error %d reading block 0\n", (int)ret);
        goto Out;
    }

    if(pCtx->pParam->fVerify)
    {
        NODEHEADER hdr = NodeHdrExtract(pMB);

        if(hdr.ulSignature != META_SIG_MASTER)
        {
            fprintf(stderr, "Missing master block signature in block 0: found 0x%08lx, expected 0x%08lx\n",
                (unsigned long)hdr.ulSignature, (unsigned long)META_SIG_MASTER);
            ret = -RED_EIO;
        }

        if(hdr.ulCRC != RedCrcNode(pMB))
        {
            fprintf(stderr, "Invalid master block CRC in block 0: found 0x%08lx, expected 0x%08lx\n",
                (unsigned long)hdr.ulCRC, (unsigned long)RedCrcNode(pMB));
            ret = -RED_EIO;
        }

        if(ret != 0)
        {
            goto Out;
        }

        /*  On a freshly formatted volume, the master block has the highest
            sequence number.
        */
        pCtx->ullSeqMax = hdr.ullSequence;
    }

    /*  Save the on-disk layout number, block count, and inode count so we know
        how to interpret the other metadata.
    */
    if(pMB->hdr.ulSignature == SWAP32(META_SIG_MASTER))
    {
        pCtx->ulVersion = SWAP32(pMB->ulVersion);
        pCtx->ulInodeCount = SWAP32(pMB->ulInodeCount);
        gpRedVolume->ulBlockCount = SWAP32(pMB->ulBlockCount);
    }
    else
    {
        pCtx->ulVersion = pMB->ulVersion;
        pCtx->ulInodeCount = pMB->ulInodeCount;
        gpRedVolume->ulBlockCount = pMB->ulBlockCount;
    }

    /*  If the version is junk, assume the default.
    */
    if(!RED_DISK_LAYOUT_IS_SUPPORTED(pCtx->ulVersion))
    {
        pCtx->ulVersion = RED_DISK_LAYOUT_VERSION;
    }

    gpRedCoreVol->ulInodeCount = pCtx->ulInodeCount;

    ret = RedVolInitBlockLayout();

    if(ret != 0)
    {
        fprintf(stderr, "Unexpected error %d from RedVolInitBlockLayout()\n", (int)ret);
    }
    else
    {
        ret = pCtx->pParam->pfnCallback(pCtx->pParam->pContext, MDTYPE_MASTER, 0U, pMB);
    }

  Out:

    if(pMB != NULL)
    {
        free(pFreePtr);
    }

    return ret;
}


/** @brief Iterate the metaroots.

    @param pCtx Metadata iteration context structure.

    @return A negated ::REDSTATUS code indicating the operation result.
*/
static REDSTATUS MDIterMR(
    MDICTX     *pCtx)
{
    METAROOT   *pMR;
    void       *pFreePtr;
    uint32_t    i;
    uint64_t    ullMRSeq = 0U;
    bool        fEitherValid = false;
    REDSTATUS   ret;

    pMR = AllocBlkBuf(&pFreePtr);
    if(pMR == NULL)
    {
        ret = -RED_ENOMEM;
        goto Out;
    }

    for(i = 0U; i < 2U; i++)
    {
        uint8_t    *pbMR = (uint8_t *)pMR;
        bool        fValid = true;
        uint32_t    ulBlock = 1U + i;
        uint32_t    ulSectorCRC;
        NODEHEADER  hdr;

        ret = RedIoRead(gbRedVolNum, ulBlock, 1U, pMR);
        if(ret != 0)
        {
            fprintf(stderr, "Error %d reading block %lu\n", (int)ret, (unsigned long)ulBlock);
            goto Out;
        }

        hdr = NodeHdrExtract(pMR);

        if(hdr.ulSignature != META_SIG_METAROOT)
        {
            fprintf(stderr, "Missing metaroot signature in block %lu: found 0x%08lx, expected 0x%08lx\n",
                (unsigned long)ulBlock, (unsigned long)hdr.ulSignature, (unsigned long)META_SIG_METAROOT);
            fValid = false;
        }

        /*  Zero the sector CRC, which is required in order to compute the MR
            node CRC.
        */
        RedMemCpy(&ulSectorCRC, &pbMR[NODEHEADER_SIZE], sizeof(ulSectorCRC));
        RedMemSet(&pbMR[NODEHEADER_SIZE], 0U, sizeof(ulSectorCRC));

        if(hdr.ulCRC != RedCrcNode(pMR))
        {
            fprintf(stderr, "Invalid metaroot CRC in block %lu: found 0x%08lx, expected 0x%08lx\n",
                (unsigned long)ulBlock, (unsigned long)hdr.ulCRC, (unsigned long)RedCrcNode(pMR));
            fValid = false;
        }

        /*  Restore the sector CRC.
        */
        RedMemCpy(&pbMR[NODEHEADER_SIZE], &ulSectorCRC, sizeof(ulSectorCRC));

        if(fValid)
        {
            fEitherValid = true;

            if(ullMRSeq < hdr.ullSequence)
            {
                ullMRSeq = hdr.ullSequence;

                /*  Save the entries from the newest valid MR -- needed in order
                    to parse the volume.
                */
                RedMemCpy(pCtx->abMREntries, pMR->abEntries, sizeof(pCtx->abMREntries));
            }

            ret = pCtx->pParam->pfnCallback(pCtx->pParam->pContext, MDTYPE_METAROOT, ulBlock, pMR);
            if(ret != 0)
            {
                goto Out;
            }
        }
    }

    /*  In order to iterate the other metadata (imaps and inodes), we need a
        valid metaroot.
    */
    if(!fEitherValid)
    {
        fprintf(stderr, "Neither metaroot block is valid, cannot continue\n");
        ret = -RED_EIO;
        goto Out;
    }

    if(pCtx->pParam->fVerify && (pCtx->ullSeqMax < ullMRSeq))
    {
        pCtx->ullSeqMax = ullMRSeq;
    }

  Out:

    if(pMR != NULL)
    {
        free(pFreePtr);
    }

    return ret;
}


#if REDCONF_IMAP_EXTERNAL == 1
/** @brief Iterate the imap nodes.

    @param pCtx Metadata iteration context structure.

    @return A negated ::REDSTATUS code indicating the operation result.
*/
static REDSTATUS MDIterImaps(
    MDICTX     *pCtx)
{
    IMAPNODE   *pImap;
    void       *pFreePtr;
    uint32_t    i;
    REDSTATUS   ret = 0;

    pImap = AllocBlkBuf(&pFreePtr);
    if(pImap == NULL)
    {
        ret = -RED_ENOMEM;
        goto Out;
    }

    for(i = 0U; i < gpRedCoreVol->ulImapNodeCount; i++)
    {
        uint32_t ulBlock = ImapBlock(pCtx, i);

        ret = RedIoRead(gbRedVolNum, ulBlock, 1U, pImap);
        if(ret != 0)
        {
            fprintf(stderr, "Error %d reading block %lu\n", (int)ret, (unsigned long)ulBlock);
            goto Out;
        }

        if(pCtx->pParam->fVerify)
        {
            NODEHEADER hdr = NodeHdrExtract(pImap);

            if(hdr.ulSignature != META_SIG_IMAP)
            {
                fprintf(stderr, "Missing imap signature in block %lu: found 0x%08lx, expected 0x%08lx\n",
                    (unsigned long)ulBlock, (unsigned long)hdr.ulSignature, (unsigned long)META_SIG_IMAP);
                ret = -RED_EIO;
            }

            if(hdr.ulCRC != RedCrcNode(pImap))
            {
                fprintf(stderr, "Invalid imap CRC in block %lu: found 0x%08lx, expected 0x%08lx\n",
                    (unsigned long)ulBlock, (unsigned long)hdr.ulCRC, (unsigned long)RedCrcNode(pImap));
                ret = -RED_EIO;
            }

            if(hdr.ullSequence >= pCtx->ullSeqMax)
            {
                fprintf(stderr, "Invalid imap seqnum in block %lu: found 0x%08llx, expected < 0x%08llx\n",
                    (unsigned long)ulBlock, (unsigned long long)hdr.ullSequence, (unsigned long long)pCtx->ullSeqMax);
                ret = -RED_EIO;
            }

            if(ret != 0)
            {
                goto Out;
            }
        }

        /*  The portions of the imap that are needed to parse the inodes are
            saved.
        */
        if(i < pCtx->ulInodeImapNodes)
        {
            RedMemCpy(&pCtx->pbInodeImap[i * IMAPNODE_ENTRY_BYTES], pImap->abEntries, IMAPNODE_ENTRY_BYTES);
        }

        ret = pCtx->pParam->pfnCallback(pCtx->pParam->pContext, MDTYPE_IMAP, ulBlock, pImap);
        if(ret != 0)
        {
            goto Out;
        }
    }

  Out:

    if(pImap != NULL)
    {
        free(pFreePtr);
    }

    return ret;
}
#endif /* REDCONF_IMAP_EXTERNAL == 1U */


/** @brief Iterate the inode metadata (inodes and all child metadata).

    @param pCtx Metadata iteration context structure.

    @return A negated ::REDSTATUS code indicating the operation result.
*/
static REDSTATUS MDIterInodes(
    MDICTX     *pCtx)
{
    INODE      *pInode;
    void       *pFreePtr;
    uint32_t    ulInode;
    REDSTATUS   ret = 0;

    pInode = AllocBlkBuf(&pFreePtr);
    if(pInode == NULL)
    {
        ret = -RED_ENOMEM;
        goto Out;
    }

    for(ulInode = INODE_FIRST_VALID; ulInode < (INODE_FIRST_VALID + pCtx->ulInodeCount); ulInode++)
    {
        uint32_t    ulBlock = InodeBlock(pCtx, ulInode);
        uint32_t    i;
        bool        fEndSwap;
        bool        fIsDirectory;

        if(ulBlock == BLOCK_SPARSE)
        {
            continue; /* Inode is free */
        }

        ret = RedIoRead(gbRedVolNum, ulBlock, 1U, pInode);
        if(ret != 0)
        {
            fprintf(stderr, "Error %d reading block %lu\n", (int)ret, (unsigned long)ulBlock);
            goto Out;
        }

        if(pCtx->pParam->fVerify)
        {
            NODEHEADER hdr = NodeHdrExtract(pInode);

            if(hdr.ulSignature != META_SIG_INODE)
            {
                fprintf(stderr, "Missing inode signature in block %lu: found 0x%08lx, expected 0x%08lx\n",
                    (unsigned long)ulBlock, (unsigned long)hdr.ulSignature, (unsigned long)META_SIG_INODE);
                ret = -RED_EIO;
            }

            if(hdr.ulCRC != RedCrcNode(pInode))
            {
                fprintf(stderr, "Invalid inode CRC in block %lu: found 0x%08lx, expected 0x%08lx\n",
                    (unsigned long)ulBlock, (unsigned long)hdr.ulCRC, (unsigned long)RedCrcNode(pInode));
                ret = -RED_EIO;
            }

            if(hdr.ullSequence >= pCtx->ullSeqMax)
            {
                fprintf(stderr, "Invalid inode seqnum in block %lu: found 0x%08llx, expected < 0x%08llx\n",
                    (unsigned long)ulBlock, (unsigned long long)hdr.ullSequence, (unsigned long long)pCtx->ullSeqMax);
                ret = -RED_EIO;
            }

            if(ret != 0)
            {
                goto Out;
            }
        }

        fEndSwap = (pInode->hdr.ulSignature == SWAP32(META_SIG_INODE));

      #if REDCONF_API_POSIX == 1
        fIsDirectory = RED_S_ISDIR(fEndSwap ? SWAP16(pInode->uMode) : pInode->uMode);
      #else
        fIsDirectory = false;
      #endif
        (void)fIsDirectory; /* Unused in some configurations. */

        for(i = 0U; i < INODE_ENTRIES; i++)
        {
            uint32_t ulEntryBlock = pInode->aulEntries[i];

            if(fEndSwap)
            {
                ulEntryBlock = SWAP32(ulEntryBlock);
            }

            if(ulEntryBlock == BLOCK_SPARSE)
            {
                continue;
            }

          #if REDCONF_DIRECT_POINTERS > 0U
            if(i < REDCONF_DIRECT_POINTERS)
            {
              #if REDCONF_API_POSIX == 1
                if(fIsDirectory)
                {
                    ret = MDIterDirectoryBlock(pCtx, ulEntryBlock);
                }
              #endif
            }
            else
          #endif
          #if REDCONF_INDIRECT_POINTERS > 0U
            if(i < (REDCONF_DIRECT_POINTERS + REDCONF_INDIRECT_POINTERS))
            {
                ret = MDIterIndir(pCtx, ulEntryBlock, ulInode, fIsDirectory);
            }
            else
          #endif
            {
              #if DINDIR_POINTERS > 0U
                ret = MDIterDindir(pCtx, ulEntryBlock, ulInode, fIsDirectory);
              #endif
            }

            if(ret != 0)
            {
                goto Out;
            }
        }

        ret = pCtx->pParam->pfnCallback(pCtx->pParam->pContext, MDTYPE_INODE, ulBlock, pInode);
        if(ret != 0)
        {
            goto Out;
        }
    }

  Out:

    if(pInode != NULL)
    {
        free(pFreePtr);
    }

    return ret;
}


#if DINDIR_POINTERS > 0U
/** @brief Iterate a double indirect node (and all indirect node children).

    @param pCtx         Metadata iteration context structure.
    @param ulBlock      The location of the double indirect node.
    @param ulInode      The inode which owns this double indirect node.
    @param fIsDirectory Whether the inode is a directory.

    @return A negated ::REDSTATUS code indicating the operation result.
*/
static REDSTATUS MDIterDindir(
    MDICTX     *pCtx,
    uint32_t    ulBlock,
    uint32_t    ulInode,
    bool        fIsDirectory)
{
    DINDIR     *pDindir;
    void       *pFreePtr;
    uint32_t    i;
    bool        fEndSwap;
    REDSTATUS   ret;

    pDindir = AllocBlkBuf(&pFreePtr);
    if(pDindir == NULL)
    {
        ret = -RED_ENOMEM;
        goto Out;
    }

    ret = RedIoRead(gbRedVolNum, ulBlock, 1U, pDindir);
    if(ret != 0)
    {
        fprintf(stderr, "Error %d reading block %lu\n", (int)ret, (unsigned long)ulBlock);
        goto Out;
    }

    fEndSwap = (pDindir->hdr.ulSignature == SWAP32(META_SIG_DINDIR));

    if(pCtx->pParam->fVerify)
    {
        NODEHEADER hdr = NodeHdrExtract(pDindir);
        uint32_t   ulOwnerInode = fEndSwap ? SWAP32(pDindir->ulInode) : pDindir->ulInode;

        if(hdr.ulSignature != META_SIG_DINDIR)
        {
            fprintf(stderr, "Missing double indirect signature in block %lu: found 0x%08lx, expected 0x%08lx\n",
                (unsigned long)ulBlock, (unsigned long)hdr.ulSignature, (unsigned long)META_SIG_DINDIR);
            ret = -RED_EIO;
        }

        if(hdr.ulCRC != RedCrcNode(pDindir))
        {
            fprintf(stderr, "Invalid double indirect CRC in block %lu: found 0x%08lx, expected 0x%08lx\n",
                (unsigned long)ulBlock, (unsigned long)hdr.ulCRC, (unsigned long)RedCrcNode(pDindir));
            ret = -RED_EIO;
        }

        if(hdr.ullSequence >= pCtx->ullSeqMax)
        {
            fprintf(stderr, "Invalid double indirect seqnum in block %lu: found 0x%08llx, expected < 0x%08llx\n",
                (unsigned long)ulBlock, (unsigned long long)hdr.ullSequence, (unsigned long long)pCtx->ullSeqMax);
            ret = -RED_EIO;
        }

        if(ulOwnerInode != ulInode)
        {
            fprintf(stderr, "Invalid double indirect inode in block %lu: found %lu, expected %lu\n",
                (unsigned long)ulBlock, (unsigned long)ulOwnerInode, (unsigned long)ulInode);
            ret = -RED_EIO;
        }

        if(ret != 0)
        {
            goto Out;
        }
    }

    for(i = 0U; i < INDIR_ENTRIES; i++)
    {
        uint32_t ulEntryBlock = pDindir->aulEntries[i];

        if(fEndSwap)
        {
            ulEntryBlock = SWAP32(ulEntryBlock);
        }

        if(ulEntryBlock == BLOCK_SPARSE)
        {
            continue;
        }

        ret = MDIterIndir(pCtx, ulEntryBlock, ulInode, fIsDirectory);
        if(ret != 0)
        {
            goto Out;
        }
    }

    ret = pCtx->pParam->pfnCallback(pCtx->pParam->pContext, MDTYPE_DINDIR, ulBlock, pDindir);

  Out:

    if(pDindir != NULL)
    {
        free(pFreePtr);
    }

    return ret;
}
#endif /* DINDIR_POINTERS > 0U */


#if REDCONF_DIRECT_POINTERS < INODE_ENTRIES
/** @brief Iterate an indirect node (and all directory data children).

    @param pCtx         Metadata iteration context structure.
    @param ulBlock      The location of the indirect node.
    @param ulInode      The inode which owns this indirect node.
    @param fIsDirectory Whether the inode is a directory.

    @return A negated ::REDSTATUS code indicating the operation result.
*/
static REDSTATUS MDIterIndir(
    MDICTX     *pCtx,
    uint32_t    ulBlock,
    uint32_t    ulInode,
    bool        fIsDirectory)
{
    INDIR      *pIndir;
    void       *pFreePtr;
    bool        fEndSwap;
    REDSTATUS   ret;

    pIndir = AllocBlkBuf(&pFreePtr);
    if(pIndir == NULL)
    {
        ret = -RED_ENOMEM;
        goto Out;
    }

    ret = RedIoRead(gbRedVolNum, ulBlock, 1U, pIndir);
    if(ret != 0)
    {
        fprintf(stderr, "Error %d reading block %lu\n", (int)ret, (unsigned long)ulBlock);
        goto Out;
    }

    fEndSwap = (pIndir->hdr.ulSignature == SWAP32(META_SIG_INDIR));

    if(pCtx->pParam->fVerify)
    {
        NODEHEADER hdr = NodeHdrExtract(pIndir);
        uint32_t   ulOwnerInode = fEndSwap ? SWAP32(pIndir->ulInode) : pIndir->ulInode;

        if(hdr.ulSignature != META_SIG_INDIR)
        {
            fprintf(stderr, "Missing indirect signature in block %lu: found 0x%08lx, expected 0x%08lx\n",
                (unsigned long)ulBlock, (unsigned long)hdr.ulSignature, (unsigned long)META_SIG_INDIR);
            ret = -RED_EIO;
        }

        if(hdr.ulCRC != RedCrcNode(pIndir))
        {
            fprintf(stderr, "Invalid indirect CRC in block %lu: found 0x%08lx, expected 0x%08lx\n",
                (unsigned long)ulBlock, (unsigned long)hdr.ulCRC, (unsigned long)RedCrcNode(pIndir));
            ret = -RED_EIO;
        }

        if(hdr.ullSequence >= pCtx->ullSeqMax)
        {
            fprintf(stderr, "Invalid indirect seqnum in block %lu: found 0x%08llx, expected < 0x%08llx\n",
                (unsigned long)ulBlock, (unsigned long long)hdr.ullSequence, (unsigned long long)pCtx->ullSeqMax);
            ret = -RED_EIO;
        }

        if(ulOwnerInode != ulInode)
        {
            fprintf(stderr, "Invalid indirect inode in block %lu: found %lu, expected %lu\n",
                (unsigned long)ulBlock, (unsigned long)ulOwnerInode, (unsigned long)ulInode);
            ret = -RED_EIO;
        }

        if(ret != 0)
        {
            goto Out;
        }
    }

  #if REDCONF_API_POSIX == 1
    if(fIsDirectory)
    {
        uint32_t i;

        for(i = 0U; i < INDIR_ENTRIES; i++)
        {
            uint32_t ulEntryBlock = pIndir->aulEntries[i];

            if(fEndSwap)
            {
                ulEntryBlock = SWAP32(ulEntryBlock);
            }

            if(ulEntryBlock == BLOCK_SPARSE)
            {
                continue;
            }

            ret = MDIterDirectoryBlock(pCtx, ulEntryBlock);
            if(ret != 0)
            {
                goto Out;
            }
        }
    }
  #else
    (void)fIsDirectory;
  #endif

    ret = pCtx->pParam->pfnCallback(pCtx->pParam->pContext, MDTYPE_INDIR, ulBlock, pIndir);

  Out:

    if(pIndir != NULL)
    {
        free(pFreePtr);
    }

    return ret;

}
#endif /* REDCONF_DIRECT_POINTERS < IMAPNODE_ENTRIES */


#if REDCONF_API_POSIX == 1
/** @brief Iterate a directory data block.

    @param pCtx     Metadata iteration context structure.
    @param ulBlock  Location of the directory data block.

    @return A negated ::REDSTATUS code indicating the operation result.
*/
static REDSTATUS MDIterDirectoryBlock(
    MDICTX     *pCtx,
    uint32_t    ulBlock)
{
    void       *pDirData;
    void       *pFreePtr;
    REDSTATUS   ret;

    pDirData = AllocBlkBuf(&pFreePtr);
    if(pDirData == NULL)
    {
        ret = -RED_ENOMEM;
        goto Out;
    }

    ret = RedIoRead(gbRedVolNum, ulBlock, 1U, pDirData);
    if(ret != 0)
    {
        fprintf(stderr, "Error %d reading block %lu\n", (int)ret, (unsigned long)ulBlock);
        goto Out;
    }

    if(pCtx->pParam->fVerify && (pCtx->ulVersion >= RED_DISK_LAYOUT_DIRCRC))
    {
        NODEHEADER hdr = NodeHdrExtract(pDirData);

        if(hdr.ulSignature != META_SIG_DIRECTORY)
        {
            fprintf(stderr, "Missing directory signature in block %lu: found 0x%08lx, expected 0x%08lx\n",
                (unsigned long)ulBlock, (unsigned long)hdr.ulSignature, (unsigned long)META_SIG_DIRECTORY);
            ret = -RED_EIO;
        }

        if(hdr.ulCRC != RedCrcNode(pDirData))
        {
            fprintf(stderr, "Invalid directory CRC in block %lu: found 0x%08lx, expected 0x%08lx\n",
                (unsigned long)ulBlock, (unsigned long)hdr.ulCRC, (unsigned long)RedCrcNode(pDirData));
            ret = -RED_EIO;
        }

        if(hdr.ullSequence >= pCtx->ullSeqMax)
        {
            fprintf(stderr, "Invalid directory seqnum in block %lu: found 0x%08llx, expected < 0x%08llx\n",
                (unsigned long)ulBlock, (unsigned long long)hdr.ullSequence, (unsigned long long)pCtx->ullSeqMax);
            ret = -RED_EIO;
        }

        if(ret != 0)
        {
            goto Out;
        }
    }

    ret = pCtx->pParam->pfnCallback(pCtx->pParam->pContext, MDTYPE_DIRECTORY, ulBlock, pDirData);

  Out:

    if(pDirData != NULL)
    {
        free(pFreePtr);
    }

    return ret;
}
#endif /* REDCONF_API_POSIX == 1 */


#if REDCONF_IMAP_EXTERNAL == 1
/** @brief Compute the block number for an imap node.

    @param pCtx         Metadata iteration context structure.
    @param ulImapNode   The imap inode.

    @return The block number for @p ulImapNode.
*/
static uint32_t ImapBlock(
    MDICTX     *pCtx,
    uint32_t    ulImapNode)
{
    uint32_t    ulImapBlock = gpRedCoreVol->ulImapStartBN + (ulImapNode * 2U);

    if(RedBitGet(pCtx->abMREntries, ulImapNode))
    {
        ulImapBlock++;
    }

    return ulImapBlock;
}
#endif


/** @brief Compute the block number for an inode number.

    @param pCtx     Metadata iteration context structure.
    @param ulInode  The inode number.

    @return The block number for @p ulInode.  If the inode is free, BLOCK_SPARSE
            is returned.
*/
static uint32_t InodeBlock(
    MDICTX     *pCtx,
    uint32_t    ulInode)
{
    uint32_t    ulInodeOffset = (ulInode - INODE_FIRST_VALID) * 2U;
    uint32_t    ulInodeBlock = gpRedCoreVol->ulInodeTableStartBN + ulInodeOffset;

    if(gpRedCoreVol->fImapInline)
    {
      #if REDCONF_IMAP_INLINE == 1
        if(!RedBitGet(pCtx->abMREntries, ulInodeOffset))
        {
            if(RedBitGet(pCtx->abMREntries, ulInodeOffset + 1U))
            {
                ulInodeBlock++;
            }
            else
            {
                ulInodeBlock = BLOCK_SPARSE;
            }
        }
      #else
        REDERROR();
        ulInodeBlock = BLOCK_SPARSE;
      #endif
    }
    else
    {
      #if REDCONF_IMAP_EXTERNAL == 1
        uint32_t ulImapNode = ulInodeOffset / IMAPNODE_ENTRIES;
        uint32_t ulImapOffset = ulInodeOffset % IMAPNODE_ENTRIES;

        if(ulImapNode >= pCtx->ulInodeImapNodes)
        {
            REDERROR();
            ulInodeBlock = BLOCK_SPARSE;
        }
        else
        {
            const uint8_t *pbImapEntries = &pCtx->pbInodeImap[ulImapNode * IMAPNODE_ENTRY_BYTES];

            if(!RedBitGet(pbImapEntries, ulImapOffset))
            {
                if(RedBitGet(pbImapEntries, ulImapOffset + 1U))
                {
                    ulInodeBlock++;
                }
                else
                {
                    ulInodeBlock = BLOCK_SPARSE;
                }
            }
        }
      #else
        REDERROR();
        ulInodeBlock = BLOCK_SPARSE;
      #endif
    }

    return ulInodeBlock;
}


/** @brief Allocate an aligned block buffer.

    @param ppFreePtr    Populated with the pointer to free().

    @return On success, the aligned block buffer; on error, NULL.
*/
static void *AllocBlkBuf(
    void      **ppFreePtr)
{
    const uint32_t  ulAlign = sizeof(uint64_t);
    uint8_t        *pbBuffer = malloc(REDCONF_BLOCK_SIZE + (ulAlign - 1U));

    *ppFreePtr = pbBuffer;

    if(pbBuffer == NULL)
    {
        fprintf(stderr, "Error: failed to allocate memory\n");
        return NULL;
    }

    if(((uintptr_t)pbBuffer & (ulAlign - 1U)) != 0U)
    {
        pbBuffer += ulAlign - ((uintptr_t)pbBuffer & (ulAlign - 1U));
    }

    return pbBuffer;
}


/** @brief Extract the common node header from a metadata buffer.

    @param pBuffer  The metadata block buffer.

    @return The common node header (native endian).
*/
static NODEHEADER NodeHdrExtract(
    const void     *pBuffer)
{
    const uint8_t  *pbBuffer = pBuffer;
    NODEHEADER      hdr;

    RedMemCpy(&hdr.ulSignature, &pbBuffer[NODEHEADER_OFFSET_SIG], sizeof(hdr.ulSignature));

    /*  This utility is used by the endian tests, so handle the case where the
        disk endianness isn't the native endianness.
    */
    if(    (hdr.ulSignature == SWAP32(META_SIG_MASTER))
        || (hdr.ulSignature == SWAP32(META_SIG_METAROOT))
        || (hdr.ulSignature == SWAP32(META_SIG_IMAP))
        || (hdr.ulSignature == SWAP32(META_SIG_INODE))
        || (hdr.ulSignature == SWAP32(META_SIG_DINDIR))
        || (hdr.ulSignature == SWAP32(META_SIG_INDIR))
        || (hdr.ulSignature == SWAP32(META_SIG_DIRECTORY)))
    {
        hdr.ulSignature = SWAP32(hdr.ulSignature);
        MemCpyAndReverse(&hdr.ulCRC, &pbBuffer[NODEHEADER_OFFSET_CRC], sizeof(hdr.ulCRC));
        MemCpyAndReverse(&hdr.ullSequence, &pbBuffer[NODEHEADER_OFFSET_SEQ], sizeof(hdr.ullSequence));
    }
    else
    {
        RedMemCpy(&hdr.ulCRC, &pbBuffer[NODEHEADER_OFFSET_CRC], sizeof(hdr.ulCRC));
        RedMemCpy(&hdr.ullSequence, &pbBuffer[NODEHEADER_OFFSET_SEQ], sizeof(hdr.ullSequence));
    }

    return hdr;
}


/** @brief Like memcpy(), but reverse the @p pSrc bytes in @p pDst.

    @param pDst     Destination for copy.
    @param pSrc     Source for copy.
    @param ulLen    Number of bytes to copy and reverse.
*/
static void MemCpyAndReverse(
    void           *pDst,
    const void     *pSrc,
    uint32_t        ulLen)
{
    uint8_t        *pbDst = pDst;
    const uint8_t  *pbSrc = pSrc;
    uint32_t        i;

    for(i = 0U; i < ulLen; i++)
    {
        pbDst[i] = pbSrc[(ulLen - i) - 1U];
    }
}
