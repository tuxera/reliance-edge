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
static REDSTATUS RedVolMountMaster(uint32_t ulFlags);
static REDSTATUS RedVolMountMetaroot(uint32_t ulFlags);
#endif
static bool MetarootIsValid(METAROOT *pMR, bool *pfSectorCRCIsValid);
#if DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1)
static REDSTATUS ConcatOrphanLists(void);
#endif
#ifdef REDCONF_ENDIAN_SWAP
static void MetaRootEndianSwap(METAROOT *pMetaRoot);
#endif
#if REDCONF_OUTPUT == 1
static void OutputCriticalError(const char *pszFileName, uint32_t ulLineNum);
static uint32_t U32toStr(char *pcBuffer, uint32_t ulBufferLen, uint32_t ulNum);
#endif


/** @brief Populate and validate the volume geometry.

    The sector size and/or count will be queried from the block device if
    the volume configuration specifies that one or both are to be detected
    automatically.  Otherwise, the values in the volume configuration are used.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL Volume geometry is invalid.
*/
REDSTATUS RedVolInitBlockGeometry(void)
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
    else
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

        /*  Use the device block count initially, until the true volume block
            count is retrieved from the master block.
        */
        gpRedVolume->ulBlockCount = (uint32_t)(pBdevInfo->ullSectorCount >> gpRedVolume->bBlockSectorShift);
    }

    return ret;
}


