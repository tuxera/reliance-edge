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
    @brief Implements core volume operations.
*/
#include <redfs.h>
#include <redcore.h>
#include <redbdev.h>


/*  Minimum number of blocks needed for metadata on any volume: the master
    block (1), the two metaroots (2), and one doubly-allocated inode (2),
    resulting in 1 + 2 + 2 = 5.
*/
#define MINIMUM_METADATA_BLOCKS (5U)


#if REDCONF_CHECKER == 0
static REDSTATUS RedVolMountMaster(void);
static REDSTATUS RedVolMountMetaroot(uint32_t ulFlags);
#endif
static bool MetarootIsValid(METAROOT *pMR, bool *pfSectorCRCIsValid);
#ifdef REDCONF_ENDIAN_SWAP
static void MetaRootEndianSwap(METAROOT *pMetaRoot);
#endif


/** @brief Populate and validate the volume geometry.

    The sector size and/or count will be queried from the block device if
    the volume configuration specifies that one or both are to be detected
    automatically.  Otherwise, the values in the volume configuration are used.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL Volume geometry is invalid.
*/
REDSTATUS RedVolInitGeometry(void)
{
    REDSTATUS       ret = 0;
    const BDEVINFO *pBdevInfo = &gaRedBdevInfo[gbRedVolNum];

    if(    (pBdevInfo->ulSectorSize < SECTOR_SIZE_MIN)
        || ((REDCONF_BLOCK_SIZE % pBdevInfo->ulSectorSize) != 0U)
        || ((UINT64_MAX - gpRedVolConf->ullSectorOffset) < pBdevInfo->ullSectorCount)) /* SectorOffset + SectorCount must not wrap */
    {
        REDERROR();
        ret = -RED_EINVAL;
    }

    if(ret == 0)
    {
        gpRedVolume->bBlockSectorShift = 0U;
        while((pBdevInfo->ulSectorSize << gpRedVolume->bBlockSectorShift) < REDCONF_BLOCK_SIZE)
        {
            gpRedVolume->bBlockSectorShift++;
        }

        /*  This should always be true since the block size is confirmed to
            be a power of two (checked at compile time) and above we ensured
            that (REDCONF_BLOCK_SIZE % pVolConf->ulSectorSize) == 0.
        */
        REDASSERT((pBdevInfo->ulSectorSize << gpRedVolume->bBlockSectorShift) == REDCONF_BLOCK_SIZE);

        gpRedVolume->ulBlockCount = (uint32_t)(pBdevInfo->ullSectorCount >> gpRedVolume->bBlockSectorShift);

        if(gpRedVolume->ulBlockCount < MINIMUM_METADATA_BLOCKS)
        {
            ret = -RED_EINVAL;
        }
        else
        {
            /*  To understand the following code, note that the fixed-
                location metadata is located at the start of the disk, in
                the following order:

                - Master block (1 block)
                - Metaroots (2 blocks)
                - External imap blocks (variable * 2 blocks)
                - Inode blocks (pVolConf->ulInodeCount * 2 blocks)
            */

            /*  The imap needs bits for all inode and allocable blocks.  If
                that bitmap will fit into the metaroot, the inline imap is
                used and there are no imap nodes on disk.  The minus 3 is
                there since the imap does not include bits for the master
                block or metaroots.
            */
            gpRedCoreVol->fImapInline = (gpRedVolume->ulBlockCount - 3U) <= METAROOT_ENTRIES;

            if(gpRedCoreVol->fImapInline)
            {
              #if REDCONF_IMAP_INLINE == 1
                gpRedCoreVol->ulInodeTableStartBN = 3U;
              #else
                REDERROR();
                ret = -RED_EINVAL;
              #endif
            }
            else
            {
              #if REDCONF_IMAP_EXTERNAL == 1
                gpRedCoreVol->ulImapStartBN = 3U;

                /*  The imap does not include bits for itself, so add two to
                    the number of imap entries for the two blocks of each
                    imap node.  This allows us to divide up the remaining
                    space, making sure to round up so all data blocks are
                    covered.
                */
                gpRedCoreVol->ulImapNodeCount =
                    ((gpRedVolume->ulBlockCount - 3U) + ((IMAPNODE_ENTRIES + 2U) - 1U)) / (IMAPNODE_ENTRIES + 2U);

                gpRedCoreVol->ulInodeTableStartBN = gpRedCoreVol->ulImapStartBN + (gpRedCoreVol->ulImapNodeCount * 2U);
              #else
                REDERROR();
                ret = -RED_EINVAL;
              #endif
            }
        }
    }

    if(ret == 0)
    {
        gpRedCoreVol->ulFirstAllocableBN = gpRedCoreVol->ulInodeTableStartBN + (gpRedVolConf->ulInodeCount * 2U);

        if(gpRedCoreVol->ulFirstAllocableBN > gpRedVolume->ulBlockCount)
        {
            /*  We can get here if there is not enough space for the number
                of configured inodes.
            */
            ret = -RED_EINVAL;
        }
        else
        {
            gpRedVolume->ulBlocksAllocable = gpRedVolume->ulBlockCount - gpRedCoreVol->ulFirstAllocableBN;
        }
    }

    return ret;
}


