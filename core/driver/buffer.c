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
    @brief Implements the block device buffering system.

    This module implements the block buffer cache.  It has a number of block
    sized buffers which are used to store data from a given block (identified
    by both block number and volume number: this cache is shared among all
    volumes).  Block buffers may be either dirty or clean.  Most I/O passes
    through this module.  When a buffer is needed for a block which is not in
    the cache, a "victim" is selected via a simple LRU scheme.
*/
#include <redfs.h>
#include <redcore.h>
#include "redbufferpriv.h"

#if BUFFER_MODULE == BM_SIMPLE


#if REDCONF_BUFFER_COUNT > 255U
#error "REDCONF_BUFFER_COUNT cannot be greater than 255"
#endif

/*  This implementation does not support the write-gather buffer.
*/
#if REDCONF_BUFFER_WRITE_GATHER_SIZE_KB != 0U
  #error "Configuration error: REDCONF_BUFFER_WRITE_GATHER_SIZE_KB must be zero"
#endif


/** @brief Convert a buffer index into a block buffer pointer.
*/
#define BIDX2BUF(idx) (&gBufCtx.pbBlkBuf[(uint32_t)(idx) << BLOCK_SIZE_P2])


/** @brief Metadata stored for each block buffer.

    To make better use of CPU caching when searching the BUFFERHEAD array, this
    structure should be kept small.
*/
typedef struct
{
    uint32_t    ulBlock;    /**< Block number the buffer is associated with; BBLK_INVALID if unused. */
    uint8_t     bVolNum;    /**< Volume the block resides on. */
    uint8_t     bRefCount;  /**< Number of references. */
    uint16_t    uFlags;     /**< Buffer flags: mask of BFLAG_* values. */
} BUFFERHEAD;


/** @brief State information for the block buffer module.
*/
typedef struct
{
    /** Number of buffers which are referenced (have a bRefCount > 0).
    */
    uint16_t    uNumUsed;

    /** MRU array.  Each element of the array stores a buffer index; each buffer
        index appears in the array once and only once.  The first element of the
        array is the most-recently-used (MRU) buffer, followed by the next most
        recently used, and so on, till the last element, which is the least-
        recently-used (LRU) buffer.
    */
    uint8_t     abMRU[REDCONF_BUFFER_COUNT];

    /** Buffer heads, storing metadata for each buffer.
    */
    BUFFERHEAD  aHead[REDCONF_BUFFER_COUNT];

    /** Byte array used as the heap for the block buffers.
    */
    uint8_t     abBlkHeap[(REDCONF_BUFFER_ALIGNMENT - 1U) + (REDCONF_BUFFER_COUNT * REDCONF_BLOCK_SIZE)];

    /** Pointer to the start of the block buffers.  This points into the
        abBlkHeap array, skipping over the initial bytes if necessary for the
        block buffers to be aligned.  Each block-sized chunk of this buffer
        is associated with the corresponding element in the aHead array.
    */
    uint8_t    *pbBlkBuf;
} BUFFERCTX;


static bool BufferToIdx(const void *pBuffer, uint8_t *pbIdx);
#if REDCONF_READ_ONLY == 0
static REDSTATUS BufferWrite(uint8_t bIdx);
#endif
static void BufferMakeLRU(uint8_t bIdx);
static void BufferMakeMRU(uint8_t bIdx);
static bool BufferFind(uint32_t ulBlock, uint8_t *pbIdx);


static BUFFERCTX gBufCtx;


/** @brief Initialize the buffers.
*/
void RedBufferInit(void)
{
    uint8_t bIdx;

    RedMemSet(&gBufCtx, 0U, sizeof(gBufCtx));

    for(bIdx = 0U; bIdx < REDCONF_BUFFER_COUNT; bIdx++)
    {
        /*  When the buffers have been freshly initialized, acquire the buffers
            in the order in which they appear in the array.
        */
        gBufCtx.abMRU[bIdx] = (uint8_t)((REDCONF_BUFFER_COUNT - bIdx) - 1U);
        gBufCtx.aHead[bIdx].ulBlock = BBLK_INVALID;
    }

    /*  Get an aligned pointer for the block buffers.
    */
    gBufCtx.pbBlkBuf = UINT8_PTR_ALIGN(gBufCtx.abBlkHeap, REDCONF_BUFFER_ALIGNMENT);
}


