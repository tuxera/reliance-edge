/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2021 Tuxera US Inc.
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
    comply with the terms of the GPLv2 license must obtain a commercial license
    before incorporating Reliance Edge into proprietary software for
    distribution in any form.  Visit http://www.datalight.com/reliance-edge for
    more information.
*/
/** @file
    @brief Defines macros used to interact with the Reliance Edge API.
*/
#ifndef REDAPIMACS_H
#define REDAPIMACS_H


/** Mount the volume as read-only. */
#define RED_MOUNT_READONLY  0x00000001U

/** Mount the volume with automatic discards enabled. */
#define RED_MOUNT_DISCARD   0x00000002U

#if (REDCONF_READ_ONLY == 0) && (RED_KIT != RED_KIT_GPL)
/** Mask of all supported mount flags. */
#define RED_MOUNT_MASK      (RED_MOUNT_READONLY | RED_MOUNT_DISCARD)
#else
/** Mask of all supported mount flags. */
#define RED_MOUNT_MASK      RED_MOUNT_READONLY
#endif

/** @brief Default mount flags.

    These are the mount flags that are used when Reliance Edge is mounted via an
    API which does not allow mount flags to be specified: viz., red_mount() or
    RedFseMount().  If red_mount2() is used, the flags provided to it supersede
    these flags.
*/
#define RED_MOUNT_DEFAULT   (RED_MOUNT_DISCARD & RED_MOUNT_MASK)


/** Force unmount, closing all open handles. */
#define RED_UMOUNT_FORCE    0x00000001U

/** Mask of all supported unmount flags. */
#define RED_UMOUNT_MASK     RED_UMOUNT_FORCE

/** Defualt unmount flags. */
#define RED_UMOUNT_DEFAULT  0U


/** Clear all events: manual transactions only. */
#define RED_TRANSACT_MANUAL     0x00000000U

/** Transact prior to unmounting in red_umount(), red_umount2(), or RedFseUnmount(). */
#define RED_TRANSACT_UMOUNT     0x00000001U

/** Transact after a successful red_open() which created a file. */
#define RED_TRANSACT_CREAT      0x00000002U

/** Transact after a successful red_unlink() or red_rmdir(). */
#define RED_TRANSACT_UNLINK     0x00000004U

/** Transact after a successful red_mkdir(). */
#define RED_TRANSACT_MKDIR      0x00000008U

/** Transact after a successful red_rename(). */
#define RED_TRANSACT_RENAME     0x00000010U

/** Transact after a successful red_link(). */
#define RED_TRANSACT_LINK       0x00000020U

/** Transact after a successful red_close(). */
#define RED_TRANSACT_CLOSE      0x00000040U

/** Transact after a successful red_write() or RedFseWrite(). */
#define RED_TRANSACT_WRITE      0x00000080U

/** Transact after a successful red_fsync(). */
#define RED_TRANSACT_FSYNC      0x00000100U

/** Transact after a successful red_ftruncate(), RedFseTruncate(), or red_open() with RED_O_TRUNC that actually truncates. */
#define RED_TRANSACT_TRUNCATE   0x00000200U

/** Transact to free space in disk full situations. */
#define RED_TRANSACT_VOLFULL    0x00000400U

/** Transact after a successful os sync. */
#define RED_TRANSACT_SYNC       0x00000800U

#if REDCONF_READ_ONLY == 1

/** @brief Mask of all supported automatic transaction events. */
#define RED_TRANSACT_MASK       0U

#elif REDCONF_API_POSIX == 1

/** @brief Mask of all supported automatic transaction events.
*/
#define RED_TRANSACT_MASK                                                   \
(                                                                           \
    RED_TRANSACT_SYNC                                                   |   \
    RED_TRANSACT_UMOUNT                                                 |   \
    RED_TRANSACT_CREAT                                                  |   \
    ((REDCONF_API_POSIX_UNLINK    == 1) ? RED_TRANSACT_UNLINK   : 0U)   |   \
    ((REDCONF_API_POSIX_MKDIR     == 1) ? RED_TRANSACT_MKDIR    : 0U)   |   \
    ((REDCONF_API_POSIX_RENAME    == 1) ? RED_TRANSACT_RENAME   : 0U)   |   \
    ((REDCONF_API_POSIX_LINK      == 1) ? RED_TRANSACT_LINK     : 0U)   |   \
    RED_TRANSACT_CLOSE                                                  |   \
    RED_TRANSACT_WRITE                                                  |   \
    RED_TRANSACT_FSYNC                                                  |   \
    ((REDCONF_API_POSIX_FTRUNCATE == 1) ? RED_TRANSACT_TRUNCATE : 0U)   |   \
    RED_TRANSACT_VOLFULL                                                    \
)

#else /* REDCONF_API_FSE == 1 */

/** @brief Mask of all supported automatic transaction events.
*/
#define RED_TRANSACT_MASK                                               \
(                                                                       \
    RED_TRANSACT_UMOUNT                                             |   \
    RED_TRANSACT_WRITE                                              |   \
    ((REDCONF_API_FSE_TRUNCATE == 1) ? RED_TRANSACT_TRUNCATE : 0U)  |   \
    RED_TRANSACT_VOLFULL                                                \
)

#endif /* REDCONF_READ_ONLY */

#if (REDCONF_TRANSACT_DEFAULT & RED_TRANSACT_MASK) != REDCONF_TRANSACT_DEFAULT
#error "Configuration error: invalid value of REDCONF_TRANSACT_DEFAULT"
#endif


#endif