/** @brief Mount a file system volume.

    @param ulFlags  A bitwise-OR'd mask of mount flags.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p ulFlags includes invalid mount flags.
    @retval -RED_EIO    Volume not formatted, improperly formatted, or corrupt.
*/
REDSTATUS RedVolMount(
    uint32_t    ulFlags)
{
    REDSTATUS   ret;

    if(ulFlags != (ulFlags & RED_MOUNT_MASK))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        BDEVOPENMODE mode = BDEV_O_RDONLY;

      #if REDCONF_READ_ONLY == 0
        if((ulFlags & RED_MOUNT_READONLY) == 0U)
        {
            mode = BDEV_O_RDWR;
        }
      #endif

        ret = RedBDevOpen(gbRedVolNum, mode);

        if(ret == 0)
        {
            ret = RedVolInitGeometry();

            if(ret == 0)
            {
                ret = RedVolMountMaster();
            }

            if(ret == 0)
            {
                ret = RedVolMountMetaroot(ulFlags);
            }

            if(ret != 0)
            {
                /*  If we fail to mount, invalidate the buffers to prevent any
                    confusion that could be caused by stale or corrupt metadata.
                */
                (void)RedBufferDiscardRange(0U, gpRedVolume->ulBlockCount);
                (void)RedBDevClose(gbRedVolNum);
            }
        }
    }

    return ret;
}


/** @brief Mount the master block.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    Master block missing, corrupt, or inconsistent with the
                        compile-time driver settings.
*/
#if REDCONF_CHECKER == 0
static
#endif
REDSTATUS RedVolMountMaster(void)
{
    REDSTATUS       ret;
    MASTERBLOCK    *pMB;

    /*  Read the master block, to ensure that the disk was formatted with
        Reliance Edge.
    */
    ret = RedBufferGet(BLOCK_NUM_MASTER, BFLAG_META_MASTER, CAST_VOID_PTR_PTR(&pMB));

    if(ret == 0)
    {
        /*  Verify that the driver was compiled with the same settings that
            the disk was formatted with.  If not, the user has made a
            mistake: either the driver settings are wrong, or the disk needs
            to be reformatted.
        */
        if(    (pMB->ulVersion != RED_DISK_LAYOUT_VERSION)
            || (pMB->ulInodeCount != gpRedVolConf->ulInodeCount)
            || (pMB->ulBlockCount != gpRedVolume->ulBlockCount)
            || (pMB->uMaxNameLen != REDCONF_NAME_MAX)
            || (pMB->uDirectPointers != REDCONF_DIRECT_POINTERS)
            || (pMB->uIndirectPointers != REDCONF_INDIRECT_POINTERS)
            || (pMB->bBlockSizeP2 != BLOCK_SIZE_P2)
            || (((pMB->bFlags & MBFLAG_API_POSIX) != 0U) != (REDCONF_API_POSIX == 1))
            || (((pMB->bFlags & MBFLAG_INODE_TIMESTAMPS) != 0U) != (REDCONF_INODE_TIMESTAMPS == 1))
            || (((pMB->bFlags & MBFLAG_INODE_BLOCKS) != 0U) != (REDCONF_INODE_BLOCKS == 1)))
        {
            ret = -RED_EIO;
        }
      #if REDCONF_API_POSIX == 1
        else if(((pMB->bFlags & MBFLAG_INODE_NLINK) != 0U) != (REDCONF_API_POSIX_LINK == 1))
        {
            ret = -RED_EIO;
        }
      #else
        else if((pMB->bFlags & MBFLAG_INODE_NLINK) != 0U)
        {
            ret = -RED_EIO;
        }
      #endif
        else
        {
            /*  Master block configuration is valid.

                Save the sequence number of the master block in the volume,
                since we need it later (see RedVolMountMetaroot()) and we do
                not want to re-buffer the master block.
            */
            gpRedVolume->ullSequence = pMB->hdr.ullSequence;
        }

        RedBufferPut(pMB);
    }

    return ret;
}


