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
    @brief Implements a heap for memory allocation.
*/
#include <redfs.h>

#ifdef REDCONF_HEAP_ALLOCATOR

#define D_HEAP_DEBUG        0U /* Must be 0 for check-in */

/*  RedPrintf() is a test utility and normally should not be used outside of
    test code.  Here, we use it only when heap debugging is enabled.
*/
#if (D_HEAP_DEBUG > 0U) && (REDCONF_OUTPUT == 1)
#include <redtestutils.h>
#define REDPRINTF(lev, txt) (((lev) <= D_HEAP_DEBUG) ? (void)RedPrintf txt : (void)0)
#else
#define REDPRINTF(lev, txt) ((void)0)
#endif

#define REDHEAP_ALIGN_SIZE  (sizeof(void *))
#define REDHEAP_ALIGN_MASK  (REDHEAP_ALIGN_SIZE - 1U)


/*  Header for each memory block in the heap
*/
typedef struct sREDHEAPHDR
{
    uint32_t            ulSentinel;     /* Sentinel with low bit allocation indicator */
    uint32_t            ulBlockSize;    /* Size of this heap allocation including header */
    struct sREDHEAPHDR *pNext;          /* Next heap header or NULL */
    struct sREDHEAPHDR *pPrev;          /* Previous heap header or NULL */
} REDHEAPHDR;

#define REDHEAP_HDR_SIZE            ((sizeof(REDHEAPHDR) + REDHEAP_ALIGN_MASK) & ~REDHEAP_ALIGN_MASK)
#define REDHEAP_MEM_TO_HDR(_pMem)   ((REDHEAPHDR *)((uint8_t *)(_pMem) - REDHEAP_HDR_SIZE))
#define REDHEAP_HDR_TO_MEM(_pHdr)   (((uint8_t *)(_pHdr)) + REDHEAP_HDR_SIZE)
#define REDHEAP_SENTINEL_FREE       (0xFBFCFDFEU)
#define REDHEAP_SENTINEL_ALLOC      (REDHEAP_SENTINEL_FREE | 1U)
#define REDHEAP_IS_ALLOC(_pHdr)     ((_pHdr)->ulSentinel == REDHEAP_SENTINEL_ALLOC)
#define REDHEAP_IS_FREE(_pHdr)      ((_pHdr)->ulSentinel == REDHEAP_SENTINEL_FREE)


/*  Heap management
*/
typedef struct
{
    uint8_t    *pbPoolBase;         /* original pool base, never changes */
    uint32_t    ulPoolSize;         /* original pool size, never changes */
    uint32_t    ulAllocBytes;       /* Bytes allocated */
    uint32_t    ulMaxAllocBytes;    /* Maximum bytes allocated */
    uint32_t    ulAllocCount;       /* Allocated headers */
    uint32_t    ulTotalCount;       /* Total headers */
} REDHEAPINFO;

/*  Verbosity levels for RedHeapCheck()
*/
#define RED_HEAP_VERBOSITY_DEFAULT  0U  /* Default displays no output */
#define RED_HEAP_VERBOSITY_SUMMARY  1U  /* Display a summary only */
#define RED_HEAP_VERBOSITY_HEADERS  2U  /* Display each block header and a summary */


static REDHEAPINFO gHI;


