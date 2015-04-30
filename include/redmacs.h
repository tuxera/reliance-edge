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
*/
#ifndef REDMACS_H
#define REDMACS_H


#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef UINT8_MAX
#define UINT8_MAX   (0xFFU)
#endif
#ifndef UINT16_MAX
#define UINT16_MAX  (0xFFFFU)
#endif
#ifndef UINT32_MAX
#define UINT32_MAX  (0xFFFFFFFFU)
#endif
#ifndef INT32_MAX
#define INT32_MAX   (0x7FFFFFFF)
#endif
#ifndef INT64_MAX
#define INT64_MAX   (0x7FFFFFFFFFFFFFFFULL)
#endif

#ifndef true
#define true (1)
#endif
#ifndef false
#define false (0)
#endif

#if   REDCONF_BLOCK_SIZE == 256U
#define BLOCK_SIZE_P2 8U
#elif REDCONF_BLOCK_SIZE == 512U
#define BLOCK_SIZE_P2 9U
#elif REDCONF_BLOCK_SIZE == 1024U
#define BLOCK_SIZE_P2 10U
#elif REDCONF_BLOCK_SIZE == 2048U
#define BLOCK_SIZE_P2 11U
#elif REDCONF_BLOCK_SIZE == 4096U
#define BLOCK_SIZE_P2 12U
#elif REDCONF_BLOCK_SIZE == 8192U
#define BLOCK_SIZE_P2 13U
#elif REDCONF_BLOCK_SIZE == 16384U
#define BLOCK_SIZE_P2 14U
#elif REDCONF_BLOCK_SIZE == 32768U
#define BLOCK_SIZE_P2 15U
#elif REDCONF_BLOCK_SIZE == 65536U
#define BLOCK_SIZE_P2 16U
#else
#error "REDCONF_BLOCK_SIZE must be a power of two value between 256 and 65536"
#endif

#define REDMIN(a, b) (((a) < (b)) ? (a) : (b))


/** @brief Cast a pointer to a const uint8_t pointer.

    All usages of this macro deviate from MISRA-C:2012 Rule 11.5 (advisory).
    Because there are no alignment requirements for a uint8_t pointer, this is
    safe.  However, it is technically a deviation from the rule.

    As Rule 11.5 is advisory, a deviation record is not required.  This notice
    and the PC-Lint error inhibition option are the only records of the
    deviation.
*/
#define CAST_VOID_PTR_TO_CONST_UINT8_PTR(PTR) ((const uint8_t *)(PTR))


/** @brief Cast a pointer to a uint8_t pointer.

    All usages of this macro deviate from MISRA-C:2012 Rule 11.5 (advisory).
    Because there are no alignment requirements for a uint8_t pointer, this is
    safe.  However, it is technically a deviation from the rule.

    As Rule 11.5 is advisory, a deviation record is not required.  This notice
    and the PC-Lint error inhibition option are the only records of the
    deviation.
*/
#define CAST_VOID_PTR_TO_UINT8_PTR(PTR) ((uint8_t *)(PTR))


/** @brief Cast a pointer to a const uint32_t pointer.

    Usages of this macro may deviate from MISRA-C:2012 Rule 11.5 (advisory).
    It is only used in cases where the pointer is known to be aligned, and thus
    it is safe to do so.

    As Rule 11.5 is advisory, a deviation record is not required.  This notice
    and the PC-Lint error inhibition option are the only records of the
    deviation.

    Usages of this macro may deviate from MISRA-C:2012 Rule 11.3 (required).
    As Rule 11.3 is required, a separate deviation record is required.
*/
#define CAST_CONST_UINT32_PTR(PTR) ((const uint32_t *)(const void *)(PTR))


/** @brief Cast a pointer to a pointer, to (void **).

    Usages of this macro deviate from MISRA-C:2012 Rule 11.3 (required).
    It is only used for populating a node structure pointer with a buffer
    pointer.  Buffer pointers are 8-byte aligned, thus it is safe to do so.

    As Rule 11.3 is required, a separate deviation record is required.
*/
#define CAST_VOID_PTR_PTR(PTRPTR) ((void **)(PTRPTR))


/** @brief Create a two-dimensional byte array which is safely aligned.

    Usages of this macro deviate from MISRA-C:2012 Rule 19.2 (advisory).
    A union is required to force alignment of the block buffers, which are used
    to access metadata nodes, which must be safely aligned for 64-bit types.

    As rule 19.2 is advisory, a deviation record is not required.  This notice
    and the PC-Lint error inhibition option are the only records of the
    deviation.
*/
#define ALIGNED_2D_BYTE_ARRAY(un, nam, size1, size2)    \
    union                                               \
    {                                                   \
        uint8_t     nam[size1][size2];                  \
        uint64_t    DummyAlign;                         \
    } un



#define INODE_INVALID   (0U)    /* General-purpose invalid inode number. */
#define INODE_ROOTDIR   (2U)    /* Inode number of the root directory. */


#endif