/** @brief Mount the latest metaroot.

    This function also populates the volume contexts.

    @param ulFlags  A bitwise-OR'd mask of mount flags.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    Both metaroots are missing or corrupt.
*/
#if REDCONF_CHECKER == 0
static
#endif
REDSTATUS RedVolMountMetaroot(
    uint32_t    ulFlags)
{
    REDSTATUS   ret;

    ret = RedIoRead(gbRedVolNum, BLOCK_NUM_FIRST_METAROOT, 1U, &gpRedCoreVol->aMR[0U]);

    if(ret == 0)
    {
        ret = RedIoRead(gbRedVolNum, BLOCK_NUM_FIRST_METAROOT + 1U, 1U, &gpRedCoreVol->aMR[1U]);
    }

    /*  Determine which metaroot is the most recent copy that was written
        completely.
    */
    if(ret == 0)
    {
        uint8_t bMR = UINT8_MAX;
        bool    fSectorCRCIsValid;

        if(MetarootIsValid(&gpRedCoreVol->aMR[0U], &fSectorCRCIsValid))
        {
            bMR = 0U;

          #ifdef REDCONF_ENDIAN_SWAP
            MetaRootEndianSwap(&gpRedCoreVol->aMR[0U]);
          #endif
        }
        else if(gpRedVolConf->fAtomicSectorWrite && !fSectorCRCIsValid)
        {
            ret = -RED_EIO;
        }
        else
        {
            /*  Metaroot is not valid, so it is ignored and there's nothing
                to do here.
            */
        }

        if(ret == 0)
        {
            if(MetarootIsValid(&gpRedCoreVol->aMR[1U], &fSectorCRCIsValid))
            {
              #ifdef REDCONF_ENDIAN_SWAP
                MetaRootEndianSwap(&gpRedCoreVol->aMR[1U]);
              #endif

                if((bMR != 0U) || (gpRedCoreVol->aMR[1U].hdr.ullSequence > gpRedCoreVol->aMR[0U].hdr.ullSequence))
                {
                    bMR = 1U;
                }
            }
            else if(gpRedVolConf->fAtomicSectorWrite && !fSectorCRCIsValid)
            {
                ret = -RED_EIO;
            }
            else
            {
                /*  Metaroot is not valid, so it is ignored and there's nothing
                    to do here.
                */
            }
        }

        if(ret == 0)
        {
            if(bMR == UINT8_MAX)
            {
                /*  Neither metaroot was valid.
                */
                ret = -RED_EIO;
            }
            else
            {
                gpRedCoreVol->bCurMR = bMR;
                gpRedMR = &gpRedCoreVol->aMR[bMR];
            }
        }
    }

    if(ret == 0)
    {
        /*  Normally the metaroot contains the highest sequence number, but the
            master block is the last block written during format, so on a
            freshly formatted volume the master block sequence number (stored in
            gpRedVolume->ullSequence) will be higher than that in the metaroot.
        */
        if(gpRedMR->hdr.ullSequence > gpRedVolume->ullSequence)
        {
            gpRedVolume->ullSequence = gpRedMR->hdr.ullSequence;
        }

        /*  gpRedVolume->ullSequence stores the *next* sequence number; to avoid
            giving the next node written to disk the same sequence number as the
            metaroot, increment it here.
        */
        ret = RedVolSeqNumIncrement(gbRedVolNum);
    }

    if(ret == 0)
    {
        gpRedVolume->fMounted = true;
      #if REDCONF_READ_ONLY == 0
        gpRedVolume->fReadOnly = (ulFlags & RED_MOUNT_READONLY) != 0U;
      #endif

      #if RESERVED_BLOCKS > 0U
        gpRedCoreVol->fUseReservedBlocks = false;
      #endif
        gpRedCoreVol->ulAlmostFreeBlocks = 0U;

        gpRedCoreVol->aMR[1U - gpRedCoreVol->bCurMR] = *gpRedMR;
        gpRedCoreVol->bCurMR = 1U - gpRedCoreVol->bCurMR;
        gpRedMR = &gpRedCoreVol->aMR[gpRedCoreVol->bCurMR];
    }

    return ret;
}


