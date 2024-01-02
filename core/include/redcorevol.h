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
#ifndef REDCOREVOL_H
#define REDCOREVOL_H


/** @brief Per-volume run-time data specific to the core.
*/
typedef struct
{
    /** On-disk layout version (copied from the master block).
    */
    uint32_t    ulVersion;

    /** Whether this volume uses the inline imap (true) or external imap
        (false).  Computed at initialization time based on the block count.
    */
    bool        fImapInline;

  #if REDCONF_IMAP_EXTERNAL == 1
    /** First block number of the on-disk imap.  Valid only when fImapInline
        is false.
    */
    uint32_t    ulImapStartBN;

    /** The number of double-allocated imap nodes that make up the imap.
    */
    uint32_t    ulImapNodeCount;
  #endif

    /** Block number where the inode table starts.
    */
    uint32_t    ulInodeTableStartBN;

    /** This is the maximum number of inodes (files and directories).  This
        number includes the root directory inode (inode 2; created during
        format), but does not include inodes 0 or 1, which do not exist on
        disk.  The number of inodes cannot be less than 1.
    */
    uint32_t    ulInodeCount;

    /** First block number that can be allocated.
    */
    uint32_t    ulFirstAllocableBN;

    /** The two metaroot structures, committed and working state.
    */
    METAROOT    aMR[2U];

    /** The index of the current metaroot; must be 0 or 1.
    */
    uint8_t     bCurMR;

    /** Whether the volume has been branched or not.
    */
    bool        fBranched;

    /** The number of blocks which will become free after the next transaction.
    */
    uint32_t    ulAlmostFreeBlocks;

  #if RESERVED_BLOCKS > 0U
    /** Whether to use the blocks reserved for operations that create free
        space.
    */
    bool        fUseReservedBlocks;
  #endif

  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FRESERVE == 1)
    /** The number of inodes which have reserved space.
    */
    uint32_t    ulReservedInodes;

    /** The number of blocks reserved, including file data, indirects and
        double-indirects.
    */
    uint32_t    ulReservedInodeBlocks;

    /** Set to true only when writing to reserved inode space.
    */
    bool        fUseReservedInodeBlocks;
  #endif
} COREVOLUME;

/*  Array of COREVOLUME structures.
*/
extern COREVOLUME gaRedCoreVol[REDCONF_VOLUME_COUNT];

/*  Pointer to the core volume currently being accessed; populated during
    RedCoreVolSetCurrent().
*/
extern COREVOLUME * CONST_IF_ONE_VOLUME gpRedCoreVol;

/*  Pointer to the metaroot currently being accessed; populated during
    RedCoreVolSetCurrent() and RedCoreVolTransact().
*/
extern METAROOT   *gpRedMR;


#endif

