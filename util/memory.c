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
    @brief Default implementations of memory manipulation functions.

    These implementations are intended to be small and simple, and thus forego
    all optimizations.  If the C library is available, or if there are better
    third-party implementations available in the system, those can be used
    instead by defining the appropriate macros in redconf.h.

    These functions are not intended to be completely 100% ANSI C compatible
    implementations, but rather are designed to meet the needs of Reliance Edge.
    The compatibility is close enough that ANSI C compatible implementations
    can be "dropped in" as replacements without difficulty.
*/
#include <redfs.h>


#ifndef RedMemCpyUnchecked
static void RedMemCpyUnchecked(void *pDest, const void *pSrc, uint32_t ulLen);
#endif
#ifndef RedMemMoveUnchecked
static void RedMemMoveUnchecked(void *pDest, const void *pSrc, uint32_t ulLen);
#endif
#ifndef RedMemSetUnchecked
static void RedMemSetUnchecked(void *pDest, uint8_t bVal, uint32_t ulLen);
#endif
#ifndef RedMemCmpUnchecked
static int32_t RedMemCmpUnchecked(const void *pMem1, const void *pMem2, uint32_t ulLen);
#endif


/** @brief Copy memory from one address to another.

    The source and destination memory buffers should not overlap.  If the
    buffers overlap, use RedMemMove() instead.

    @param pDest    The destination buffer.
    @param pSrc     The source buffer.
    @param ulLen    The number of bytes to copy.
*/
void RedMemCpy(
    void       *pDest,
    const void *pSrc,
    uint32_t    ulLen)
{
    if((pDest == NULL) || (pSrc == NULL))
    {
        REDERROR();
    }
    else
    {
        RedMemCpyUnchecked(pDest, pSrc, ulLen);
    }
}


#ifndef RedMemCpyUnchecked
/** @brief Copy memory from one address to another.

    This function should only be called from RedMemCpy().

    @param pDest    The destination buffer.
    @param pSrc     The source buffer.
    @param ulLen    The number of bytes to copy.
*/
static void RedMemCpyUnchecked(
    void           *pDest,
    const void     *pSrc,
    uint32_t        ulLen)
{
    uint8_t        *pbDest = pDest;
    const uint8_t  *pbSrc = pSrc;
    uint32_t        ulIdx;

    for(ulIdx = 0U; ulIdx < ulLen; ulIdx++)
    {
        pbDest[ulIdx] = pbSrc[ulIdx];
    }
}
#endif


/** @brief Move memory from one address to another.

    Supports overlapping memory regions.  If memory regions do not overlap, it
    is generally better to use RedMemCpy() instead.

    @param pDest    The destination buffer.
    @param pSrc     The source buffer.
    @param ulLen    The number of bytes to copy.
*/
void RedMemMove(
    void       *pDest,
    const void *pSrc,
    uint32_t    ulLen)
{
    if((pDest == NULL) || (pSrc == NULL))
    {
        REDERROR();
    }
    else
    {
        RedMemMoveUnchecked(pDest, pSrc, ulLen);
    }
}


#ifndef RedMemMoveUnchecked
/** @brief Move memory from one address to another.

    This function should only be called from RedMemMove().

    @param pDest    The destination buffer.
    @param pSrc     The source buffer.
    @param ulLen    The number of bytes to copy.
*/
static void RedMemMoveUnchecked(
    void           *pDest,
    const void     *pSrc,
    uint32_t        ulLen)
{
    uint8_t        *pbDest = pDest;
    const uint8_t  *pbSrc = pSrc;
    uint32_t        ulIdx;

    /*  In order to copy between overlapping memory regions, RedMemMove() must
        copy forward if the destination memory is lower, and backward if the
        destination memory is higher.  Failure to do so would yield incorrect
        results.

        The only way to make this determination without gross inefficiency is to
        use pointer comparison.  Pointer comparisons are undefined unless both
        pointers point within the same object or array (or one element past the
        end of the array); see section 6.3.8 of ANSI C89.  While RedMemMove() is
        normally only used when memory regions overlap, which would not result
        in undefined behavior, it (like memmove()) is supposed to work even for
        non-overlapping regions, which would make this function invoke undefined
        behavior.  Experience has shown the pointer comparisons of this sort
        behave intuitively on common platforms, even though the behavior is
        undefined.  For those platforms where this is not the case, this
        implementation of memmove() should be replaced with an alternate one.
    */
    if(pbDest < pbSrc)
    {
        /*  If the destination is lower than the source with overlapping memory
            regions, we must copy from start to end in order to copy the memory
            correctly.

            Don't use RedMemCpy() to do this.  It is possible that RedMemCpy()
            has been replaced (even though this function has not been replaced)
            with an implementation that cannot handle any kind of buffer
            overlap.
        */
        for(ulIdx = 0U; ulIdx < ulLen; ulIdx++)
        {
            pbDest[ulIdx] = pbSrc[ulIdx];
        }
    }
    else
    {
        ulIdx = ulLen;

        while(ulIdx > 0U)
        {
            ulIdx--;
            pbDest[ulIdx] = pbSrc[ulIdx];
        }
    }
}
#endif /* RedMemMoveUnchecked */


