/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2025 Tuxera US Inc.
                      All Rights Reserved Worldwide.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; use version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but "AS-IS," WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, see <https://www.gnu.org/licenses/>.
*/
/*  Businesses and individuals that for commercial or other reasons cannot
    comply with the terms of the GPLv2 license must obtain a commercial
    license before incorporating Reliance Edge into proprietary software
    for distribution in any form.

    Visit https://www.tuxera.com/products/tuxera-edge-fs/ for more information.
*/
/** @file
*/
#ifndef REDCOREMACS_H
#define REDCOREMACS_H


#define BLOCK_NUM_MASTER         (0UL) /* Block number of the master block. */
#define BLOCK_NUM_FIRST_METAROOT (1UL) /* Block number of the first metaroot. */

#define BLOCK_SPARSE        (0U)

#define DINDIR_POINTERS     ((INODE_ENTRIES - REDCONF_DIRECT_POINTERS) - REDCONF_INDIRECT_POINTERS)
#define DINDIR_DATA_BLOCKS  (INDIR_ENTRIES * INDIR_ENTRIES)

/*  Whether INDIR and DINDIR nodes exist with the configured inode pointers.
*/
#define DINDIRS_EXIST       (DINDIR_POINTERS > 0U)
#define INDIRS_EXIST        (REDCONF_DIRECT_POINTERS < INODE_ENTRIES)

#define INODE_INDIR_BLOCKS  (REDCONF_INDIRECT_POINTERS * INDIR_ENTRIES)

/*  With large block sizes, the number of data blocks that a double-indirect can
    point to begins to approach UINT32_MAX.  The total number of data blocks
    addressable by an inode is limited to UINT32_MAX, so it's possible to
    configure the file system with more double-indirect pointers than can be
    used.  The logic below ensures that the number of data blocks in the double-
    indirect range results in at most UINT32_MAX total data blocks per inode.
*/
#define INODE_DINDIR_BLOCKS_MAX (UINT32_MAX - (REDCONF_DIRECT_POINTERS + INODE_INDIR_BLOCKS))
#define DINDIR_POINTERS_MAX \
    (   (INODE_DINDIR_BLOCKS_MAX / DINDIR_DATA_BLOCKS) \
      + (((INODE_DINDIR_BLOCKS_MAX % DINDIR_DATA_BLOCKS) == 0U) ? 0U : 1U))
#if DINDIR_POINTERS_MAX <= DINDIR_POINTERS
#define INODE_DINDIR_BLOCKS INODE_DINDIR_BLOCKS_MAX
#else
#define INODE_DINDIR_BLOCKS (DINDIR_POINTERS * DINDIR_DATA_BLOCKS)
#endif

#define INODE_DATA_BLOCKS   (REDCONF_DIRECT_POINTERS + INODE_INDIR_BLOCKS + INODE_DINDIR_BLOCKS)
#define INODE_SIZE_MAX      (UINT64_SUFFIX(1) * REDCONF_BLOCK_SIZE * INODE_DATA_BLOCKS)

/*  Maximum depth of allocable blocks below the inode, including (if applicable)
    double-indirect node, indirect node, and data block.
*/
#if DINDIRS_EXIST
  #define INODE_MAX_DEPTH   3U
#elif INDIRS_EXIST
  #define INODE_MAX_DEPTH   2U
#else
  #define INODE_MAX_DEPTH   1U
#endif


/*  First inode number that can be allocated.
*/
#if REDCONF_API_POSIX == 1
#define INODE_FIRST_FREE    (INODE_FIRST_VALID + 1U)
#else
#define INODE_FIRST_FREE    (INODE_FIRST_VALID)
#endif

/** @brief Determine if an inode number is valid.
*/
#define INODE_IS_VALID(INODENUM)    (((INODENUM) >= INODE_FIRST_VALID) && ((INODENUM) < (INODE_FIRST_VALID + gpRedCoreVol->ulInodeCount)))


/*  The number of blocks reserved to allow a truncate or delete operation to
    complete when the disk is otherwise full.

    The more expensive of the two operations is delete, which has to actually
    write to a file data block to remove the directory entry.
*/
#if REDCONF_READ_ONLY == 1
  #define RESERVED_BLOCKS 0U
#elif (REDCONF_API_POSIX == 1) && ((REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1))
  #if DINDIRS_EXIST
    #define RESERVED_BLOCKS 3U
  #elif REDCONF_INDIRECT_POINTERS > 0U
    #define RESERVED_BLOCKS 2U
  #else
    #define RESERVED_BLOCKS 1U
  #endif
#elif ((REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FTRUNCATE == 1)) || ((REDCONF_API_FSE == 1) && (REDCONF_API_FSE_TRUNCATE == 1))
  #if DINDIRS_EXIST
    #define RESERVED_BLOCKS 2U
  #elif REDCONF_INDIRECT_POINTERS > 0U
    #define RESERVED_BLOCKS 1U
  #else
    #define RESERVED_BLOCKS 0U
  #endif
#else
  #define RESERVED_BLOCKS 0U
#endif


#define CRITICAL_ASSERT(EXP)    ((EXP) ? (void)0 : CRITICAL_ERROR())
#ifdef __FILE_NAME__
#define CRITICAL_ERROR()        RedVolCriticalError(__FILE_NAME__, __LINE__)
#else
#define CRITICAL_ERROR()        RedVolCriticalError(__FILE__, __LINE__)
#endif


#endif