/** @brief Initialize the memory heap subsystem.

    It must be called early in the driver initialization process, before any
    other functions are invoked that may attempt to allocate memory.

    @param pMemBase     The address of the base of the memory pool.  This must
                        be aligned on an REDHEAP_ALIGN_SIZE boundary.
    @param ulMemSize    The size of the memory pool.  This value must be evenly
                        divided by REDHEAP_ALIGN_SIZE.
*/
void RedHeapInit(
    void       *pMemBase,
    uint32_t    ulMemSize)
{
    REDHEAPHDR  *pHead;
    REDHEAPHDR  *pLast;

    REDPRINTF(1U, ("RedHeapInit() base=0x%p size=0x%lx\n", pMemBase, (unsigned long)ulMemSize));

    REDASSERT(pMemBase != NULL);
    REDASSERT(IS_ALIGNED_PTR(pMemBase, REDHEAP_ALIGN_SIZE));
    REDASSERT(ulMemSize >= (REDHEAP_HDR_SIZE * 2U));
    REDASSERT((ulMemSize % REDHEAP_ALIGN_SIZE) == 0U);

    RedMemSet(&gHI, 0, sizeof(gHI));
    gHI.pbPoolBase = pMemBase;
    gHI.ulPoolSize = ulMemSize;
    gHI.ulTotalCount = 2U;
    gHI.ulAllocCount = 1U;
    gHI.ulAllocBytes = REDHEAP_HDR_SIZE;
    gHI.ulMaxAllocBytes = gHI.ulAllocBytes;

    /*  Make the first header
    */
    pHead = (REDHEAPHDR *)gHI.pbPoolBase;
    pHead->ulSentinel = REDHEAP_SENTINEL_FREE;
    pHead->ulBlockSize = ((gHI.ulPoolSize - REDHEAP_HDR_SIZE) & ~REDHEAP_ALIGN_MASK);
    pHead->pNext = (REDHEAPHDR *)&gHI.pbPoolBase[pHead->ulBlockSize];
    pHead->pPrev = NULL;

    /*  Now make the terminating header.  This places a sentinel at the end
        of the heap.  Mark as allocated so it will never combine with a free
        allocation.
    */
    pLast = pHead->pNext;
    pLast->ulSentinel = REDHEAP_SENTINEL_ALLOC;
    pLast->ulBlockSize = REDHEAP_HDR_SIZE;
    pLast->pNext = NULL;
    pLast->pPrev = pHead;
}


/** @brief Mark memory block as allocated.

    Mark memory block as allocated while also splitting the allocation if it is
    too large.

    @param pBlock           Header to be allocated.
    @param ulRequestedSize  Size to be allocated.
*/
static void HeapBlockAlloc(
    REDHEAPHDR *pBlock,
    uint32_t    ulRequestedSize)
{
    /*  Split this allocation if it is too large
    */
    if(pBlock->ulBlockSize > (ulRequestedSize + REDHEAP_HDR_SIZE))
    {
        REDHEAPHDR *pNext;

        pNext = (REDHEAPHDR *)((uint8_t *)pBlock + ulRequestedSize);
        pNext->pNext = pBlock->pNext;
        pNext->pNext->pPrev = pNext;
        pNext->pPrev = pBlock;
        pNext->pPrev->pNext = pNext;
        pNext->ulBlockSize = pBlock->ulBlockSize - ulRequestedSize;
        pNext->ulSentinel = REDHEAP_SENTINEL_FREE;
        gHI.ulTotalCount++;

        pBlock->ulBlockSize = ulRequestedSize;
    }

    /*  Mark this block as allocated
    */
    pBlock->ulSentinel = REDHEAP_SENTINEL_ALLOC;
    gHI.ulAllocCount++;
    gHI.ulAllocBytes += pBlock->ulBlockSize;
    if(gHI.ulAllocBytes > gHI.ulMaxAllocBytes)
    {
        gHI.ulMaxAllocBytes = gHI.ulAllocBytes;
    }
}