/** @brief Populate the volume layout derived from the block and inode counts.

    `gpRedVolume->ulBlockCount` and `gpRedCoreVol->ulInodeCount` must be
    initialized by the caller before invoking this function.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL Volume geometry is invalid.
*/
REDSTATUS RedVolInitBlockLayout(void)
{
    REDSTATUS ret = 0;

    if(gpRedVolume->ulBlockCount < MINIMUM_METADATA_BLOCKS)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        /*  To understand the following code, note that the fixed-location
            metadata is located at the start of the disk, in the following
            order:

            - Master block (1 block)
            - Metaroots (2 blocks)
            - External imap blocks (variable * 2 blocks)
            - Inode blocks (pVolConf->ulInodeCount * 2 blocks)
        */

        /*  The imap needs bits for all inode and allocable blocks.  If that
            bitmap will fit into the metaroot, the inline imap is used and there
            are no imap nodes on disk.  The minus 3 is there since the imap does
            not include bits for the master block or metaroots.
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

            /*  The imap does not include bits for itself, so add two to the
                number of imap entries for the two blocks of each imap node.
                This allows us to divide up the remaining space, making sure to
                round up so all data blocks are covered.
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

    /*  Check for overflow.
    */
    if((ret == 0) && ((((uint64_t)gpRedCoreVol->ulInodeCount * 2U) + gpRedCoreVol->ulInodeTableStartBN) > UINT32_MAX))
    {
        ret = -RED_EINVAL;
    }

    if(ret == 0)
    {
        gpRedCoreVol->ulFirstAllocableBN = gpRedCoreVol->ulInodeTableStartBN + (gpRedCoreVol->ulInodeCount * 2U);

        if(gpRedCoreVol->ulFirstAllocableBN > gpRedVolume->ulBlockCount)
        {
            /*  We can get here if there is not enough space for the number of
                configured inodes.
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
            ret = RedVolInitBlockGeometry();

            if(ret == 0)
            {
                ret = RedVolMountMaster(ulFlags);
            }

            if(ret == 0)
            {
                ret = RedVolMountMetaroot(ulFlags);
            }

          #if DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1)
            if(ret == 0)
            {
                if((ulFlags & RED_MOUNT_SKIP_DELETE) == 0U)
                {
                    ret = RedVolFreeOrphans(UINT32_MAX);

                    if(ret == 0)
                    {
                        /*  At mount time, all orphans are defunct and should be
                            freed.
                        */
                        gpRedMR->ulDefunctOrphanHead = gpRedMR->ulOrphanHead;
                        gpRedMR->ulOrphanHead = INODE_INVALID;
                        gpRedMR->ulOrphanTail = INODE_INVALID;

                        ret = RedVolFreeOrphans(UINT32_MAX);
                    }
                }
                else if(gpRedMR->ulDefunctOrphanHead == INODE_INVALID)
                {
                    gpRedMR->ulDefunctOrphanHead = gpRedMR->ulOrphanHead;
                    gpRedMR->ulOrphanHead = INODE_INVALID;
                    gpRedMR->ulOrphanTail = INODE_INVALID;
                }
                else if(gpRedMR->ulOrphanHead != INODE_INVALID)
                {
                    /*  There are two lists, neither of which are empty, that
                        both contain inodes which were orphaned prior to mount.
                        However, the caller requested that we not free the
                        orphans during mount.  Combine the two lists into the
                        defunct list, so that new orphans have a home.
                    */
                    ret = ConcatOrphanLists();
                }
                else
                {
                    /*  There are orphans in the defunct list only, and the
                        caller has asked us not to free orphans at this time, so
                        there's nothing to do.
                    */
                }

                REDASSERT(    (gpRedMR->ulOrphanHead == INODE_INVALID)
                           == (gpRedMR->ulOrphanTail == INODE_INVALID));
            }
          #endif

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
REDSTATUS RedVolMountMaster(
    uint32_t        ulFlags)
{
    REDSTATUS       ret;
    MASTERBLOCK    *pMB;

    /*  Read the master block, to ensure that the disk was formatted with
        Reliance Edge.
    */
    ret = RedBufferGet(BLOCK_NUM_MASTER, BFLAG_META_MASTER, (void **)&pMB);

    if(ret == 0)
    {
        /*  Verify that the driver was compiled with the same settings that
            the disk was formatted with.  If not, the user has made a
            mistake: either the driver settings are wrong, or the disk needs
            to be reformatted.
        */
        if(    !RED_DISK_LAYOUT_IS_SUPPORTED(pMB->ulVersion)
            || (pMB->ulBlockCount > gpRedVolume->ulBlockCount)
            || (pMB->uMaxNameLen != REDCONF_NAME_MAX)
            || (pMB->uDirectPointers != REDCONF_DIRECT_POINTERS)
            || (pMB->uIndirectPointers != REDCONF_INDIRECT_POINTERS)
            || (pMB->bBlockSizeP2 != BLOCK_SIZE_P2)
            || (((pMB->bFlags & MBFLAG_API_POSIX) != 0U) != (REDCONF_API_POSIX == 1))
            || (((pMB->bFlags & MBFLAG_INODE_TIMESTAMPS) != 0U) != (REDCONF_INODE_TIMESTAMPS == 1))
            || (((pMB->bFlags & MBFLAG_INODE_BLOCKS) != 0U) != (REDCONF_INODE_BLOCKS == 1))
            || (((pMB->bFlags & MBFLAG_INODE_UIDGID) != 0U) != ((REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1)))
            || (((pMB->bFlags & MBFLAG_DELETE_OPEN) != 0U) != ((REDCONF_API_POSIX == 1) && (REDCONF_DELETE_OPEN == 1)))
            || ((pMB->uFeaturesIncompat & MBFEATURE_MASK_INCOMPAT) != 0U)
            || (    (pMB->ulVersion >= RED_DISK_LAYOUT_POSIXIER)
                 && (gaRedBdevInfo[gbRedVolNum].ulSectorSize != (1U << pMB->bSectorSizeP2))))
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

            /*  Save the on-disk layout version so we know how to interpret
                the metadata.
            */
            gpRedCoreVol->ulVersion = pMB->ulVersion;

            /*  gpRedVolume->ulBlockCount is currently the block count derived
                from the block device sector count but, on a mounted volume, it
                needs to be the block count of the volume.  These can be
                different values since we support mounting a volume which is
                smaller than the block device that it resides on.
            */
            gpRedVolume->ulBlockCount = pMB->ulBlockCount;

            gpRedCoreVol->ulInodeCount = pMB->ulInodeCount;

            /*  With the correct block and inode counts, the layout of the
                volume can now be computed.
            */
            ret = RedVolInitBlockLayout();

          #if REDCONF_READ_ONLY == 0
            if(ret == 0)
            {
                gpRedVolume->fReadOnly = (ulFlags & RED_MOUNT_READONLY) != 0U;

                /*  Check for feature flags that prevent this driver from
                    writing.
                */
                if(!gpRedVolume->fReadOnly && ((pMB->uFeaturesReadOnly & MBFEATURE_MASK_UNWRITEABLE) != 0U))
                {
                    ret = -RED_EROFS;
                }
            }
          #endif
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
    REDSTATUS   retMR0;
    REDSTATUS   retMR1;
    REDSTATUS   ret;

    retMR0 = RedIoRead(gbRedVolNum, BLOCK_NUM_FIRST_METAROOT, 1U, &gpRedCoreVol->aMR[0U]);
    retMR1 = RedIoRead(gbRedVolNum, BLOCK_NUM_FIRST_METAROOT + 1U, 1U, &gpRedCoreVol->aMR[1U]);

    ret = ((retMR0 == 0) || (retMR1 == 0)) ? 0 : retMR0;

    /*  Determine which metaroot is the most recent copy that was written
        completely.
    */
    if(ret == 0)
    {
        uint8_t bMR = UINT8_MAX;
        bool    fSectorCRCIsValid;

        if(retMR0 == 0)
        {
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
        }

        if((ret == 0) && (retMR1 == 0))
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
        const uint8_t  *pbMR = (const uint8_t *)pMR;
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

        ret = RedBufferFlushRange(0U, gpRedVolume->ulBlockCount);

        if(ret == 0)
        {
            gpRedMR->hdr.ulSignature = META_SIG_METAROOT;
            gpRedMR->hdr.ullSequence = gpRedVolume->ullSequence;

            ret = RedVolSeqNumIncrement(gbRedVolNum);
        }

        if(ret == 0)
        {
            const uint8_t  *pbMR = (const uint8_t *)gpRedMR;
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


/** @brief Rollback to the previous transaction point.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    An I/O error occurred.
*/
REDSTATUS RedVolRollback(void)
{
    REDSTATUS ret = 0;

    REDASSERT(gpRedVolume->fMounted); /* Should be checked by caller. */
    REDASSERT(!gpRedVolume->fReadOnly); /* Should be checked by caller. */

    if(gpRedCoreVol->fBranched)
    {
        uint32_t ulFlags = RED_MOUNT_DEFAULT;

        ret = RedBufferDiscardRange(0U, gpRedVolume->ulBlockCount);

        if(ret == 0)
        {
            ret = RedVolMountMaster(ulFlags);
        }

        if(ret == 0)
        {
            ret = RedVolMountMetaroot(ulFlags);
        }

        if(ret == 0)
        {
            gpRedCoreVol->fBranched = false;
        }

        CRITICAL_ASSERT(ret == 0);
    }

    return ret;
}
#endif /* REDCONF_READ_ONLY == 0 */


/** @brief Yields the number of currently available free blocks.

    Accounts for reserved blocks, subtracting the number of reserved blocks if
    they are unavailable.

    @return Number of currently available free blocks.
*/
uint32_t RedVolFreeBlockCount(void)
{
    uint32_t ulFreeBlocks = gpRedMR->ulFreeBlocks;

  #if RESERVED_BLOCKS > 0U
    if(!gpRedCoreVol->fUseReservedBlocks)
    {
        if(ulFreeBlocks >= RESERVED_BLOCKS)
        {
            ulFreeBlocks -= RESERVED_BLOCKS;
        }
        else
        {
            ulFreeBlocks = 0U;
        }
    }
  #endif

  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FRESERVE == 1)
    if(!gpRedCoreVol->fUseReservedInodeBlocks)
    {
        if(ulFreeBlocks < gpRedCoreVol->ulReservedInodeBlocks)
        {
            REDERROR();
            ulFreeBlocks = 0U;
        }
        else
        {
            ulFreeBlocks -= gpRedCoreVol->ulReservedInodeBlocks;
        }

        if(gpRedCoreVol->ulReservedInodes > 0U)
        {
            uint32_t ulBranchBlocks = gpRedCoreVol->ulReservedInodes * INODE_MAX_DEPTH;

            /*  The blocks set aside for freserve branching are, for simplicity,
                always reserved: even if they have already been branched.  If blocks
                are both reserved and branched, they are double-counted against free
                space, and so it's possible for this reserved count to be larger
                than remaining free space.
            */
            ulFreeBlocks -= REDMIN(ulFreeBlocks, ulBranchBlocks);
        }
    }
  #endif

    return ulFreeBlocks;
}


#if DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1)
/** @brief Free inodes which were orphaned before the most recent mount of the
           volume (defunct orphans).

    If there are fewer defunct orphans than were requested, all defunct orphans
    will be freed.

    @param ulCount  The maximum number of defunct orphans to free.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p ulCount is zero.
    @retval -RED_EIO    A disk I/O error occurred.
*/
REDSTATUS RedVolFreeOrphans(
    uint32_t    ulCount)
{
    REDSTATUS   ret = 0;

    if(ulCount == 0U)
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        uint32_t ulIdx;

        /*  Inode numbers are 32-bits, thus a count of UINT32_MAX will always be
            sufficient to include all defunct orphans.
        */
        for(ulIdx = 0U; ulIdx < ulCount; ulIdx++)
        {
            CINODE ino;

            ino.ulInode = gpRedMR->ulDefunctOrphanHead;
            ret = RedInodeMount(&ino, FTYPE_ANY, false);

            if(ret == 0)
            {
                uint32_t ulNextInode = ino.pInodeBuf->ulNextOrphan;

                ret = RedInodeFreeOrphan(&ino);

                if(ret == 0)
                {
                    gpRedMR->ulDefunctOrphanHead = ulNextInode;
                }
            }

            if(ret != 0)
            {
                break;
            }
        }

        /*  RED_EBADF is the only expected error, which can be returned by
            RedInodeMount() when we reach the end of the list.  However,
            RedInodeMount() will also return RED_EBADF for invalid inodes, which
            is a critical error.  Thus the special handling of RED_EBADF here.
        */
        if(ret == -RED_EBADF)
        {
            if(gpRedMR->ulDefunctOrphanHead == INODE_INVALID)
            {
                /*  The loop above does not look for the end of the list
                    (indicated by an orphan list value of INODE_INVALID). It
                    will instead call RedInodeMount() with the inode number
                    INODE_INVALID, which will return -RED_EBADF.  That condition
                    is not an error for this function because the count is a
                    maximum.
                */
                ret = 0;
            }
            else
            {
                /*  The loop above encountered an inode in the list that is not
                    valid.
                */
                CRITICAL_ERROR();
                ret = -RED_EFUBAR;
            }
        }
    }

    return ret;
}


/** @brief Concatenate the two lists of orphans.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS ConcatOrphanLists(void)
{
    CINODE ino;
    REDSTATUS ret;

    REDASSERT(gpRedMR->ulDefunctOrphanHead != INODE_INVALID);
    REDASSERT(gpRedMR->ulOrphanHead != INODE_INVALID);
    REDASSERT(gpRedMR->ulOrphanTail != INODE_INVALID);

    ino.ulInode = gpRedMR->ulOrphanTail;
    ret = RedInodeMount(&ino, FTYPE_ANY, true);

    if(ret == 0)
    {
        if(ino.pInodeBuf->ulNextOrphan != INODE_INVALID)
        {
            CRITICAL_ERROR();
            ret = -RED_EFUBAR;
        }
        else
        {
            ino.pInodeBuf->ulNextOrphan = gpRedMR->ulDefunctOrphanHead;
            gpRedMR->ulDefunctOrphanHead = gpRedMR->ulOrphanHead;

            gpRedMR->ulOrphanHead = INODE_INVALID;
            gpRedMR->ulOrphanTail = INODE_INVALID;
        }

        RedInodePut(&ino, 0U);
    }

    return ret;
}
#endif /* DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1) */


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
    /*  Unused in some configurations
    */
    (void)pszFileName;
    (void)ulLineNum;

  #if REDCONF_OUTPUT == 1
    OutputCriticalError(pszFileName, ulLineNum);
  #endif

  #if REDCONF_READ_ONLY == 0
    gpRedVolume->fReadOnly = true;
  #endif

  #if REDCONF_ASSERTS == 1
    RedOsAssertFail(pszFileName, ulLineNum);
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


#if REDCONF_OUTPUT == 1
/** @brief Output a critical error message.

    @param pszFileName  The file in which the error occurred.
    @param ulLineNum    The line number at which the error occurred.
*/
static void OutputCriticalError(
    const char *pszFileName,
    uint32_t    ulLineNum)
{
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

    /*  Print @p pszFileName and @p ulLineNum.
    */
    if(pszFileName == NULL)
    {
        REDERROR();
    }
    else
    {
        #define FILENAME_MAX_LEN 24U /* 2x the longest core source file name */
        #define LINENUM_MAX_LEN 10U /* Big enough for UINT32_MAX */
        #define PREFIX_LEN (sizeof(szPrefix) - 1U)
        #define OUTBUFSIZE (PREFIX_LEN + FILENAME_MAX_LEN + 1U /* ':' */ + LINENUM_MAX_LEN + 2U /* "\n\0" */)

        const char  szPrefix[] = "Reliance Edge critical error at ";
        char        szBuffer[OUTBUFSIZE];
        const char *pszBaseName;
        uint32_t    ulNameLen;
        uint32_t    ulIdx;

        /*  Many compilers include the path in __FILE__ strings.  szBuffer
            isn't large enough to print paths, so find the basename.
        */
        pszBaseName = pszFileName;
        ulIdx = 0U;
        while(pszFileName[ulIdx] != '\0')
        {
            /*  Currently it's safe to assume that the host system uses slashes
                as path separators.  On Unix-like hosts, a backslash is also a
                legal file name character, but we don't need to worry about that
                edge case, since only the last slash matters, and _our_ file
                names will never include backslashes.
            */
            if((pszFileName[ulIdx] == '/') || (pszFileName[ulIdx] == '\\'))
            {
                pszBaseName = &pszFileName[ulIdx + 1U];
            }

            ulIdx++;
        }

        ulNameLen = RedStrLen(pszBaseName);
        ulNameLen = REDMIN(ulNameLen, FILENAME_MAX_LEN); /* Paranoia */

        /*  We never use printf() in the core, for the sake of portability
            and minimal code size.  Instead, craft a string buffer for
            RedOsOutputString().

            E.g., "Reliance Edge critical error at file.c:123\n"
        */
        RedMemCpy(szBuffer, szPrefix, PREFIX_LEN);
        ulIdx = PREFIX_LEN;
        RedMemCpy(&szBuffer[ulIdx], pszBaseName, ulNameLen);
        ulIdx += ulNameLen;
        szBuffer[ulIdx] = ':';
        ulIdx++;
        ulIdx += U32toStr(&szBuffer[ulIdx], sizeof(szBuffer) - ulIdx, ulLineNum);
        szBuffer[ulIdx] = '\n';
        ulIdx++;
        szBuffer[ulIdx] = '\0';

        RedOsOutputString(szBuffer);
    }
}


/** @brief Output a decimal string representation of a uint32_t value.

    @note This function does _not_ null-terminate the output buffer.

    @param pcBuffer     The output buffer.
    @param ulBufferLen  The size of @p pcBuffer.
    @param ulNum        The unsigned 32-bit number.

    @return The number of bytes written to @p pcBuffer.
*/
static uint32_t U32toStr(
    char       *pcBuffer,
    uint32_t    ulBufferLen,
    uint32_t    ulNum)
{
    uint32_t    ulBufferIdx = 0U;

    if((pcBuffer == NULL) || (ulBufferLen == 0U))
    {
        REDERROR();
    }
    else
    {
        const char  szDigits[] = "0123456789";
        char        ach[10U]; /* Big enough for a uint32_t in radix 10 */
        uint32_t    ulDigits = 0U;
        uint32_t    ulRemaining = ulNum;

        /*  Compute the digit characters, from least significant to most
            significant.  For example, if @p ulNum is 123, the ach array
            is populated with "321".
        */
        do
        {
            ach[ulDigits] = szDigits[ulRemaining % 10U];
            ulRemaining /= 10U;
            ulDigits++;
        }
        while(ulRemaining > 0U);

        /*  Copy and reverse the string.  For example, if the ach array
            contains "321", then "123" is written to @p pcBuffer.
        */
        while((ulDigits > 0U) && (ulBufferIdx < ulBufferLen))
        {
            ulDigits--;
            pcBuffer[ulBufferIdx] = ach[ulDigits];
            ulBufferIdx++;
        }
    }

    return ulBufferIdx;
}
#endif /* REDCONF_OUTPUT == 1 */