/** @brief Acquire a buffer.

    @param ulBlock  Block number to acquire.
    @param uFlags   BFLAG_ values for the operation.
    @param ppBuffer On success, populated with the acquired buffer.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
    @retval -RED_EINVAL Invalid parameters.
    @retval -RED_EBUSY  All buffers are referenced.
*/
REDSTATUS RedBufferGet(
    uint32_t    ulBlock,
    uint16_t    uFlags,
    void      **ppBuffer)
{
    REDSTATUS   ret = 0;
    uint8_t     bIdx;

    if(    (ulBlock >= gpRedVolume->ulBlockCount)
        || ((uFlags & BFLAG_MASK) != uFlags)
        || (((uFlags & BFLAG_NEW) != 0U) && ((uFlags & BFLAG_DIRTY) == 0U))
        || !BFLAG_TYPE_IS_VALID(uFlags)
        || (ppBuffer == NULL))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        if(BufferFind(ulBlock, &bIdx))
        {
            /*  Error if the buffer exists and BFLAG_NEW was specified, since
                the new flag is used when a block is newly allocated/created, so
                the block was previously free and and there should never be an
                existing buffer for a free block.

                Error if the buffer exists but does not have the same type as
                was requested.
            */
            if(    ((uFlags & BFLAG_NEW) != 0U)
                || ((uFlags & BFLAG_META_MASK) != (gBufCtx.aHead[bIdx].uFlags & BFLAG_META_MASK)))
            {
                CRITICAL_ERROR();
                ret = -RED_EFUBAR;
            }
        }
        else if(gBufCtx.uNumUsed == REDCONF_BUFFER_COUNT)
        {
            /*  The MINIMUM_BUFFER_COUNT is supposed to ensure that no operation
                ever runs out of buffers, so this should never happen.
            */
            CRITICAL_ERROR();
            ret = -RED_EBUSY;
        }
        else
        {
            BUFFERHEAD *pHead;

            /*  Search for the least recently used buffer which is not
                referenced.
            */
            for(bIdx = (uint8_t)(REDCONF_BUFFER_COUNT - 1U); bIdx > 0U; bIdx--)
            {
                if(gBufCtx.aHead[gBufCtx.abMRU[bIdx]].bRefCount == 0U)
                {
                    break;
                }
            }

            bIdx = gBufCtx.abMRU[bIdx];
            pHead = &gBufCtx.aHead[bIdx];

            if(pHead->bRefCount == 0U)
            {
                /*  If the LRU buffer is valid and dirty, write it out before
                    repurposing it.
                */
                if(((pHead->uFlags & BFLAG_DIRTY) != 0U) && (pHead->ulBlock != BBLK_INVALID))
                {
                  #if REDCONF_READ_ONLY == 1
                    CRITICAL_ERROR();
                    ret = -RED_EFUBAR;
                  #else
                    ret = BufferWrite(bIdx);
                  #endif
                }
            }
            else
            {
                /*  All the buffers are used, which should have been caught by
                    checking gBufCtx.uNumUsed.
                */
                CRITICAL_ERROR();
                ret = -RED_EBUSY;
            }

            if(ret == 0)
            {
                uint8_t *pbBuffer = BIDX2BUF(bIdx);

                if((uFlags & BFLAG_NEW) == 0U)
                {
                    /*  Invalidate the LRU buffer.  If the read fails, we do not
                        want the buffer head to continue to refer to the old
                        block number, since the read, even if it fails, may have
                        partially overwritten the buffer data (consider the case
                        where block size exceeds sector size, and some but not
                        all of the sectors are read successfully), and if the
                        buffer were to be used subsequently with its partially
                        erroneous contents, bad things could happen.
                    */
                    pHead->ulBlock = BBLK_INVALID;

                    ret = RedIoRead(gbRedVolNum, ulBlock, 1U, pbBuffer);

                    if((ret == 0) && ((uFlags & BFLAG_META) != 0U))
                    {
                        if(!RedBufferIsValid(pbBuffer, uFlags))
                        {
                            /*  A corrupt metadata node is usually a critical
                                error.  The master block is an exception since
                                it might be invalid because the volume is not
                                mounted; that condition is expected and should
                                not result in an assertion.
                            */
                            CRITICAL_ASSERT((uFlags & BFLAG_META_MASTER) == BFLAG_META_MASTER);
                            ret = -RED_EIO;
                        }
                    }

                  #ifdef REDCONF_ENDIAN_SWAP
                    if(ret == 0)
                    {
                        RedBufferEndianSwap(pbBuffer, uFlags);
                    }
                  #endif
                }
                else
                {
                    RedMemSet(pbBuffer, 0U, REDCONF_BLOCK_SIZE);
                }
            }

            if(ret == 0)
            {
                pHead->bVolNum = gbRedVolNum;
                pHead->ulBlock = ulBlock;
                pHead->uFlags = 0U;
            }
        }

        /*  Reference the buffer, update its flags, and promote it to MRU.  This
            happens both when BufferFind() found an existing buffer for the
            block and when the LRU buffer was repurposed to create a buffer for
            the block.
        */
        if(ret == 0)
        {
            BUFFERHEAD *pHead = &gBufCtx.aHead[bIdx];

            pHead->bRefCount++;

            if(pHead->bRefCount == 1U)
            {
                gBufCtx.uNumUsed++;
            }

            /*  BFLAG_NEW tells this function to zero the buffer instead of
                reading it from disk; it has no meaning later on, and thus is
                not saved.
            */
            pHead->uFlags |= (uFlags & (~BFLAG_NEW));

            BufferMakeMRU(bIdx);

            *ppBuffer = BIDX2BUF(bIdx);
        }
    }

    return ret;
}