/** @brief Allocate a block of memory.

    Allocate a block of memory from an internal heap.

    @param ulSize  Number of bytes to allocate.

    @return Pointer to allocated memory or NULL on failure.
*/
void *RedHeapAlloc(
    uint32_t  ulSize)
{
    REDHEAPHDR *pHead;
    REDHEAPHDR *pBestBlock = NULL;
    void       *pMem = NULL;
    uint32_t    ulBestSize = 0U;
    uint32_t    ulRequestedSize;

    /*  Search for the best fit
    */
    ulRequestedSize = ((ulSize + REDHEAP_ALIGN_MASK) & ~REDHEAP_ALIGN_MASK) + REDHEAP_HDR_SIZE;
    pHead = (REDHEAPHDR *)gHI.pbPoolBase;
    while(pHead != NULL)
    {
        /*  Determine if this free block is the best fit so far
        */
        if(REDHEAP_IS_FREE(pHead))
        {
            if((pHead->ulBlockSize >= ulRequestedSize) && ((ulBestSize == 0U) || (pHead->ulBlockSize < ulBestSize)))
            {
                ulBestSize = pHead->ulBlockSize;
                pBestBlock = pHead;

                /*  Early out for optimal fit
                */
                if(ulBestSize == ulRequestedSize)
                {
                    break;
                }
            }
        }
        else if(!REDHEAP_IS_ALLOC(pHead))
        {
            REDPRINTF(1U, ("RedHeapAlloc() Corrupted heap, pHead=0x%p\n", pHead));
            REDERROR();
            ulBestSize = 0U;
            pBestBlock = NULL;
            break;
        }

        pHead = pHead->pNext;
    }

    if(ulBestSize > 0U)
    {
        /*  Mark this block as allocated
        */
        HeapBlockAlloc(pBestBlock, ulRequestedSize);

        /*  Determine the allocation for the caller
        */
        pMem = REDHEAP_HDR_TO_MEM(pBestBlock);
    }

    return pMem;
}


/** @brief Mark memory block as free.

    Mark memory block as free then combine with the previous and next
    allocations if they are free.

    @param pBlock Header to free

    @return If there is a previous block and it's free, returns a pointer to the
            previous block; otherwise, returns @p pBlock.
*/
static REDHEAPHDR *HeapBlockFree(
    REDHEAPHDR *pBlock)
{
    REDHEAPHDR *pRet = pBlock;

    /*  Mark this block as free
    */
    pBlock->ulSentinel = REDHEAP_SENTINEL_FREE;
    gHI.ulAllocCount--;
    gHI.ulAllocBytes -= pBlock->ulBlockSize;

    /*  Combine with the next free block
    */
    if((pBlock->pNext != NULL) && (pBlock->pNext->ulSentinel == REDHEAP_SENTINEL_FREE))
    {
        REDHEAPHDR *pNext = pBlock->pNext;

        pBlock->ulBlockSize += pNext->ulBlockSize;
        pBlock->pNext = pNext->pNext;
        if(pBlock->pNext != NULL)
        {
            pBlock->pNext->pPrev = pBlock;
        }
        pNext->ulSentinel = ~REDHEAP_SENTINEL_FREE;
        gHI.ulTotalCount--;
    }

    /*  Combine with the previous free block
    */
    if((pBlock->pPrev != NULL) && (pBlock->pPrev->ulSentinel == REDHEAP_SENTINEL_FREE))
    {
        REDHEAPHDR *pPrev = pBlock->pPrev;

        pPrev->ulBlockSize += pBlock->ulBlockSize;
        pPrev->pNext = pBlock->pNext;
        if(pPrev->pNext != NULL)
        {
            pPrev->pNext->pPrev = pPrev;
        }
        pBlock->ulSentinel = ~REDHEAP_SENTINEL_FREE;
        gHI.ulTotalCount--;

        pRet = pPrev;
    }

    return pRet;
}