/** @brief Determine whether the metaroot is valid.

    @param pMR                  The metaroot buffer.
    @param pfSectorCRCIsValid   Populated with whether the first sector of the
                                metaroot buffer is valid.

    @return Whether the metaroot is valid.

    @retval true    The metaroot buffer is valid.
    @retval false   The metaroot buffer is invalid.
*/
static bool MetarootIsValid(
    METAROOT   *pMR,
    bool       *pfSectorCRCIsValid)
{
    bool        fRet = false;

    if(pfSectorCRCIsValid == NULL)
    {
        REDERROR();
    }
    else if(pMR == NULL)
    {
        REDERROR();
        *pfSectorCRCIsValid = false;
    }
  #ifdef REDCONF_ENDIAN_SWAP
    else if(RedRev32(pMR->hdr.ulSignature) != META_SIG_METAROOT)
  #else
    else if(pMR->hdr.ulSignature != META_SIG_METAROOT)
  #endif
    {
        *pfSectorCRCIsValid = false;
    }
    else
    {
        const uint8_t  *pbMR = CAST_VOID_PTR_TO_CONST_UINT8_PTR(pMR);
        uint32_t        ulSectorSize = gaRedBdevInfo[gbRedVolNum].ulSectorSize;
        uint32_t        ulSectorCRC = pMR->ulSectorCRC;
        uint32_t        ulCRC;

      #ifdef REDCONF_ENDIAN_SWAP
        ulSectorCRC = RedRev32(ulSectorCRC);
      #endif

        /*  The sector CRC was zero when the CRC was computed during the
            transaction, so it must be zero here.
        */
        pMR->ulSectorCRC = 0U;

        ulCRC = RedCrc32Update(0U, &pbMR[8U], ulSectorSize - 8U);

        fRet = ulCRC == ulSectorCRC;
        *pfSectorCRCIsValid = fRet;

        if(fRet)
        {
            ulCRC = RedCrc32Update(ulCRC, &pbMR[ulSectorSize], REDCONF_BLOCK_SIZE - ulSectorSize);

          #ifdef REDCONF_ENDIAN_SWAP
            ulCRC = RedRev32(ulCRC);
          #endif

            fRet = ulCRC == pMR->hdr.ulCRC;
        }
    }

    return fRet;
}


#if REDCONF_READ_ONLY == 0
/** @brief Commit a transaction point.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
REDSTATUS RedVolTransact(void)
{
    REDSTATUS ret = 0;

    REDASSERT(!gpRedVolume->fReadOnly); /* Should be checked by caller. */

    if(gpRedCoreVol->fBranched)
    {
        gpRedMR->ulFreeBlocks += gpRedCoreVol->ulAlmostFreeBlocks;
        gpRedCoreVol->ulAlmostFreeBlocks = 0U;

        ret = RedBufferFlush(0U, gpRedVolume->ulBlockCount);

        if(ret == 0)
        {
            gpRedMR->hdr.ulSignature = META_SIG_METAROOT;
            gpRedMR->hdr.ullSequence = gpRedVolume->ullSequence;

            ret = RedVolSeqNumIncrement(gbRedVolNum);
        }

        if(ret == 0)
        {
            const uint8_t  *pbMR = CAST_VOID_PTR_TO_CONST_UINT8_PTR(gpRedMR);
            uint32_t        ulSectorSize = gaRedBdevInfo[gbRedVolNum].ulSectorSize;
            uint32_t        ulSectorCRC;

          #ifdef REDCONF_ENDIAN_SWAP
            MetaRootEndianSwap(gpRedMR);
          #endif

            gpRedMR->ulSectorCRC = 0U;

            ulSectorCRC = RedCrc32Update(0U, &pbMR[8U], ulSectorSize - 8U);

            if(ulSectorSize < REDCONF_BLOCK_SIZE)
            {
                gpRedMR->hdr.ulCRC = RedCrc32Update(ulSectorCRC, &pbMR[ulSectorSize], REDCONF_BLOCK_SIZE - ulSectorSize);
            }
            else
            {
                gpRedMR->hdr.ulCRC = ulSectorCRC;
            }

            gpRedMR->ulSectorCRC = ulSectorCRC;

          #ifdef REDCONF_ENDIAN_SWAP
            gpRedMR->hdr.ulCRC = RedRev32(gpRedMR->hdr.ulCRC);
            gpRedMR->ulSectorCRC = RedRev32(gpRedMR->ulSectorCRC);
          #endif

            /*  Flush the block device before writing the metaroot, so that all
                previously written blocks are guaranteed to be on the media before
                the metaroot is written.  Otherwise, if the block device reorders
                the writes, the metaroot could reach the media before metadata it
                points at, creating a window for disk corruption if power is lost.
            */
            ret = RedIoFlush(gbRedVolNum);
        }

        if(ret == 0)
        {
            ret = RedIoWrite(gbRedVolNum, BLOCK_NUM_FIRST_METAROOT + gpRedCoreVol->bCurMR, 1U, gpRedMR);

          #ifdef REDCONF_ENDIAN_SWAP
            MetaRootEndianSwap(gpRedMR);
          #endif
        }

        /*  Flush the block device to force the metaroot write to the media.  This
            guarantees the transaction point is really complete before we return.
        */
        if(ret == 0)
        {
            ret = RedIoFlush(gbRedVolNum);
        }

        /*  Toggle to the other metaroot buffer.  The working state and committed
            state metaroot buffers exchange places.
        */
        if(ret == 0)
        {
            uint8_t bNextMR = 1U - gpRedCoreVol->bCurMR;

            gpRedCoreVol->aMR[bNextMR] = *gpRedMR;
            gpRedCoreVol->bCurMR = bNextMR;

            gpRedMR = &gpRedCoreVol->aMR[gpRedCoreVol->bCurMR];

            gpRedCoreVol->fBranched = false;
        }

        CRITICAL_ASSERT(ret == 0);
    }

    return ret;
}
#endif


