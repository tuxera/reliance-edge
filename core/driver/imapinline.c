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
    @brief Implements routines for the inline imap.

    The inline imap is used on volumes that are small enough for the imap bitmap
    to be entirely contained within the metaroot.
*/
#include <redfs.h>

#if REDCONF_IMAP_INLINE == 1

#include <redcore.h>


/** @brief Get the allocation bit of a block from either metaroot.

    @param bMR          The metaroot index: either 0 or 1.
    @param ulBlock      The block number to query.
    @param pfAllocated  On successful return, populated with the allocation bit
                        of the block.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bMR is out of range; or @p ulBlock is out of range;
                        @p pfAllocated is `NULL`; or the current volume does not
                        use the inline imap.
*/
REDSTATUS RedImapIBlockGet(
    uint8_t     bMR,
    uint32_t    ulBlock,
    bool       *pfAllocated)
{
    REDSTATUS   ret;

    if(    (!gpRedCoreVol->fImapInline)
        || (bMR > 1U)
        || (ulBlock < gpRedCoreVol->ulInodeTableStartBN)
        || (ulBlock >= gpRedVolume->ulBlockCount)
        || (pfAllocated == NULL))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        *pfAllocated = RedBitGet(gpRedCoreVol->aMR[bMR].abEntries, ulBlock - gpRedCoreVol->ulInodeTableStartBN);
        ret = 0;
    }

    return ret;
}


#if REDCONF_READ_ONLY == 0
/** @brief Set the allocation bit of a block in the working metaroot.

    @param ulBlock      The block number to allocate or free.
    @param fAllocated   Whether to allocate the block (true) or free it (false).

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p ulBlock is out of range; or the current volume does
                        not use the inline imap.
*/
REDSTATUS RedImapIBlockSet(
    uint32_t    ulBlock,
    bool        fAllocated)
{
    REDSTATUS   ret;

    if(    (!gpRedCoreVol->fImapInline)
        || (ulBlock < gpRedCoreVol->ulInodeTableStartBN)
        || (ulBlock >= gpRedVolume->ulBlockCount))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        uint32_t ulOffset = ulBlock - gpRedCoreVol->ulInodeTableStartBN;

        if(RedBitGet(gpRedMR->abEntries, ulOffset) == fAllocated)
        {
            /*  The driver shouldn't ever set a bit in the imap to its current
                value.  This is more of a problem with the external imap, but it
                is checked here for consistency.
            */
            CRITICAL_ERROR();
            ret = -RED_EFUBAR;
        }
        else if(fAllocated)
        {
            RedBitSet(gpRedMR->abEntries, ulOffset);
            ret = 0;
        }
        else
        {
            RedBitClear(gpRedMR->abEntries, ulOffset);
            ret = 0;
        }
    }

    return ret;
}


/** @brief Scan the imap for a free block.

    @param ulBlock      The block at which to start the search.
    @param pulFreeBlock On success, populated with the found free block.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p ulBlock is out of range; or @p pulFreeBlock is
                        `NULL`.
    @retval -RED_ENOSPC No free block was found.
*/
REDSTATUS RedImapIBlockFindFree(
    uint32_t    ulBlock,
    uint32_t   *pulFreeBlock)
{
    REDSTATUS   ret;

    if(    (!gpRedCoreVol->fImapInline)
        || (ulBlock < gpRedCoreVol->ulFirstAllocableBN)
        || (ulBlock >= gpRedVolume->ulBlockCount)
        || (pulFreeBlock == NULL))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        const uint8_t  *pbBmpCurMR = gpRedCoreVol->aMR[gpRedCoreVol->bCurMR].abEntries;
        const uint8_t  *pbBmpCmtMR = gpRedCoreVol->aMR[1U - gpRedCoreVol->bCurMR].abEntries;
        uint32_t        ulSearchBlock = ulBlock;

        /*  Default to an ENOSPC error, which will be returned if we search
            every allocable block without finding a free block.
        */
        ret = -RED_ENOSPC;

        do
        {
            /*  Blocks before the inode table aren't included in the bitmap.
            */
            uint32_t ulBmpIdx = ulSearchBlock - gpRedCoreVol->ulInodeTableStartBN;

            /*  As an optimization to reduce the number of RedBitGet() calls, if
                all eight blocks in the current byte are allocated, then skip
                to the next byte.
            */
            if(((ulBmpIdx & 7U) == 0U) && (pbBmpCurMR[ulBmpIdx >> 3U] == UINT8_MAX))
            {
                ulSearchBlock += REDMIN(8U, gpRedVolume->ulBlockCount - ulSearchBlock);
            }
            else
            {
                /*  If the block is free in the working state...
                */
                if(!RedBitGet(pbBmpCurMR, ulBmpIdx))
                {
                    /*  If the block is free in the committed state...
                    */
                    if(!RedBitGet(pbBmpCmtMR, ulBmpIdx))
                    {
                        /*  Found a free block.
                        */
                        *pulFreeBlock = ulSearchBlock;
                        ret = 0;
                        break;
                    }
                }

                ulSearchBlock++;
            }

            if(ulSearchBlock == gpRedVolume->ulBlockCount)
            {
                ulSearchBlock = gpRedCoreVol->ulFirstAllocableBN;
            }
        } while(ulSearchBlock != ulBlock);
    }

    return ret;
}
#endif /* REDCONF_READ_ONLY == 0 */

#endif /* REDCONF_IMAP_INLINE == 1 */