/** @brief Change the size of an allocated block of memory.

    Change the size of an allocated memory block returned from RedHeapAlloc().
    Contents of the memory will be unchanged up to the lesser of the old and new
    sizes.

    @param pMem     A pointer to the allocated memory block.
    @param ulSize   Number of bytes to allocate

    @return Pointer to allocated memory or NULL on failure.  The location and
            contents of the memory block are unchanged on failure.  On success,
            the location can change but the contents of the memory block will be
            moved.
*/
void *RedHeapRealloc(
    void       *pMem,
    uint32_t    ulSize)
{
    REDHEAPHDR *pHead;
    REDHEAPHDR *pCurrent;
    REDHEAPHDR *pBestBlock;
    uint32_t    ulBestSize;
    uint32_t    ulRequestedSize;
    void       *pNewMem = NULL;

    /*  Specifying a new size of zero indicates that the memory block should be
        freed.
    */
    if(ulSize == 0U)
    {
        RedHeapFree(pMem);
        return NULL;
    }

    /*  Memory block being freed should be within the heap
    */
    if(    ((uint8_t *)pMem < &gHI.pbPoolBase[REDHEAP_HDR_SIZE])
        || ((uint8_t *)pMem >= &gHI.pbPoolBase[gHI.ulPoolSize - (REDHEAP_HDR_SIZE * 2U)]))
    {
        REDPRINTF(1U, ("RedHeapRealloc() memory outside of heap, pMem=0x%p\n", pMem));
        REDERROR();
        return NULL;
    }

    /*  Validate this header
    */
    pCurrent = REDHEAP_MEM_TO_HDR(pMem);
    if(pCurrent->ulSentinel != REDHEAP_SENTINEL_ALLOC)
    {
        REDPRINTF(1U, ("RedHeapRealloc() Corrupted heap, pCurrent=0x%p\n", pCurrent));
        REDERROR();
        return NULL;
    }

    /*  For the moment, assume that the best memory block is the current block.
    */
    pBestBlock = pCurrent;
    ulBestSize = pCurrent->ulBlockSize;

    /*  Account for the merger of the previous and next memory blocks.
    */
    if((pCurrent->pNext != NULL) && (pCurrent->pNext->ulSentinel == REDHEAP_SENTINEL_FREE))
    {
        ulBestSize += pCurrent->pNext->ulBlockSize;
    }
    if((pCurrent->pPrev != NULL) && (pCurrent->pPrev->ulSentinel == REDHEAP_SENTINEL_FREE))
    {
        ulBestSize += pCurrent->pPrev->ulBlockSize;
        pBestBlock = pCurrent->pPrev;
    }

    /*  Ensure the obtimistic best fit actually fits
    */
    ulRequestedSize = ((ulSize + REDHEAP_ALIGN_MASK) & ~REDHEAP_ALIGN_MASK) + REDHEAP_HDR_SIZE;
    if(ulBestSize < ulRequestedSize)
    {
        pBestBlock = NULL;
        ulBestSize = 0U;
    }

    /*  Search for the best fit
    */
    pHead = (REDHEAPHDR *)gHI.pbPoolBase;
    while(pHead != NULL)
    {
        /*  Skip over the best block which could be part of a future merge
        */
        if(pHead == pBestBlock)
        {
            pHead = (REDHEAPHDR *)((uint8_t *)pBestBlock + ulBestSize);
            continue;
        }

        /*  Determine if this free block is the best fit so far
        */
        if(REDHEAP_IS_FREE(pHead))
        {
            if((pHead->ulBlockSize >= ulRequestedSize) && ((ulBestSize == 0U) || (pHead->ulBlockSize < ulBestSize)))
            {
                pBestBlock = pHead;
                ulBestSize = pHead->ulBlockSize;

                /*  Early out for optimal fit
                */
                if(ulBestSize == ulRequestedSize)
                {
                    break;
                }
            }
        }
        else if(!REDHEAP_IS_ALLOC(pHead))
        {
            REDPRINTF(1U, ("RedHeapRealloc() Corrupted heap, pHead=0x%p\n", pHead));
            REDERROR();
            ulBestSize = 0;
            pBestBlock = NULL;
            break;
        }

        pHead = pHead->pNext;
    }

    if(ulBestSize > 0U)
    {
        uint32_t ulCopySize = REDMIN(ulSize, pCurrent->ulBlockSize - REDHEAP_HDR_SIZE);

        if((pBestBlock == pCurrent) || (pBestBlock == pCurrent->pPrev))
        {
            uint8_t *pbDst;
            uint8_t *pbSrc = REDHEAP_HDR_TO_MEM(pCurrent);

            /*  Combine neighboring blocks
            */
            pBestBlock = HeapBlockFree(pCurrent);

            /*  Copy user data from the old location to the new location
            */
            pbDst = REDHEAP_HDR_TO_MEM(pBestBlock);
            if(pbDst != pbSrc)
            {
                if((pbSrc >= pbDst) && (pbSrc < &pbDst[ulCopySize]))
                {
                    RedMemMove(pbDst, pbSrc, ulCopySize);
                }
                else
                {
                    RedMemCpy(pbDst, pbSrc, ulCopySize);
                }
            }

            /*  Split this allocation if it is too large
            */
            HeapBlockAlloc(pBestBlock, ulRequestedSize);
        }
        else
        {
            /*  Copy the user data from the old block to the new block
            */
            RedMemCpy(REDHEAP_HDR_TO_MEM(pBestBlock), REDHEAP_HDR_TO_MEM(pCurrent), ulCopySize);

            /*  Allocate the new block
            */
            HeapBlockAlloc(pBestBlock, ulRequestedSize);

            /*  Free the old block
            */
            HeapBlockFree(pCurrent);
        }

        /*  Determine the allocation for the caller
        */
        pNewMem = REDHEAP_HDR_TO_MEM(pBestBlock);
    }

    return pNewMem;
}


