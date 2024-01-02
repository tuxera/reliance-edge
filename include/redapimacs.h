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
    @brief Defines macros used to interact with the Reliance Edge API.
*/
#ifndef REDAPIMACS_H
#define REDAPIMACS_H


#include "redtransact.h"


/** Mount the volume as read-only. */
#define RED_MOUNT_READONLY      0x00000001U

/** Mount the volume with automatic discards enabled. */
#define RED_MOUNT_DISCARD       0x00000002U

/** Do not finish deletion of any unlinked inodes before returning from mount. */
#define RED_MOUNT_SKIP_DELETE   0x00000004U

/** Mask of all supported mount flags. */
#if REDCONF_API_POSIX == 1
  #define RED_MOUNT_MASK                                                                    \
  (                                                                                         \
      RED_MOUNT_READONLY                                                                |   \
      (((REDCONF_READ_ONLY == 0) && (RED_KIT != RED_KIT_GPL)) ? RED_MOUNT_DISCARD : 0U) |   \
      ((DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1))   ? RED_MOUNT_SKIP_DELETE : 0U)     \
  )
#else
  #define RED_MOUNT_MASK (((REDCONF_READ_ONLY == 0) && (RED_KIT != RED_KIT_GPL)) ? RED_MOUNT_DISCARD : 0U)
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

/** Default unmount flags. */
#define RED_UMOUNT_DEFAULT  0U


#if REDCONF_READ_ONLY == 1

/** @brief Mask of all supported automatic transaction events. */
#define RED_TRANSACT_MASK 0U

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

#if (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1)
/** @brief UID value indicating that the user ID should not be changed. */
#define RED_UID_KEEPSAME    0xFFFFFFFFU

/** @brief GID value indicating that the group ID should not be changed. */
#define RED_GID_KEEPSAME    0xFFFFFFFFU

/** @brief Superuser ID. */
#define RED_ROOT_USER       0U

/** Test for execute or search permission. */
#define RED_X_OK    0x01U

/** Test for write permission. */
#define RED_W_OK    0x02U

/** Test for read permission. */
#define RED_R_OK    0x04U

/** Supported `RED_*_OK` flags. */
#define RED_MASK_OK (RED_X_OK | RED_W_OK | RED_R_OK)
#endif


#endif

