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
    @brief Defines macros and interfaces used internally by the buffer modules.
*/
#ifndef REDBUFFERPRIV_H
#define REDBUFFERPRIV_H


/*  The original implementation in buffer.c.  Simpler, smaller (code size),
    but has more limitations (fewer buffers, lower performance).
*/
#define BM_SIMPLE   1U

/*  The enhanced implementation in buffer2.c.  More complicated, larger (code
    size), but is more capable (more buffers, faster performance).
*/
#define BM_ENHANCED 2U

/*  GPL release only has the simple buffer module.

    Commercial release has both, so:
      - Only the enhanced buffer module supports the write-gather buffer, so
        using enabling it automatically selects the enhanced buffer module.
      - Otherwise, the decision is based on a buffer count threshold, for now.
*/
#if (RED_KIT == RED_KIT_GPL) || ((REDCONF_BUFFER_WRITE_GATHER_SIZE_KB == 0U) && (REDCONF_BUFFER_COUNT < 24U))
  #define BUFFER_MODULE BM_SIMPLE
#else
  #define BUFFER_MODULE BM_ENHANCED
#endif


#if DINDIR_POINTERS > 0U
  #define INODE_META_BUFFERS 3U /* Inode, double indirect, indirect */
#elif REDCONF_INDIRECT_POINTERS > 0U
  #define INODE_META_BUFFERS 2U /* Inode, indirect */
#elif REDCONF_DIRECT_POINTERS == INODE_ENTRIES
  #define INODE_META_BUFFERS 1U /* Inode only */
#endif

#define INODE_BUFFERS (INODE_META_BUFFERS + 1U) /* Add data buffer */

#if REDCONF_IMAP_EXTERNAL == 1
  #define IMAP_BUFFERS 1U
#else
  #define IMAP_BUFFERS 0U
#endif

#if (REDCONF_READ_ONLY == 1) || (REDCONF_API_FSE == 1)
  /*  Read, write, truncate, lookup: One inode all the way down, plus imap.
  */
  #define MINIMUM_BUFFER_COUNT (INODE_BUFFERS + IMAP_BUFFERS)
#elif REDCONF_API_POSIX == 1
  #if REDCONF_API_POSIX_RENAME == 1
    #if REDCONF_RENAME_ATOMIC == 1
      /*  Two parent directories all the way down.  Source and destination inode
          buffer.  One inode buffer for cyclic rename detection.  Imap.  The
          parent inode buffers are released before deleting the destination
          inode, so that does not increase the minimum.
      */
      #define MINIMUM_BUFFER_COUNT (INODE_BUFFERS + INODE_BUFFERS + 3U + IMAP_BUFFERS)
    #else
      /*  Two parent directories all the way down.  Source inode buffer.  One
          inode buffer for cyclic rename detection.  Imap.
      */
      #define MINIMUM_BUFFER_COUNT (INODE_BUFFERS + INODE_BUFFERS + 2U + IMAP_BUFFERS)
    #endif
  #else
    /*  Link/create: Needs a parent inode all the way down, an extra inode
        buffer, and an imap buffer.

        Unlink is the same, since the parent inode buffers are released before
        the inode is deleted.
    */
    #define MINIMUM_BUFFER_COUNT (INODE_BUFFERS + 1U + IMAP_BUFFERS)
  #endif
#endif

#if REDCONF_BUFFER_COUNT < MINIMUM_BUFFER_COUNT
  #error "REDCONF_BUFFER_COUNT is too low for the configuration"
#endif

/*  REDCONF_BUFFER_COUNT upper limit checked in buffer.c or buffer2.c
*/


/*  On some RISC architectures, the block buffers need to be 8-byte aligned
    in order to dereference uint64_t structure members.  On other architectures,
    the alignment requirement is lower, but allowing a lower alignment would
    only save a few bytes of memory.  Thus, keep things simple by requiring an
    8-byte alignment everywhere.
*/
#if REDCONF_BUFFER_ALIGNMENT < 8U
  #error "Configuration error: REDCONF_BUFFER_ALIGNMENT must be at least 8"
#endif

/*  The block size is the maximum supported alignment.  This is because we only
    align the start of the block buffers (BUFFERCTX::pbBlkBuf).  Buffers after
    the first, and the write-gather buffer (if enabled), are offset into the
    pbBlkBuf array at block-size aligned positions: so no matter what the
    alignment of pbBlkBuf, the block size is the maximum guaranteed alignment
    for those buffers.
*/
#if REDCONF_BUFFER_ALIGNMENT > REDCONF_BLOCK_SIZE
  #error "Configuration error: REDCONF_BUFFER_ALIGNMENT cannot exceed the block size"
#endif

/*  It is easier to align the pointer if the alignment is a power of two, and
    in practice the alignment needed for DMA is always a power-of-two.
*/
#if !IS_POWER_OF_2(REDCONF_BUFFER_ALIGNMENT)
  #error "Configuration error: REDCONF_BUFFER_ALIGNMENT must be a power of two"
#endif


/*  A note on the typecasts in the below macros: Operands to bitwise operators
    are subject to the "usual arithmetic conversions".  This means that the
    flags, which have uint16_t values, are promoted to int (if int is larger
    than 16 bits).  To avoid using signed integers in bitwise operations, we
    cast to uint32_t to avoid the integer promotion, then back to uint16_t to
    reflect the actual type.
*/
#define BFLAG_META_MASK (uint16_t)((uint32_t)BFLAG_META_MASTER | BFLAG_META_IMAP | BFLAG_META_INODE | BFLAG_META_INDIR | BFLAG_META_DINDIR | BFLAG_META_DIRECTORY)
#define BFLAG_MASK (uint16_t)((uint32_t)BFLAG_DIRTY | BFLAG_NEW | BFLAG_META_MASK)

/*  Validate the type bits in the buffer flags.  For file data, all metadata
    bits should be zeroes.  For metadata, exactly one metadata type flag should
    be specified: this is what IS_POWER_OF_2() is checking for.
*/
#define BFLAG_TYPE_IS_VALID(flags) \
    (    (((flags) & BFLAG_META_MASK) == 0U) \
      || IS_POWER_OF_2(((flags) & BFLAG_META_MASK) & ~BFLAG_META))


/*  An invalid block number.  Used to indicate buffers which are not currently
    in use.
*/
#define BBLK_INVALID UINT32_MAX


bool RedBufferIsValid(const uint8_t *pbBuffer, uint16_t uFlags);
#if REDCONF_READ_ONLY == 0
REDSTATUS RedBufferFinalize(uint8_t *pbBuffer, uint8_t bVolNum, uint16_t uFlags);
#endif
#ifdef REDCONF_ENDIAN_SWAP
void RedBufferEndianSwap(void *pBuffer, uint16_t uFlags);
#endif


#endif /* REDBUFFERPRIV_H */