/** @brief Initialize a buffer with the specified byte value.

    @param pDest    The buffer to initialize.
    @param bVal     The byte value with which to initialize @p pDest.
    @param ulLen    The number of bytes to initialize.
*/
void RedMemSet(
    void       *pDest,
    uint8_t     bVal,
    uint32_t    ulLen)
{
    if(pDest == NULL)
    {
        REDERROR();
    }
    else
    {
        RedMemSetUnchecked(pDest, bVal, ulLen);
    }
}


#ifndef RedMemSetUnchecked
/** @brief Initialize a buffer with the specified byte value.

    This function should only be called from RedMemSet().

    @param pDest    The buffer to initialize.
    @param bVal     The byte value with which to initialize @p pDest.
    @param ulLen    The number of bytes to initialize.
*/
static void RedMemSetUnchecked(
    void       *pDest,
    uint8_t     bVal,
    uint32_t    ulLen)
{
    uint8_t    *pbDest = pDest;
    uint32_t    ulIdx;

    for(ulIdx = 0U; ulIdx < ulLen; ulIdx++)
    {
        pbDest[ulIdx] = bVal;
    }
}
#endif


/** @brief Compare the contents of two buffers.

    @param pMem1    The first buffer to compare.
    @param pMem2    The second buffer to compare.
    @param ulLen    The length to compare.

    @return Zero if the two buffers are the same, otherwise nonzero.

    @retval 0   @p pMem1 and @p pMem2 are the same.
    @retval 1   @p pMem1 is greater than @p pMem2, as determined by the
                values of the first differing bytes.
    @retval -1  @p pMem2 is greater than @p pMem1, as determined by the
                values of the first differing bytes.
*/
int32_t RedMemCmp(
    const void *pMem1,
    const void *pMem2,
    uint32_t    ulLen)
{
    int32_t     lResult;

    if((pMem1 == NULL) || (pMem2 == NULL))
    {
        REDERROR();
        lResult = 0;
    }
    else
    {
        lResult = RedMemCmpUnchecked(pMem1, pMem2, ulLen);
    }

    return lResult;
}


#ifndef RedMemCmpUnchecked
/** @brief Compare the contents of two buffers.

    @param pMem1    The first buffer to compare.
    @param pMem2    The second buffer to compare.
    @param ulLen    The length to compare.

    @return Zero if the two buffers are the same, otherwise nonzero.
*/
static int32_t RedMemCmpUnchecked(
    const void     *pMem1,
    const void     *pMem2,
    uint32_t        ulLen)
{
    const uint8_t  *pbMem1 = pMem1;
    const uint8_t  *pbMem2 = pMem2;
    uint32_t        ulIdx = 0U;
    int32_t         lResult;

    while((ulIdx < ulLen) && (pbMem1[ulIdx] == pbMem2[ulIdx]))
    {
        ulIdx++;
    }

    if(ulIdx == ulLen)
    {
        lResult = 0;
    }
    else if(pbMem1[ulIdx] > pbMem2[ulIdx])
    {
        lResult = 1;
    }
    else
    {
        lResult = -1;
    }

    return lResult;
}
#endif

