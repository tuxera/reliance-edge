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
#ifndef REDCOREMACS_H
#define REDCOREMACS_H


#define BLOCK_SPARSE        (0U)

#define DINDIR_POINTERS     ((INODE_ENTRIES - REDCONF_DIRECT_POINTERS) - REDCONF_INDIRECT_POINTERS)
#define DINDIR_DATA_BLOCKS  (INDIR_ENTRIES * INDIR_ENTRIES)

#define INODE_INDIR_BLOCKS  (REDCONF_INDIRECT_POINTERS * INDIR_ENTRIES)
#define INODE_DINDIR_BLOCKS (DINDIR_POINTERS * DINDIR_DATA_BLOCKS)
#define INODE_DATA_BLOCKS   (REDCONF_DIRECT_POINTERS + INODE_INDIR_BLOCKS + INODE_DINDIR_BLOCKS)
#define INODE_SIZE_MAX      (1ULL * REDCONF_BLOCK_SIZE * INODE_DATA_BLOCKS)


#define INODE_FIRST_VALID   (INODE_ROOTDIR)          /* First valid inode number. */

/*  First inode number than can be allocated.
*/
#if REDCONF_API_POSIX == 1
#define INODE_FIRST_FREE    (INODE_FIRST_VALID + 1U)
#else
#define INODE_FIRST_FREE    (INODE_FIRST_VALID)
#endif

/** @brief Determine if an inode number is valid.

    All usages of this macro deviate from MISRA-C:2012 Directive 4.9 (advisory).
    To reduce code duplication while maintaining portability and efficiency, a
    function-like macro is used instead of a static inline function.

    As Directive 4.9 is advisory, a deviation record is not required.  This
    notice and the PC-Lint error inhibition option are the only records of the
    deviation.
*/
#define INODE_IS_VALID(INODENUM)    (((INODENUM) >= INODE_FIRST_VALID) && ((INODENUM) < (INODE_FIRST_VALID + gpRedVolConf->ulInodeCount)))


/*  The number of blocks reserved to allow a truncate or delete operation to
    complete when the disk is otherwise full.

    The more expensive of the two operations is delete, which has to actually
    write to a file data block to remove the directory entry.
*/
#if (REDCONF_API_POSIX == 1) && ((REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1))
  #if DINDIR_POINTERS > 0U
    #define RESERVED_BLOCKS 3U
  #elif REDCONF_INDIRECT_POINTERS > 0U
    #define RESERVED_BLOCKS 2U
  #else
    #define RESERVED_BLOCKS 1U
  #endif
#elif ((REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FTRUNCATE == 1)) || ((REDCONF_API_FSE == 1) && (REDCONF_API_FSE_TRUNCATE == 1))
  #if DINDIR_POINTERS > 0U
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
#define CRITICAL_ERROR()        RedVolCriticalError(__FILE__, __LINE__)


#endif

