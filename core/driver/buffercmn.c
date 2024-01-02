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
    @brief Common (shared) buffer module functions.
*/
#include <redfs.h>
#include <redcore.h>
#include "redbufferpriv.h"


#ifdef REDCONF_ENDIAN_SWAP
static void BufferEndianSwapHeader(NODEHEADER *pHeader);
static void BufferEndianSwapMaster(MASTERBLOCK *pMaster);
static void BufferEndianSwapInode(INODE *pInode);
#if REDCONF_DIRECT_POINTERS < INODE_ENTRIES
static void BufferEndianSwapIndir(INDIR *pIndir);
#endif
#endif


/** Determine whether a metadata buffer is valid.

    This includes checking its signature, CRC, and sequence number.

    @param pbBuffer Pointer to the metadata buffer to validate.
    @param uFlags   The buffer flags provided by the caller.  Used to determine
                    the expected signature.

    @return Whether the metadata buffer is valid.

    @retval true    The metadata buffer is valid.
    @retval false   The metadata buffer is invalid.
*/
bool RedBufferIsValid(
    const uint8_t  *pbBuffer,
    uint16_t        uFlags)
{
    bool            fValid;

    if((pbBuffer == NULL) || ((uFlags & BFLAG_MASK) != uFlags))
    {
        REDERROR();
        fValid = false;
    }
    else
    {
        const NODEHEADER   *pHdr = (const NODEHEADER *)pbBuffer;
        uint16_t            uMetaFlags = uFlags & BFLAG_META_MASK;
      #ifdef REDCONF_ENDIAN_SWAP
        NODEHEADER          bufSwap;

        bufSwap.ulCRC = RedRev32(pHdr->ulCRC);
        bufSwap.ulSignature = RedRev32(pHdr->ulSignature);
        bufSwap.ullSequence = RedRev64(pHdr->ullSequence);
        pHdr = &bufSwap;
      #endif

        /*  Make sure the signature is correct for the type of metadata block
            requested by the caller.
        */
        switch(pHdr->ulSignature)
        {
            case META_SIG_MASTER:
                fValid = (uMetaFlags == BFLAG_META_MASTER);
                break;
          #if REDCONF_IMAP_EXTERNAL == 1
            case META_SIG_IMAP:
                fValid = (uMetaFlags == BFLAG_META_IMAP);
                break;
          #endif
            case META_SIG_INODE:
                fValid = (uMetaFlags == BFLAG_META_INODE);
                break;
          #if DINDIR_POINTERS > 0U
            case META_SIG_DINDIR:
                fValid = (uMetaFlags == BFLAG_META_DINDIR);
                break;
          #endif
          #if REDCONF_DIRECT_POINTERS < INODE_ENTRIES
            case META_SIG_INDIR:
                fValid = (uMetaFlags == BFLAG_META_INDIR);
                break;
          #endif
          #if REDCONF_API_POSIX == 1
            case META_SIG_DIRECTORY:
                fValid = (uMetaFlags == BFLAG_META_DIRECTORY);
                break;
          #endif
            default:
                fValid = false;
                break;
        }

        if(fValid)
        {
            uint32_t ulComputedCrc;

            /*  Check for disk corruption by comparing the stored CRC with one
                computed from the data.

                Also check the sequence number: if it is greater than the
                current sequence number, the block is from a previous format
                or the disk is writing blocks out of order.  During mount,
                before the metaroots have been read, the sequence number will
                be unknown, and the check is skipped.
            */
            ulComputedCrc = RedCrcNode(pbBuffer);
            if(pHdr->ulCRC != ulComputedCrc)
            {
                fValid = false;
            }
            else if(gpRedVolume->fMounted && (pHdr->ullSequence >= gpRedVolume->ullSequence))
            {
                fValid = false;
            }
            else
            {
                /*  Buffer is valid.  No action, fValid is already true.
                */
            }
        }
    }

    return fValid;
}