/** @brief Release a buffer.

    @param pBuffer  The buffer to release.
 */
void RedBufferPut(
    const void *pBuffer)
{
    uint8_t     bIdx;

    if(!BufferToIdx(pBuffer, &bIdx))
    {
        REDERROR();
    }
    else
    {
        REDASSERT(gBufCtx.aHead[bIdx].bRefCount > 0U);
        gBufCtx.aHead[bIdx].bRefCount--;

        if(gBufCtx.aHead[bIdx].bRefCount == 0U)
        {
            REDASSERT(gBufCtx.uNumUsed > 0U);
            gBufCtx.uNumUsed--;
        }
    }
}


#if REDCONF_READ_ONLY == 0
/** @brief Flush all buffers for the active volume in the given range of blocks.

    @param ulBlockStart Starting block number to flush.
    @param ulBlockCount Count of blocks, starting at @p ulBlockStart, to flush.
                        Must not be zero.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
    @retval -RED_EINVAL Invalid parameters.
*/
REDSTATUS RedBufferFlushRange(
    uint32_t    ulBlockStart,
    uint32_t    ulBlockCount)
{
    REDSTATUS   ret = 0;

    if(    (ulBlockStart >= gpRedVolume->ulBlockCount)
        || ((gpRedVolume->ulBlockCount - ulBlockStart) < ulBlockCount)
        || (ulBlockCount == 0U))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        uint8_t bIdx;

        for(bIdx = 0U; bIdx < REDCONF_BUFFER_COUNT; bIdx++)
        {
            BUFFERHEAD *pHead = &gBufCtx.aHead[bIdx];

            if(    (pHead->bVolNum == gbRedVolNum)
                && (pHead->ulBlock != BBLK_INVALID)
                && ((pHead->uFlags & BFLAG_DIRTY) != 0U)
                && (pHead->ulBlock >= ulBlockStart)
                && (pHead->ulBlock < (ulBlockStart + ulBlockCount)))
            {
                ret = BufferWrite(bIdx);

                if(ret == 0)
                {
                    pHead->uFlags &= (~BFLAG_DIRTY);
                }
                else
                {
                    break;
                }
            }
        }
    }

    return ret;
}


/** @brief Mark a buffer dirty.

    @param pBuffer  The buffer to mark dirty.
*/
void RedBufferDirty(
    const void *pBuffer)
{
    uint8_t     bIdx;

    if(!BufferToIdx(pBuffer, &bIdx))
    {
        REDERROR();
    }
    else
    {
        REDASSERT(gBufCtx.aHead[bIdx].bRefCount > 0U);

        gBufCtx.aHead[bIdx].uFlags |= BFLAG_DIRTY;
    }
}


