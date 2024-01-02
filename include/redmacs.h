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
*/
#ifndef REDMACS_H
#define REDMACS_H


#ifndef NULL
#define NULL ((void *)0)
#endif

/*  The ULL and LL suffixes mean "unsigned long long" and "long long",
    respectively.  C99 guarantees that these types are 64-bit integers or
    something larger (though in practice, it's never larger).

    Although these suffixes are a C99 feature, most C89 compilers support these
    suffixes as an extension.  Thus, even though we usually stick to C89, we
    make an exception for these suffixes.
*/
#define UINT64_SUFFIX(number) (number##ULL)
#define INT64_SUFFIX(number) (number##LL)

#ifndef UINT8_MAX
#define UINT8_MAX   (0xFFU)
#endif
#ifndef UINT16_MAX
#define UINT16_MAX  (0xFFFFU)
#endif
#ifndef UINT32_MAX
#define UINT32_MAX  (0xFFFFFFFFU)
#endif
#ifndef UINT64_MAX
#define UINT64_MAX  UINT64_SUFFIX(0xFFFFFFFFFFFFFFFF)
#endif
#ifndef INT32_MAX
#define INT32_MAX   (0x7FFFFFFF)
#endif
#ifndef INT64_MAX
#define INT64_MAX   INT64_SUFFIX(0x7FFFFFFFFFFFFFFF)
#endif

#ifndef true
#define true (1)
#endif
#ifndef false
#define false (0)
#endif

#define SECTOR_SIZE_MIN (128U)

#if   REDCONF_BLOCK_SIZE == 128U
#define BLOCK_SIZE_P2 7U
#elif REDCONF_BLOCK_SIZE == 256U
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
#error "REDCONF_BLOCK_SIZE must be a power of two value between 128 and 65536"
#endif

/** @brief Assert a condition at compile time.
*/
#define REDSTATICASSERT(EXP) ((void)sizeof(char[1 - (2 * !(EXP))]))

/** @brief Cast a const-qualified pointer to a pointer which is *not*
           const-qualified.

    Reliance Edge uses const-qualified pointers whenever possible.  However,
    sometimes the third-party RTOS or block device driver code that Reliance
    Edge interfaces with aren't as careful, so it's necessary to cast away the
    const to call those functions.

    This macro exists so that places that cast away the cost-qualifier are
    explicitly annotated.  If the pointer type cast were done without using this
    macro, it might not be obvious that the original pointer was a const type.
*/
#define CAST_AWAY_CONST(type, ptr) ((type *)(ptr))

/** @brief Determine whether a pointer is aligned.

    The specified alignment is assumed to be a power-of-two.
*/
#define IS_ALIGNED_PTR(ptr, alignment) (((uintptr_t)(ptr) & ((alignment) - 1U)) == 0U)

/** @brief Increment a uint8_t pointer so that it has the given alignment.

    @param u8ptr        The uint8_t pointer to be aligned.
    @param alignment    The desired alignment (must be a power of two).

    @return Returns @p u8ptr plus however many bytes are necessary for the
            pointer to be aligned by @p alignment bytes.
*/
#define UINT8_PTR_ALIGN(u8ptr, alignment) \
    ((((uintptr_t)(u8ptr) & ((alignment) - 1U)) == 0U) ? (u8ptr) : \
    (&(u8ptr)[(alignment) - ((uintptr_t)(u8ptr) & ((alignment) - 1U))]))

#define REDMIN(a, b) (((a) < (b)) ? (a) : (b))
#define REDMAX(a, b) (((a) > (b)) ? (a) : (b))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(ARRAY)   (sizeof(ARRAY) / sizeof((ARRAY)[0U]))
#endif