#if REDCONF_READ_ONLY == 0
/** @brief Finalize a metadata buffer.

    This updates the CRC and the sequence number.  It also sets the signature,
    though this is only truly needed if the buffer is new.

    @param pbBuffer Pointer to the metadata buffer to finalize.
    @param bVolNum  The volume number for the metadata buffer.
    @param uFlags   The associated buffer flags.  Used to determine the expected
                    signature.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL Invalid parameter; or maximum sequence number reached.
*/
REDSTATUS RedBufferFinalize(
    uint8_t    *pbBuffer,
    uint8_t     bVolNum,
    uint16_t    uFlags)
{
    REDSTATUS   ret = 0;

    if((pbBuffer == NULL) || (bVolNum >= REDCONF_VOLUME_COUNT) || ((uFlags & BFLAG_MASK) != uFlags))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        uint32_t ulSignature;

        switch(uFlags & BFLAG_META_MASK)
        {
            case BFLAG_META_MASTER:
                ulSignature = META_SIG_MASTER;
                break;
          #if REDCONF_IMAP_EXTERNAL == 1
            case BFLAG_META_IMAP:
                ulSignature = META_SIG_IMAP;
                break;
          #endif
            case BFLAG_META_INODE:
                ulSignature = META_SIG_INODE;
                break;
          #if DINDIR_POINTERS > 0U
            case BFLAG_META_DINDIR:
                ulSignature = META_SIG_DINDIR;
                break;
          #endif
          #if REDCONF_DIRECT_POINTERS < INODE_ENTRIES
            case BFLAG_META_INDIR:
                ulSignature = META_SIG_INDIR;
                break;
          #endif
          #if REDCONF_API_POSIX == 1
            case BFLAG_META_DIRECTORY:
                ulSignature = META_SIG_DIRECTORY;
                break;
          #endif
            default:
                ulSignature = 0U;
                break;
        }

        if(ulSignature == 0U)
        {
            REDERROR();
            ret = -RED_EINVAL;
        }
        else
        {
            uint64_t ullSeqNum = gaRedVolume[bVolNum].ullSequence;

            ret = RedVolSeqNumIncrement(bVolNum);
            if(ret == 0)
            {
                uint32_t ulCrc;

                RedMemCpy(&pbBuffer[NODEHEADER_OFFSET_SIG], &ulSignature, sizeof(ulSignature));
                RedMemCpy(&pbBuffer[NODEHEADER_OFFSET_SEQ], &ullSeqNum, sizeof(ullSeqNum));

              #ifdef REDCONF_ENDIAN_SWAP
                RedBufferEndianSwap(pbBuffer, uFlags);
              #endif

                ulCrc = RedCrcNode(pbBuffer);
              #ifdef REDCONF_ENDIAN_SWAP
                ulCrc = RedRev32(ulCrc);
              #endif
                RedMemCpy(&pbBuffer[NODEHEADER_OFFSET_CRC], &ulCrc, sizeof(ulCrc));
            }
        }
    }

    return ret;
}
#endif /* REDCONF_READ_ONLY == 0 */


#ifdef REDCONF_ENDIAN_SWAP
/** @brief Swap the byte order of a metadata buffer

    Does nothing if the buffer is not a metadata node.  Also does nothing for
    meta roots, which don't go through the buffers anyways.

    @param pBuffer  Pointer to the metadata buffer to swap
    @param uFlags   The associated buffer flags.  Used to determine the type of
                    metadata node.
*/
void RedBufferEndianSwap(
    void       *pBuffer,
    uint16_t    uFlags)
{
    if((pBuffer == NULL) || ((uFlags & BFLAG_MASK) != uFlags))
    {
        REDERROR();
    }
    else if((uFlags & BFLAG_META_MASK) != 0)
    {
        BufferEndianSwapHeader(pBuffer);

        switch(uFlags & BFLAG_META_MASK)
        {
            case BFLAG_META_MASTER:
                BufferEndianSwapMaster(pBuffer);
                break;
            case BFLAG_META_INODE:
                BufferEndianSwapInode(pBuffer);
                break;
          #if DINDIR_POINTERS > 0U
            case BFLAG_META_DINDIR:
                BufferEndianSwapIndir(pBuffer);
                break;
          #endif
          #if REDCONF_DIRECT_POINTERS < INODE_ENTRIES
            case BFLAG_META_INDIR:
                BufferEndianSwapIndir(pBuffer);
                break;
          #endif
            default:
                /*  The metadata node doesn't require endian swaps outside the
                    header.
                */
                break;
        }
    }
    else
    {
        /*  File data buffers do not need to be swapped.
        */
    }
}