#ifdef REDCONF_ENDIAN_SWAP
static void MetaRootEndianSwap(
    METAROOT *pMetaRoot)
{
    if(pMetaRoot == NULL)
    {
        REDERROR();
    }
    else
    {
        pMetaRoot->hdr.ulSignature = RedRev32(pMetaRoot->hdr.ulSignature);
        pMetaRoot->hdr.ullSequence = RedRev64(pMetaRoot->hdr.ullSequence);

        pMetaRoot->ulSectorCRC = RedRev32(pMetaRoot->ulSectorCRC);
        pMetaRoot->ulFreeBlocks = RedRev32(pMetaRoot->ulFreeBlocks);
      #if REDCONF_API_POSIX == 1
        pMetaRoot->ulFreeInodes = RedRev32(pMetaRoot->ulFreeInodes);
      #endif
        pMetaRoot->ulAllocNextBlock = RedRev32(pMetaRoot->ulAllocNextBlock);
    }
}
#endif


/** @brief Process a critical file system error.

    @param pszFileName  The file in which the error occurred.
    @param ulLineNum    The line number at which the error occurred.
*/
void RedVolCriticalError(
    const char *pszFileName,
    uint32_t    ulLineNum)
{
  #if REDCONF_OUTPUT == 1
  #if REDCONF_READ_ONLY == 0
    if(!gpRedVolume->fReadOnly)
    {
        RedOsOutputString("Critical file system error in Reliance Edge, setting volume to READONLY\n");
    }
    else
  #endif
    {
        RedOsOutputString("Critical file system error in Reliance Edge (volume already READONLY)\n");
    }
  #endif

  #if REDCONF_READ_ONLY == 0
    gpRedVolume->fReadOnly = true;
  #endif

  #if REDCONF_ASSERTS == 1
    RedOsAssertFail(pszFileName, ulLineNum);
  #else
    (void)pszFileName;
    (void)ulLineNum;
  #endif
}


/** @brief Increment the sequence number.

    @param bVolNum  Volume number of the volume whose sequence number is to be
                    incremented.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL Cannot increment sequence number: maximum value reached.
                        This should not ever happen.
*/
REDSTATUS RedVolSeqNumIncrement(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;

    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else if(gaRedVolume[bVolNum].ullSequence == UINT64_MAX)
    {
        /*  In practice this should never, ever happen; to get here, there would
            need to be UINT64_MAX disk writes, which would take eons: longer
            than the lifetime of any product or storage media.  If this assert
            fires and the current year is still written with four digits,
            suspect memory corruption.
        */
        CRITICAL_ERROR();
        ret = -RED_EFUBAR;
    }
    else
    {
        gaRedVolume[bVolNum].ullSequence++;
        ret = 0;
    }

    return ret;
}

