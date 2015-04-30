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
    @brief Implements utilities for performing endian swaps.
*/
#include <redfs.h>


#ifdef REDCONF_ENDIAN_SWAP

/** @brief Reverse the byte order of a 64-bit number.

    @param ullToRev Number whose bytes will be reversed

    @retval @p ullToRev with its bytes reversed.
*/
uint64_t RedRev64(
    uint64_t    ullToRev)
{
    uint64_t    ullRet = ullToRev;

    ullRet = ((ullRet & 0x00000000FFFFFFFFULL) << 32U) | ((ullRet & 0xFFFFFFFF00000000ULL) >> 32U);
    ullRet = ((ullRet & 0x0000FFFF0000FFFFULL) << 16U) | ((ullRet & 0xFFFF0000FFFF0000ULL) >> 16U);
    ullRet = ((ullRet & 0x00FF00FF00FF00FFULL) <<  8U) | ((ullRet & 0xFF00FF00FF00FF00ULL) >>  8U);

    return ullRet;
}


/** @brief Reverse the byte order of a 32-bit number.

    @param ulToRev  Number whose bytes will be reversed

    @retval @p ulToRev with its bytes reversed.
*/
uint32_t RedRev32(
    uint32_t    ulToRev)
{
    return   ((ulToRev & 0x000000FFU) << 24U)
           | ((ulToRev & 0x0000FF00U) <<  8U)
           | ((ulToRev & 0x00FF0000U) >>  8U)
           | ((ulToRev & 0xFF000000U) >> 24U);
}


/** @brief Reverse the byte order of a 16-bit number.

    @param uToRev   Number whose bytes will be reversed

    @retval @p uToRev with its bytes reversed.
*/
uint16_t RedRev16(
    uint16_t    uToRev)
{
    return   ((uToRev & 0xFF00U) >> 8U)
           | ((uToRev & 0x00FFU) << 8U);
}

#endif