/** @brief Swap the byte order of a metadata node header

    @param pHeader  Pointer to the metadata node header to swap
*/
static void BufferEndianSwapHeader(
    NODEHEADER *pHeader)
{
    if(pHeader == NULL)
    {
        REDERROR();
    }
    else
    {
        pHeader->ulSignature = RedRev32(pHeader->ulSignature);
        pHeader->ulCRC = RedRev32(pHeader->ulCRC);
        pHeader->ullSequence = RedRev64(pHeader->ullSequence);
    }
}


/** @brief Swap the byte order of a master block

    @param pMaster  Pointer to the master block to swap
*/
static void BufferEndianSwapMaster(
    MASTERBLOCK *pMaster)
{
    if(pMaster == NULL)
    {
        REDERROR();
    }
    else
    {
        pMaster->ulVersion = RedRev32(pMaster->ulVersion);
        pMaster->ulFormatTime = RedRev32(pMaster->ulFormatTime);
        pMaster->ulInodeCount = RedRev32(pMaster->ulInodeCount);
        pMaster->ulBlockCount = RedRev32(pMaster->ulBlockCount);
        pMaster->uMaxNameLen = RedRev16(pMaster->uMaxNameLen);
        pMaster->uDirectPointers = RedRev16(pMaster->uDirectPointers);
        pMaster->uIndirectPointers = RedRev16(pMaster->uIndirectPointers);
    }
}


/** @brief Swap the byte order of an inode

    @param pInode   Pointer to the inode to swap
*/
static void BufferEndianSwapInode(
    INODE  *pInode)
{
    if(pInode == NULL)
    {
        REDERROR();
    }
    else
    {
        uint32_t ulIdx;

        pInode->ullSize = RedRev64(pInode->ullSize);

      #if REDCONF_INODE_BLOCKS == 1
        pInode->ulBlocks = RedRev32(pInode->ulBlocks);
      #endif

      #if REDCONF_INODE_TIMESTAMPS == 1
        pInode->ulATime = RedRev32(pInode->ulATime);
        pInode->ulMTime = RedRev32(pInode->ulMTime);
        pInode->ulCTime = RedRev32(pInode->ulCTime);
      #endif

        pInode->uMode = RedRev16(pInode->uMode);

      #if (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_LINK == 1)
        pInode->uNLink = RedRev16(pInode->uNLink);
      #endif

      #if REDCONF_API_POSIX == 1
        pInode->ulPInode = RedRev32(pInode->ulPInode);
      #endif

        for(ulIdx = 0; ulIdx < INODE_ENTRIES; ulIdx++)
        {
            pInode->aulEntries[ulIdx] = RedRev32(pInode->aulEntries[ulIdx]);
        }
    }
}


#if REDCONF_DIRECT_POINTERS < INODE_ENTRIES
/** @brief Swap the byte order of an indirect or double indirect node

    @param pIndir   Pointer to the node to swap
*/
static void BufferEndianSwapIndir(
    INDIR  *pIndir)
{
    if(pIndir == NULL)
    {
        REDERROR();
    }
    else
    {
        uint32_t ulIdx;

        pIndir->ulInode = RedRev32(pIndir->ulInode);

        for(ulIdx = 0; ulIdx < INDIR_ENTRIES; ulIdx++)
        {
            pIndir->aulEntries[ulIdx] = RedRev32(pIndir->aulEntries[ulIdx]);
        }
    }
}
#endif /* REDCONF_DIRECT_POINTERS < INODE_ENTRIES */
#endif /* #ifdef REDCONF_ENDIAN_SWAP */
