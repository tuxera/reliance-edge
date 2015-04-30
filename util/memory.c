/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                   Copyright (c) 2014-2015 Datalight, Inc.
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
    comply with the terms of the GPLv2 license may obtain a commercial license
    before incorporating Reliance Edge into proprietary software for
    distribution in any form.  Visit http://www.datalight.com/reliance-edge for
    more information.
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


#ifndef RedMemCpy
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
        uint8_t        *pbDest = CAST_VOID_PTR_TO_UINT8_PTR(pDest);
        const uint8_t  *pbSrc = CAST_VOID_PTR_TO_CONST_UINT8_PTR(pSrc);
        uint32_t        ulIdx;

        for(ulIdx = 0U; ulIdx < ulLen; ulIdx++)
        {
            pbDest[ulIdx] = pbSrc[ulIdx];
        }
    }
}
#endif


#ifndef RedMemMove

/** @brief Determine whether RedMemMove() must copy memory in the forward
           direction, instead of in the reverse.

    In order to copy between overlapping memory regions, RedMemMove() must copy
    forward if the destination memory is lower, and backward if the destination
    memory is higher.  Failure to do so would yield incorrect results.

    The only way to make this determination without gross inefficiency is to
    use pointer comparison.  Pointer comparisons are undefined unless both
    pointers point within the same object or array (or one element past the end
    of the array); see section 6.3.8 of ANSI C89.  While RedMemMove() is
    normally only used when memory regions overlap, which would not result in
    undefined behavior, it (like memmove()) is supposed to work even for non-
    overlapping regions, which would make this function invoke undefined
    behavior.  Experience has shown the pointer comparisons of this sort behave
    intuitively on common platforms, even though the behavior is undefined.  For
    those platforms where this is not the case, this implementation of memmove()
    should be replaced with an alternate one.

    Usages of this function deviate from MISRA-C:2012 Rule 18.3 (required).  As
    Rule 18.3 is required, a separate deviation record is required.

    @param pbDest   The destination buffer.
    @param pbSrc    The source buffer.

    @return Whether RedMemMove() must copy memory in the forward direction.
*/
static bool MemMoveMustCopyForward(
    const uint8_t *pbDest,
    const uint8_t *pbSrc)
{
    return pbDest < pbSrc;
}


/** @brief Move memory from one address to another.

    Supports overlapping memory regions.  If memory regions do not overlap, it
    is generally better to use RedMemCpy() instead.

    @param pDest    The destination buffer.
    @param pSrc     The source buffer.
    @param ulLen    The number of bytes to copy.
*/
void RedMemMove(
    void           *pDest,
    const void     *pSrc,
    uint32_t        ulLen)
{
    uint8_t        *pbDest = CAST_VOID_PTR_TO_UINT8_PTR(pDest);
    const uint8_t  *pbSrc = CAST_VOID_PTR_TO_CONST_UINT8_PTR(pSrc);
    uint32_t        ulIdx;

    if((pDest == NULL) || (pSrc == NULL))
    {
        REDERROR();
    }
    else if(MemMoveMustCopyForward(pbDest, pbSrc))
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

#endif /* RedMemMove */


#ifndef RedMemSet
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
        uint8_t    *pbDest = CAST_VOID_PTR_TO_UINT8_PTR(pDest);
        uint32_t    ulIdx;

        for(ulIdx = 0U; ulIdx < ulLen; ulIdx++)
        {
            pbDest[ulIdx] = bVal;
        }
    }
}
#endif


#ifndef RedMemCmp
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
        const uint8_t  *pbMem1 = CAST_VOID_PTR_TO_CONST_UINT8_PTR(pMem1);
        const uint8_t  *pbMem2 = CAST_VOID_PTR_TO_CONST_UINT8_PTR(pMem2);
        uint32_t        ulIdx = 0U;

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
    }

    return lResult;
}
#endif
