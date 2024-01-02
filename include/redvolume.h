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
#ifndef REDVOLUME_H
#define REDVOLUME_H


#include "redexclude.h" /* for DISCARD_SUPPORTED */
#include <redosconf.h> /* for REDOSCONF_MUTABLE_VOLCONF */


/** Indicates that the sector size should be queried from the block device.
*/
#define SECTOR_SIZE_AUTO    0U

/** Indicates that the sector count should be queried from the block device.
*/
#define SECTOR_COUNT_AUTO   0U


/** Indicates that the inode count should be automatically computed.
*/
#define INODE_COUNT_AUTO    0U


/** @brief Per-volume configuration structure.

    Contains the configuration values that may differ between volumes.  Must be
    declared in an array in redconf.c in the Reliance Edge project directory and
    statically initialized with values representing the volume configuration of
    the target system.
*/
typedef struct
{
    /** The sector size for the block device underlying the volume: the basic
        unit for reading and writing to the storage media.  Commonly ranges
        between 512 and 4096; the full range of permitted values are the
        powers-of-two between 128 and 65536 which are less than or equal to (<=)
        #REDCONF_BLOCK_SIZE.  A value of #SECTOR_SIZE_AUTO (0) indicates that
        the sector size should be queried from the block device.
    */
    uint32_t    ulSectorSize;

    /** The number of sectors in this file system volume.  A value of
        #SECTOR_COUNT_AUTO (0) indicates that the sector count should be queried
        from the block device.
    */
    uint64_t    ullSectorCount;

    /** The number of sectors into the disk where this volume starts.
    */
    uint64_t    ullSectorOffset;

    /** Whether a sector write on the block device underlying the volume is
        atomic.  It is atomic if when the sector write is interrupted, the
        contents of the sector are guaranteed to be either all of the new data,
        or all of the old data.  If unsure, leave as false.
    */
    bool        fAtomicSectorWrite;

    /** This is the default number of inodes for which the formatter will
        reserve space.  The inode count for a volume is the maximum number of
        files and directories that can exist on the volume.  This count includes
        the root directory inode (inode 2; created during format), but does not
        include inodes 0 or 1, which do not exist on disk.  A value of
        #INODE_COUNT_AUTO (0) tells the formatter to pick an inode count which
        is reasonable for the volume size.  The value specified here can be
        overridden at run-time via format options.
    */
    uint32_t    ulInodeCount;

    /** This is the maximum number of times a block device I/O operation will
        be retried.  If a block device read, write, or flush fails, Reliance
        Edge will try again up to this number of times until the operation is
        successful.  Set this to 0 to disable retries.
    */
    uint8_t     bBlockIoRetries;

  #if REDCONF_API_POSIX == 1
    /** The path prefix for the volume; for example, "VOL1:", "FlashDisk", etc.
    */
    const char *pszPathPrefix;
  #endif
} VOLCONF;

#if REDOSCONF_MUTABLE_VOLCONF == 1
  #define VOLCONF_CONST
#else
  #define VOLCONF_CONST const
#endif

extern VOLCONF_CONST VOLCONF gaRedVolConf[REDCONF_VOLUME_COUNT];
extern const VOLCONF * CONST_IF_ONE_VOLUME gpRedVolConf;


/** @brief Per-volume run-time data.
*/
typedef struct
{
    /** Whether the volume is currently mounted.
    */
    bool        fMounted;

  #if REDCONF_READ_ONLY == 0
    /** Whether the volume is read-only.
    */
    bool        fReadOnly;

    /** The active automatic transaction mask.
    */
    uint32_t    ulTransMask;
  #endif

    /** The power of 2 difference between sector size and block size.
    */
    uint8_t     bBlockSectorShift;

    /** The number of logical blocks in this file system volume.  The unit here
        is the global block size.
    */
    uint32_t    ulBlockCount;

    /** The total number of allocable blocks; Also the maximum count of free
        blocks.
    */
    uint32_t    ulBlocksAllocable;

    /** The maximum number of bytes that an inode is capable of addressing.
    */
    uint64_t    ullMaxInodeSize;

    /** The current metadata sequence number.  This value is included in all
        metadata nodes and incremented every time a metadata node is written.
        It is assumed to never wrap around.
    */
    uint64_t    ullSequence;
} VOLUME;

/*  Array of VOLUME structures, populated at during RedCoreInit().
*/
extern VOLUME gaRedVolume[REDCONF_VOLUME_COUNT];

/*  Volume number currently being accessed; populated during
    RedCoreVolSetCurrent().
*/
extern CONST_IF_ONE_VOLUME uint8_t gbRedVolNum;

/*  Pointer to the volume currently being accessed; populated during
    RedCoreVolSetCurrent().
*/
extern VOLUME * CONST_IF_ONE_VOLUME gpRedVolume;

#endif