/** @brief Release a block of memory.

    Release a block of memory that was allocated with RedHeapAlloc().

    @param pMem  A pointer to the allocated memory block.
*/
void RedHeapFree(
    void       *pMem)
{
    REDHEAPHDR *pHead;

    /*  Memory block being freed should be within the heap
    */
    if(    ((uint8_t *)pMem < &gHI.pbPoolBase[REDHEAP_HDR_SIZE])
        || ((uint8_t *)pMem >= &gHI.pbPoolBase[gHI.ulPoolSize - (REDHEAP_HDR_SIZE * 2U)]))
    {
        REDPRINTF(1U, ("RedHeapFree() memory outside of heap, pMem=0x%p\n", pMem));
        REDERROR();
        return;
    }

    /*  Validate this header
    */
    pHead = REDHEAP_MEM_TO_HDR(pMem);
    if(pHead->ulSentinel != REDHEAP_SENTINEL_ALLOC)
    {
        REDPRINTF(1U, ("RedHeapFree() Corrupted heap, pHead=0x%p\n", pHead));
        REDERROR();
        return;
    }

    /*  Mark this block as free
    */
    HeapBlockFree(pHead);
}


/** @brief Get heap stats.

    Get heap stats for number of bytes allocated, number of allocation headers,
    and total number of headers.

    @param pulAllocBytes    Address to record the number of bytes allocated.
    @param pulMaxAllocBytes Address to record the Maximum number of bytes
                            allocated.
    @param pulAllocHdr      Address to record the allocated headers.
    @param pulTotalHdr      Address to record the total headers.
*/
void RedHeapStats(
    uint32_t *pulAllocBytes,
    uint32_t *pulMaxAllocBytes,
    uint32_t *pulAllocHdr,
    uint32_t *pulTotalHdr)
{
    if(pulAllocBytes != NULL)
    {
        *pulAllocBytes = gHI.ulAllocBytes;
    }
    if(pulMaxAllocBytes != NULL)
    {
        *pulMaxAllocBytes = gHI.ulMaxAllocBytes;
    }
    if(pulAllocHdr != NULL)
    {
        *pulAllocHdr = gHI.ulAllocCount;
    }
    if(pulTotalHdr != NULL)
    {
        *pulTotalHdr = gHI.ulTotalCount;
    }
}