/** @brief Branch a buffer, marking it dirty and assigning a new block number.

    @param pBuffer      The buffer to branch.
    @param ulBlockNew   The new block number for the buffer.
*/
void RedBufferBranch(
    const void *pBuffer,
    uint32_t    ulBlockNew)
{
    uint8_t     bIdx;

    if(    !BufferToIdx(pBuffer, &bIdx)
        || (ulBlockNew >= gpRedVolume->ulBlockCount))
    {
        REDERROR();
    }
    else
    {
        BUFFERHEAD *pHead = &gBufCtx.aHead[bIdx];

        REDASSERT(pHead->bRefCount > 0U);
        REDASSERT((pHead->uFlags & BFLAG_DIRTY) == 0U);

        pHead->uFlags |= BFLAG_DIRTY;
        pHead->ulBlock = ulBlockNew;
    }
}


#if (REDCONF_API_POSIX == 1) || FORMAT_SUPPORTED
/** @brief Discard a buffer, releasing it and marking it invalid.

    @param pBuffer  The buffer to discard.
*/
void RedBufferDiscard(
    const void *pBuffer)
{
    uint8_t     bIdx;

    if(!BufferToIdx(pBuffer, &bIdx))
    {
        REDERROR();
    }
    else
    {
        REDASSERT(gBufCtx.aHead[bIdx].bRefCount == 1U);
        REDASSERT(gBufCtx.uNumUsed > 0U);

        gBufCtx.aHead[bIdx].bRefCount = 0U;
        gBufCtx.aHead[bIdx].ulBlock = BBLK_INVALID;

        gBufCtx.uNumUsed--;

        BufferMakeLRU(bIdx);
    }
}
#endif
#endif /* REDCONF_READ_ONLY == 0 */


/** @brief Discard a range of buffers, marking them invalid.

    @param ulBlockStart The starting block number to discard
    @param ulBlockCount The number of blocks, starting at @p ulBlockStart, to
                        discard.  Must not be zero.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL Invalid parameters.
    @retval -RED_EBUSY  A block in the desired range is referenced.
*/
REDSTATUS RedBufferDiscardRange(
    uint32_t    ulBlockStart,
    uint32_t    ulBlockCount)
{
    REDSTATUS   ret = 0;

    if(    (ulBlockStart >= gpRedVolume->ulBlockCount)
        || ((gpRedVolume->ulBlockCount - ulBlockStart) < ulBlockCount)
        || (ulBlockCount == 0U))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        uint8_t bIdx;

        for(bIdx = 0U; bIdx < REDCONF_BUFFER_COUNT; bIdx++)
        {
            BUFFERHEAD *pHead = &gBufCtx.aHead[bIdx];

            if(    (pHead->bVolNum == gbRedVolNum)
                && (pHead->ulBlock != BBLK_INVALID)
                && (pHead->ulBlock >= ulBlockStart)
                && (pHead->ulBlock < (ulBlockStart + ulBlockCount)))
            {
                if(pHead->bRefCount == 0U)
                {
                    pHead->ulBlock = BBLK_INVALID;

                    BufferMakeLRU(bIdx);
                }
                else
                {
                    /*  This should never happen.  There are three general cases
                        when this function is used:

                        1) Discarding every block, as happens during unmount
                           and at the end of format.  There should no longer be
                           any referenced buffers at those points.
                        2) Discarding a block which has become free.  All
                           buffers for such blocks should be put or branched
                           beforehand.
                        3) Discarding of blocks that were just written straight
                           to disk, leaving stale data in the buffer.  The write
                           code should never reference buffers for these blocks,
                           since they would not be needed or used.
                    */
                    CRITICAL_ERROR();
                    ret = -RED_EBUSY;
                    break;
                }
            }
        }
    }

    return ret;
}


/** @brief Read a range of data, either from the buffers or from disk.

    @param ulBlockStart The first block number to read.
    @param ulBlockCount The number of blocks, starting at @p ulBlockStart, to be
                        read.  Must not be zero.
    @param pbDataBuffer The buffer to read into.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL Invalid parameters.
    @retval -RED_EIO    A disk I/O error occurred.
*/
REDSTATUS RedBufferReadRange(
    uint32_t    ulBlockStart,
    uint32_t    ulBlockCount,
    uint8_t    *pbDataBuffer)
{
    REDSTATUS   ret = 0;

    if(    (ulBlockStart >= gpRedVolume->ulBlockCount)
        || ((gpRedVolume->ulBlockCount - ulBlockStart) < ulBlockCount)
        || (ulBlockCount == 0U)
        || (pbDataBuffer == NULL))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
      #if REDCONF_READ_ONLY == 0
        /*  If there are any dirty buffers in the range, it would be erroneous
            to return stale data from the disk, so flush dirty buffers prior to
            reading from disk.
        */
        ret = RedBufferFlushRange(ulBlockStart, ulBlockCount);
        if(ret == 0)
      #endif
        {
            /*  This implementation always reads directly from disk, bypassing
                the buffers.
            */
            ret = RedIoRead(gbRedVolNum, ulBlockStart, ulBlockCount, pbDataBuffer);
        }
    }

    return ret;
}


