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
    @brief Implements the Reliance Edge file system formatter.
*/
#include <redfs.h>
#include <redcoreapi.h>
#include <redcore.h>
#include <redbdev.h>


#if FORMAT_SUPPORTED

/*  The master block has a field for the build number, however this edition
    of Reliance Edge does not have build numbers.  Populate the field with a
    placeholder value.
*/
#define PLACEHOLDER_BUILD_NUMBER "0"


static uint32_t ComputeInodeCount(void);


/** @brief Format a file system volume.

    @param pOptions Format options.  May be `NULL`, in which case the default
                    values are used for the options.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EBUSY  Volume is mounted.
    @retval -RED_EIO    A disk I/O error occurred.
*/
REDSTATUS RedVolFormat(
    const REDFMTOPT *pOptions)
{
    static const REDFMTOPT ZeroOpts = {0U};

    REDSTATUS   ret = 0;
    REDSTATUS   ret2;
    REDFMTOPT   opts = (pOptions == NULL) ? ZeroOpts : *pOptions;
    bool        fBDevOpen = false;
    bool        fGeoInited = false;

    if(opts.ulVersion == 0U)
    {
        /*  Version 0 means to use the default.
        */
        opts.ulVersion = RED_DISK_LAYOUT_VERSION;
    }
    else if(!RED_DISK_LAYOUT_IS_SUPPORTED(opts.ulVersion))
    {
        /*  Return an error if the version number is invalid or if it is not
            supported by the compile-time configuration of the formatter and
            driver.
        */
        ret = -RED_EINVAL;
    }
    else
    {
        /*  Use the specified on-disk layout.
        */
    }

    if(ret == 0)
    {
        if(gpRedVolume->fMounted)
        {
            ret = -RED_EBUSY;
        }
        else
        {
            ret = RedBDevOpen(gbRedVolNum, BDEV_O_RDWR);
            fBDevOpen = (ret == 0);
        }
    }

    if(ret == 0)
    {
        ret = RedVolInitBlockGeometry();
        fGeoInited = (ret == 0);
    }

    if(ret == 0)
    {
        uint32_t ulInodeCount;

        if(opts.ulInodeCount == RED_FORMAT_INODE_COUNT_CONFIG)
        {
            ulInodeCount = gpRedVolConf->ulInodeCount;
        }
        else if(opts.ulInodeCount == RED_FORMAT_INODE_COUNT_AUTO)
        {
            ulInodeCount = INODE_COUNT_AUTO;
        }
        else
        {
            ulInodeCount = opts.ulInodeCount;
        }

        if(ulInodeCount == INODE_COUNT_AUTO)
        {
            ulInodeCount = ComputeInodeCount();
        }

        gpRedCoreVol->ulVersion = opts.ulVersion;
        gpRedCoreVol->ulInodeCount = ulInodeCount;

        /*  fReadOnly might still be true from the last time the volume was
            mounted (or from the checker).  Clear it now to avoid assertions in
            lower-level code.
        */
        gpRedVolume->fReadOnly = false;

        ret = RedVolInitBlockLayout();
    }

    if(ret == 0)
    {
        MASTERBLOCK *pMB;

        /*  Overwrite the master block with zeroes, so that if formatting is
            interrupted, the volume will not be mountable.
        */
        ret = RedBufferGet(BLOCK_NUM_MASTER, BFLAG_NEW | BFLAG_DIRTY, (void **)&pMB);

        if(ret == 0)
        {
            ret = RedBufferFlushRange(BLOCK_NUM_MASTER, 1U);

            RedBufferDiscard(pMB);
        }
    }

    if(ret == 0)
    {
        ret = RedIoFlush(gbRedVolNum);
    }

  #if REDCONF_IMAP_EXTERNAL == 1
    if((ret == 0) && !gpRedCoreVol->fImapInline)
    {
        uint32_t ulImapBlock;
        uint32_t ulImapBlockLimit = gpRedCoreVol->ulImapStartBN + (gpRedCoreVol->ulImapNodeCount * 2U);
        uint16_t uImapFlags = (uint16_t)((uint32_t)BFLAG_META_IMAP | BFLAG_NEW | BFLAG_DIRTY);

        /*  Technically it is only necessary to create one copy of each imap
            node (the copy the metaroot points at), but creating them both
            avoids headaches during disk image analysis from stale imaps left
            over from previous formats.
        */
        for(ulImapBlock = gpRedCoreVol->ulImapStartBN; ulImapBlock < ulImapBlockLimit; ulImapBlock++)
        {
            IMAPNODE   *pImap;

            ret = RedBufferGet(ulImapBlock, uImapFlags, (void **)&pImap);
            if(ret != 0)
            {
                break;
            }

            RedBufferPut(pImap);
        }
    }
  #endif

    /*  Write the first metaroot.
    */
    if(ret == 0)
    {
        RedMemSet(gpRedMR, 0U, sizeof(*gpRedMR));

        gpRedMR->ulFreeBlocks = gpRedVolume->ulBlocksAllocable;
      #if REDCONF_API_POSIX == 1
        gpRedMR->ulFreeInodes = gpRedCoreVol->ulInodeCount;
      #endif
        gpRedMR->ulAllocNextBlock = gpRedCoreVol->ulFirstAllocableBN;

        /*  The branched flag is typically set automatically when bits in the
            imap change.  It is set here explicitly because the imap has only
            been initialized, not changed.
        */
        gpRedCoreVol->fBranched = true;

        ret = RedVolTransact();
    }

  #if REDCONF_API_POSIX == 1
    /*  Create the root directory.
    */
    if(ret == 0)
    {
        CINODE rootdir;

        rootdir.ulInode = INODE_ROOTDIR;
        ret = RedInodeCreate(&rootdir, NULL, RED_S_IFDIR | (RED_S_IRWXUGO & RED_S_IFVALID));

        if(ret == 0)
        {
            RedInodePut(&rootdir, 0U);
        }
    }
  #endif

  #if REDCONF_API_FSE == 1
    /*  The FSE API does not support creating or deleting files, so all the
        inodes are created during setup.
    */
    if(ret == 0)
    {
        uint32_t ulInodeIdx;

        for(ulInodeIdx = 0U; ulInodeIdx < gpRedCoreVol->ulInodeCount; ulInodeIdx++)
        {
            CINODE ino;

            ino.ulInode = INODE_FIRST_FREE + ulInodeIdx;
            ret = RedInodeCreate(&ino, NULL, RED_S_IFREG);

            if(ret == 0)
            {
                RedInodePut(&ino, 0U);
            }
        }
    }
  #endif

    /*  Write the second metaroot.
    */
    if(ret == 0)
    {
        ret = RedVolTransact();
    }

    /*  Populate and write out the master block.
    */
    if(ret == 0)
    {
        MASTERBLOCK *pMB;

        ret = RedBufferGet(BLOCK_NUM_MASTER, (uint16_t)((uint32_t)BFLAG_META_MASTER | BFLAG_NEW | BFLAG_DIRTY), (void **)&pMB);
        if(ret == 0)
        {
            pMB->ulVersion = opts.ulVersion;
            RedStrNCpy(pMB->acBuildNum, PLACEHOLDER_BUILD_NUMBER, sizeof(pMB->acBuildNum));
            pMB->ulFormatTime = RedOsClockGetTime();
            pMB->ulInodeCount = gpRedCoreVol->ulInodeCount;
            pMB->ulBlockCount = gpRedVolume->ulBlockCount;
            pMB->uMaxNameLen = REDCONF_NAME_MAX;
            pMB->uDirectPointers = REDCONF_DIRECT_POINTERS;
            pMB->uIndirectPointers = REDCONF_INDIRECT_POINTERS;
            pMB->bBlockSizeP2 = BLOCK_SIZE_P2;

          #if REDCONF_API_POSIX == 1
            pMB->bFlags |= MBFLAG_API_POSIX;
          #endif
          #if REDCONF_INODE_TIMESTAMPS == 1
            pMB->bFlags |= MBFLAG_INODE_TIMESTAMPS;
          #endif
          #if REDCONF_INODE_BLOCKS == 1
            pMB->bFlags |= MBFLAG_INODE_BLOCKS;
          #endif
          #if (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_LINK == 1)
            pMB->bFlags |= MBFLAG_INODE_NLINK;
          #endif
          #if (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1)
            pMB->bFlags |= MBFLAG_INODE_UIDGID;
          #endif
          #if (REDCONF_API_POSIX == 1) && (REDCONF_DELETE_OPEN == 1)
            pMB->bFlags |= MBFLAG_DELETE_OPEN;
          #endif

          #if (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_SYMLINK == 1)
            pMB->uFeaturesReadOnly |= MBFEATURE_SYMLINK;
          #endif

            if(pMB->ulVersion >= RED_DISK_LAYOUT_POSIXIER)
            {
                pMB->bSectorSizeP2 = 1U;
                while((1U << pMB->bSectorSizeP2) < gaRedBdevInfo[gbRedVolNum].ulSectorSize)
                {
                    pMB->bSectorSizeP2++;
                }

                REDASSERT((1U << pMB->bSectorSizeP2) == gaRedBdevInfo[gbRedVolNum].ulSectorSize);
            }

            ret = RedBufferFlushRange(BLOCK_NUM_MASTER, 1U);

            RedBufferPut(pMB);
        }
    }

    if(ret == 0)
    {
        ret = RedIoFlush(gbRedVolNum);
    }

    if(fBDevOpen)
    {
        ret2 = RedBDevClose(gbRedVolNum);
        if(ret == 0)
        {
            ret = ret2;
        }
    }

    if(fGeoInited)
    {
        /*  Discard the buffers so a subsequent format will not run into blocks
            it does not expect.
        */
        ret2 = RedBufferDiscardRange(0U, gpRedVolume->ulBlockCount);
        if(ret == 0)
        {
            ret = ret2;
        }
    }

    return ret;
}