/** @brief Determine whether a pointer is a member of an array.

    Returns true only if: 1) the pointer address falls within the bounds of the
    array; and 2) the pointer address is aligned such that it points to the
    start of an array element, rather than pointing at the middle of an element.

    Notes:
    - Technically, this macro relies on "implementation-defined" pointer
      behavior, but it should work on all normal platforms.
    - The alignment check casts the pointer offset to uint32_t, to avoid doing
      modulus on uintptr_t, which might be a 64-bit type.  It is assumed that
      this macro won't be used on arrays that are >4GB in size.

    @param ptr          The pointer to examine.
    @param array        Start of the array.
    @param nelem        The number of elements in the array.
    @param elemalign    The alignment of each element in the array.

    @return Whether @p ptr is an aligned element of the @p array array.
*/
#define PTR_IS_ARRAY_ELEMENT(ptr, array, nelem, elemalign) \
    ( \
         ((uintptr_t)(ptr) >= (uintptr_t)&(array)[0U]) \
      && ((uintptr_t)(ptr) < (uintptr_t)&(array)[nelem]) \
      && (((uint32_t)((uintptr_t)(ptr) - (uintptr_t)&(array)[0U]) % (uint32_t)(elemalign)) == 0U) \
    )

#define BITMAP_SIZE(bitcnt) (((bitcnt) + 7U) / 8U)

/** @brief Determine whether an unsigned integer value is a power of two.

    Note that zero is _not_ a power of two.
*/
#define IS_POWER_OF_2(val)  (((val) > 0U) && (((val) & ((~(val)) + 1U)) == (val)))

#define INODE_INVALID       (0U) /* General-purpose invalid inode number (must be zero). */
#define INODE_FIRST_VALID   (2U) /* First valid inode number. */
#define INODE_ROOTDIR       (INODE_FIRST_VALID) /* Inode number of the root directory. */

/*  Expands to a "const" qualifier when the volume count is one, otherwise
    expands to nothing.  This is useful for variables that never change in
    single-volume configurations but do change in multi-volume configurations.
*/
#if REDCONF_VOLUME_COUNT == 1U
  #define CONST_IF_ONE_VOLUME const
#else
  #define CONST_IF_ONE_VOLUME
#endif

/*  Block device implementations may choose to validate the sector geometry in
    VOLCONF against the sector size/count values reported by the storage device.
    These macros help validate those parameters in a standardized way.
*/

/** @brief Yields the first sector number beyond the end of the volume.

    RedCoreInit() ensures that SectorOffset + SectorCount will not result in
    unsigned integer wrap-around.
*/
#define VOLUME_SECTOR_LIMIT(volnum) (gaRedVolConf[(volnum)].ullSectorOffset + gaRedBdevInfo[(volnum)].ullSectorCount)

/** @brief Determine if the sector size reported by the storage device is
           compatible with the configured volume geometry.
*/
#define VOLUME_SECTOR_SIZE_IS_VALID(volnum, devsectsize) ((uint32_t)(devsectsize) == gaRedBdevInfo[(volnum)].ulSectorSize)

/** @brief Determine if the sector count reported by the storage device is
           compatible with the configured volume geometry.

    The storage device must be large enough to contain the volume.  If it is
    bigger than needed, that is _not_ an error: the extra sectors might be in
    use for other purposes, such as another partititon.
*/
#define VOLUME_SECTOR_COUNT_IS_VALID(volnum, devsectcount) ((uint64_t)(devsectcount) >= VOLUME_SECTOR_LIMIT(volnum))

/** @brief Determine if the sector size and sector count reported by the
           storage device are compatible with the configured volume geometry.
*/
#define VOLUME_SECTOR_GEOMETRY_IS_VALID(volnum, devsectsize, devsectcount) \
    (VOLUME_SECTOR_SIZE_IS_VALID(volnum, devsectsize) && VOLUME_SECTOR_COUNT_IS_VALID(volnum, devsectcount))

/** @brief Ensure a range of sectors is within the boundaries of a volume.

    Assumes the sector offset has already been added into the starting sector.
*/
#define VOLUME_SECTOR_RANGE_IS_VALID(volnum, sectstart, sectcount) \
    (    ((sectstart) >= gaRedVolConf[(volnum)].ullSectorOffset) \
      && ((sectcount) <= (VOLUME_SECTOR_LIMIT(volnum) - (sectstart))))


#endif