#if REDCONF_READ_ONLY == 0
/** @brief Write a range of data, either to the buffers or to disk.

    @param ulBlockStart The first block number to write.
    @param ulBlockCount The number of blocks, starting at @p ulBlockStart, to be
                        written.  Must not be zero.
    @param pbDataBuffer The buffer containing the data to be written.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EBUSY  A block in the desired range is referenced.
    @retval -RED_EINVAL Invalid parameters.
    @retval -RED_EIO    A disk I/O error occurred.
*/
REDSTATUS RedBufferWriteRange(
    uint32_t        ulBlockStart,
    uint32_t        ulBlockCount,
    const uint8_t  *pbDataBuffer)
{
    REDSTATUS       ret = 0;

    if(    (ulBlockStart >= gpRedVolume->ulBlockCount)
        || ((gpRedVolume->ulBlockCount - ulBlockStart) < ulBlockCount)
        || (ulBlockCount == 0U)
        || (pbDataBuffer == NULL))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        /*  This implementation always writes directly to disk, bypassing the
            buffers.
        */
        ret = RedIoWrite(gbRedVolNum, ulBlockStart, ulBlockCount, pbDataBuffer);
        if(ret == 0)
        {
            /*  If there is any buffered data for the blocks we just wrote,
                those buffers are now stale.
            */
            ret = RedBufferDiscardRange(ulBlockStart, ulBlockCount);
        }
    }

    return ret;
}
#endif


/** @brief Derive the index of the buffer.

    @param pBuffer  The buffer to derive the index of.
    @param pbIdx    On success, populated with the index of the buffer.

    @return Boolean indicating result.

    @retval true    Success.
    @retval false   Failure.  @p pBuffer is not a valid buffer pointer.
*/
static bool BufferToIdx(
    const void *pBuffer,
    uint8_t    *pbIdx)
{
    bool        fRet = false;

    if(    PTR_IS_ARRAY_ELEMENT(pBuffer, gBufCtx.pbBlkBuf, REDCONF_BUFFER_COUNT << BLOCK_SIZE_P2, REDCONF_BLOCK_SIZE)
        && (pbIdx != NULL))
    {
        uint8_t bIdx = (uint8_t)(((uintptr_t)pBuffer - (uintptr_t)gBufCtx.pbBlkBuf) >> BLOCK_SIZE_P2);

        /*  This should be guaranteed, since PTR_IS_ARRAY_ELEMENT() was true.
        */
        REDASSERT(bIdx < REDCONF_BUFFER_COUNT);

        /*  At this point, we know the buffer pointer refers to a valid buffer.
            However, if the corresponding buffer head isn't an in-use buffer for
            the current volume, then something is wrong.
        */
        if(    (gBufCtx.aHead[bIdx].ulBlock != BBLK_INVALID)
            && (gBufCtx.aHead[bIdx].bVolNum == gbRedVolNum))
        {
            *pbIdx = bIdx;
            fRet = true;
        }
    }

    return fRet;
}


#if REDCONF_READ_ONLY == 0
/** @brief Write out a dirty buffer.

    @param bIdx The index of the buffer to write.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
    @retval -RED_EINVAL Invalid parameters.
*/
static REDSTATUS BufferWrite(
    uint8_t     bIdx)
{
    REDSTATUS   ret = 0;

    if(bIdx < REDCONF_BUFFER_COUNT)
    {
        const BUFFERHEAD   *pHead = &gBufCtx.aHead[bIdx];
        uint8_t            *pbBuffer = BIDX2BUF(bIdx);

        REDASSERT((pHead->uFlags & BFLAG_DIRTY) != 0U);

        if((pHead->uFlags & BFLAG_META) != 0U)
        {
            ret = RedBufferFinalize(pbBuffer, pHead->bVolNum, pHead->uFlags);
        }

        if(ret == 0)
        {
            ret = RedIoWrite(pHead->bVolNum, pHead->ulBlock, 1U, pbBuffer);

          #ifdef REDCONF_ENDIAN_SWAP
            RedBufferEndianSwap(pbBuffer, pHead->uFlags);
          #endif
        }
    }
    else
    {
        REDERROR();
        ret = -RED_EINVAL;
    }

    return ret;
}
#endif /* REDCONF_READ_ONLY == 0 */