/** @brief Check the state of the heap.

    Check the state of the heap while optionally displaying each heap header
    and/or a heap summary.

    @note This function is always silent, regardless of @p bVerbosity, when
          heap debugging is disabled.

    @param bVerbosity   Verbosity level.  0 - no output, 1 - display heap
                        summary, 2 - additionally display heap headers.

    @return Zero on success otherwise a negative value.
*/
int32_t RedHeapCheck(
    uint8_t     bVerbosity)
{
    REDHEAPHDR *pHead;
    bool        fCorrupt = false;
    uint32_t    ulAllocated = 0U;
    uint32_t    ulFree = 0U;
    uint32_t    ulBytesFree = 0U;
    uint32_t    ulBytesAllocated = 0U;

    /*  Traverse the allocation list
    */
    pHead = (REDHEAPHDR *)gHI.pbPoolBase;
    while(pHead != NULL)
    {
        if(bVerbosity >= RED_HEAP_VERBOSITY_HEADERS)
        {
            REDPRINTF(1U, ("RedHeapCheck() Address=0x%p Next=0x%p Prev=0x%p Sentinel=0x%x Size=0x%lx\n",
                pHead, pHead->pNext, pHead->pPrev, (unsigned)pHead->ulSentinel, (unsigned long)pHead->ulBlockSize));
        }

        if(pHead->ulSentinel == REDHEAP_SENTINEL_FREE)
        {
            ulFree++;
            ulBytesFree += pHead->ulBlockSize;
        }
        else if(pHead->ulSentinel == REDHEAP_SENTINEL_ALLOC)
        {
            ulAllocated++;
            ulBytesAllocated += pHead->ulBlockSize;
        }
        else
        {
            fCorrupt = true;
            break;
        }

        if((pHead->pNext != NULL) && (pHead->pNext->pPrev != pHead))
        {
            fCorrupt = true;
            break;
        }
        if((pHead->pPrev != NULL) && (pHead->pPrev->pNext != pHead))
        {
            fCorrupt = true;
            break;
        }
        if((pHead->pNext != NULL) && ((uint32_t)((uint8_t *)pHead->pNext - (uint8_t *)pHead) != pHead->ulBlockSize))
        {
            fCorrupt = true;
            break;
        }

        pHead = pHead->pNext;
    }

    if(!fCorrupt)
    {
        fCorrupt |= ulBytesAllocated != gHI.ulAllocBytes;
        fCorrupt |= ulAllocated != gHI.ulAllocCount;
        fCorrupt |= ulFree != (gHI.ulTotalCount - gHI.ulAllocCount);
    }

    if(fCorrupt)
    {
        REDPRINTF(1U, ("RedHeapCheck() Corrupted heap, pHead=0x%p\n", pHead));
        REDERROR();
    }
    else if(bVerbosity >= RED_HEAP_VERBOSITY_SUMMARY)
    {
        REDPRINTF(1U, ("Heap Summary: BlocksAllocated=%3lu BlocksFree=%3lu BytesAllocated=%6lu BytesFree=%6lu\n",
            (unsigned long)ulAllocated, (unsigned long)ulFree, (unsigned long)ulBytesAllocated, (unsigned long)ulBytesFree));
    }

    return fCorrupt ? -1 : 0;
}


/** @brief Allocate a zeroed block of memory.

    Allocate a block of memory from an internal heap and initialize it to zero.

    @param ulElements       Number of elements.
    @param ulElementSize    Size of each element.

    @return Pointer to allocated memory or NULL on failure.
*/
void *RedHeapCalloc(
    uint32_t    ulElements,
    uint32_t    ulElementSize)
{
    uint32_t    ulSize;
    void       *pMem;

    /*  Determine the amount of memory to be allocated
    */
    ulSize = (ulElementSize + REDHEAP_ALIGN_MASK) & ~REDHEAP_ALIGN_MASK;
    if(    (ulSize < ulElementSize)
        || ((ulElements > 0U) && (ulSize > (UINT32_MAX / ulElements))))
    {
        pMem = NULL; /* Overflow */
    }
    else
    {
        ulSize *= ulElements;

        /*  Allocate the memory and initialize it to zero
        */
        pMem = RedHeapAlloc(ulSize);
        if(pMem != NULL)
        {
            RedMemSet(pMem, 0, ulSize);
        }
    }

    return pMem;
}

#endif /* #ifdef REDCONF_HEAP_ALLOCATOR */