/** @brief Compute a reasonable number of inodes for the current volume.

    @return The computed inode count.
*/
static uint32_t ComputeInodeCount(void)
{
    uint32_t ulInodeBlocksMax;
    uint32_t ulInodeBlocks;
    uint32_t ulInodeCountMin;
    uint32_t ulInodeCount;

    /*  Compute the maximum number of blocks that an inode can consume.
    */
    ulInodeBlocksMax = (uint32_t)REDMIN(UINT32_MAX, UINT64_SUFFIX(2) + INODE_DATA_BLOCKS + REDCONF_INDIRECT_POINTERS + (DINDIR_POINTERS * INDIR_ENTRIES));

    /*  Compute an absolute minimum, such that there are enough inodes to
        conceivably consume all allocable blocks.
    */
    ulInodeCountMin = gpRedVolume->ulBlockCount / ulInodeBlocksMax;

    if((gpRedVolume->ulBlockCount % ulInodeBlocksMax) != 0U)
    {
        ulInodeCountMin++;
    }

    /*  Allow 16 allocable blocks for each inode, plus the two inode blocks.
    */
    ulInodeBlocks = 16U + 2U;
    ulInodeCount = gpRedVolume->ulBlockCount / ulInodeBlocks;

    return REDMAX(ulInodeCountMin, ulInodeCount);
}

#endif /* FORMAT_SUPPORTED */