/** @brief Mark a buffer as least recently used.

    @param bIdx The index of the buffer to make LRU.
*/
static void BufferMakeLRU(
    uint8_t bIdx)
{
    if(bIdx >= REDCONF_BUFFER_COUNT)
    {
        REDERROR();
    }
    else if(bIdx != gBufCtx.abMRU[REDCONF_BUFFER_COUNT - 1U])
    {
        uint8_t bMruIdx;

        /*  Find the current position of the buffer in the MRU array.  We do not
            need to check the last slot, since we already know from the above
            check that the index is not there.
        */
        for(bMruIdx = 0U; bMruIdx < (REDCONF_BUFFER_COUNT - 1U); bMruIdx++)
        {
            if(bIdx == gBufCtx.abMRU[bMruIdx])
            {
                break;
            }
        }

        if(bMruIdx < (REDCONF_BUFFER_COUNT - 1U))
        {
            /*  Move the buffer index to the back of the MRU array, making it
                the LRU buffer.
            */
            RedMemMove(&gBufCtx.abMRU[bMruIdx], &gBufCtx.abMRU[bMruIdx + 1U], REDCONF_BUFFER_COUNT - ((uint32_t)bMruIdx + 1U));
            gBufCtx.abMRU[REDCONF_BUFFER_COUNT - 1U] = bIdx;
        }
        else
        {
            REDERROR();
        }
    }
    else
    {
        /*  Buffer already LRU, nothing to do.
        */
    }
}


/** @brief Mark a buffer as most recently used.

    @param bIdx The index of the buffer to make MRU.
*/
static void BufferMakeMRU(
    uint8_t bIdx)
{
    if(bIdx >= REDCONF_BUFFER_COUNT)
    {
        REDERROR();
    }
    else if(bIdx != gBufCtx.abMRU[0U])
    {
        uint8_t bMruIdx;

        /*  Find the current position of the buffer in the MRU array.  We do not
            need to check the first slot, since we already know from the above
            check that the index is not there.
        */
        for(bMruIdx = 1U; bMruIdx < REDCONF_BUFFER_COUNT; bMruIdx++)
        {
            if(bIdx == gBufCtx.abMRU[bMruIdx])
            {
                break;
            }
        }

        if(bMruIdx < REDCONF_BUFFER_COUNT)
        {
            /*  Move the buffer index to the front of the MRU array, making it
                the MRU buffer.
            */
            RedMemMove(&gBufCtx.abMRU[1U], &gBufCtx.abMRU[0U], bMruIdx);
            gBufCtx.abMRU[0U] = bIdx;
        }
        else
        {
            REDERROR();
        }
    }
    else
    {
        /*  Buffer already MRU, nothing to do.
        */
    }
}


/** @brief Find a block in the buffers.

    @param ulBlock  The block number to find.
    @param pbIdx    If the block is buffered (true is returned), populated with
                    the index of the buffer.

    @return Boolean indicating whether or not the block is buffered.

    @retval true    @p ulBlock is buffered, and its index has been stored in
                    @p pbIdx.
    @retval false   @p ulBlock is not buffered.
*/
static bool BufferFind(
    uint32_t ulBlock,
    uint8_t *pbIdx)
{
    bool     ret = false;

    if((ulBlock >= gpRedVolume->ulBlockCount) || (pbIdx == NULL))
    {
        REDERROR();
    }
    else
    {
        uint8_t bIdx;

        for(bIdx = 0U; bIdx < REDCONF_BUFFER_COUNT; bIdx++)
        {
            const BUFFERHEAD *pHead = &gBufCtx.aHead[bIdx];

            if((pHead->bVolNum == gbRedVolNum) && (pHead->ulBlock == ulBlock))
            {
                *pbIdx = bIdx;
                ret = true;
                break;
            }
        }
    }

    return ret;
}

#endif /* BUFFER_MODULE == BM_SIMPLE */
