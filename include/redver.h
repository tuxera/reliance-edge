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
    @brief Macros for version number and product information.
*/
#ifndef REDVER_H
#define REDVER_H


#define RED_KIT_GPL         0U  /* Open source GPL kit. */
#define RED_KIT_COMMERCIAL  1U  /* Commercially-licensed kit. */
#define RED_KIT_SANDBOX     2U  /* Not a kit: developer sandbox. */

/** @brief Indicates the Reliance Edge kit.
*/
#define RED_KIT RED_KIT_GPL


/** @brief Version number to display in output.
*/
#define RED_VERSION "v3.x"

/** @brief Version number in hex.

    The most significant byte is the major version number, etc.
*/
#define RED_VERSION_VAL 0x03FF0000U


/** @brief Original Reliance Edge on-disk layout.

    Used by Reliance Edge v0.9 through v2.5.x.
*/
#define RED_DISK_LAYOUT_ORIGINAL 1U

/** @brief Reliance Edge on-disk layout with directory data CRCs.

    New on-disk layout which adds a metadata header (signature, CRC, and
    sequence number) to the directory data blocks.
*/
#define RED_DISK_LAYOUT_DIRCRC 4U

/** @brief Reliance Edge on-disk layout with additional POSIX support.

    On-disk layout which adds POSIX ownership and permissions, symbolic links,
    and allows inodes to be unlinked while open.
*/
#define RED_DISK_LAYOUT_POSIXIER 5U

/** @brief Minimum on-disk layout required to support the current configuration.

    Enabling certain features will require a newer on-disk layout.
*/
#if (REDCONF_API_POSIX == 1) && ((REDCONF_POSIX_OWNER_PERM == 1) || (REDCONF_DELETE_OPEN == 1) || (REDCONF_API_POSIX_SYMLINK == 1))
  #define RED_DISK_LAYOUT_MINIMUM RED_DISK_LAYOUT_POSIXIER
#else
  #define RED_DISK_LAYOUT_MINIMUM RED_DISK_LAYOUT_ORIGINAL
#endif

/** @brief Maximum on-disk layout supported by the current configuration.

    Enabling certain deprecated features will require an older on-disk layout.
*/
#if (REDCONF_API_POSIX == 1) && (REDCONF_NAME_MAX > (REDCONF_BLOCK_SIZE - 4U /* Inode */ - 16U /* NodeHeader */))
  #define RED_DISK_LAYOUT_MAXIMUM RED_DISK_LAYOUT_ORIGINAL
  #if RED_DISK_LAYOUT_MAXIMUM < RED_DISK_LAYOUT_MINIMUM
    #error "error: REDCONF_NAME_MAX cannot exceed REDCONF_BLOCK_SIZE minus 20 in this configuration"
  #endif
#else
  #define RED_DISK_LAYOUT_MAXIMUM RED_DISK_LAYOUT_POSIXIER
#endif

/** @brief On-disk layouts supported by the current configuration as a string.

    Used by the --help text of the interactive front-ends for the formatter and
    the image builder.
*/
#if RED_DISK_LAYOUT_MAXIMUM == RED_DISK_LAYOUT_ORIGINAL
  #define RED_DISK_LAYOUT_SUPPORTED_STR "1"
#elif RED_DISK_LAYOUT_MINIMUM == RED_DISK_LAYOUT_POSIXIER
  #define RED_DISK_LAYOUT_SUPPORTED_STR "5"
#else
  #define RED_DISK_LAYOUT_SUPPORTED_STR "1, 4, and 5"
#endif

/** @brief Whether an on-disk layout version is supported by _any_ configuration
           of the driver.
*/
#define RED_DISK_LAYOUT_IS_VALID(ver) ( \
       ((ver) == RED_DISK_LAYOUT_ORIGINAL) \
    || ((ver) == RED_DISK_LAYOUT_DIRCRC) \
    || ((ver) == RED_DISK_LAYOUT_POSIXIER))

/** @brief Whether an on-disk layout version is supported by the _current_
           configuration of the driver.
*/
#define RED_DISK_LAYOUT_IS_SUPPORTED(ver) \
    (RED_DISK_LAYOUT_IS_VALID(ver) && ((ver) >= RED_DISK_LAYOUT_MINIMUM) && ((ver) <= RED_DISK_LAYOUT_MAXIMUM))

/** @brief Default on-disk version number.

    The on-disk layout is incremented only when the on-disk layout is updated in
    such a way which is incompatible with previously released versions of the
    file system.

    Version history:
    - 1: Reliance Edge v0.9 through v2.5.x
    - 2: Custom version of Reliance Edge for a specific customer
    - 3: Custom version of Reliance Edge for a specific customer
    - 4: Reliance Edge v2.6+
    - 5: Reliance Edge v3.0+

    The default on-disk version number depends on the file system configuration:
    - None of the features in the newer on-disk layouts are relevant to the
      FSE API, so keep using the original layout for backwards compatibility.
    - The v4+ on-disk layout has a lower maximum name length than the original
      layout.  If the #REDCONF_NAME_MAX value is only legal with the original
      layout, then use it by default.  Doing this avoids breaking existing
      configurations.
    - Certain POSIX-like features require the v5 on-disk layout.
*/
#if (REDCONF_API_FSE == 1) || (RED_DISK_LAYOUT_MAXIMUM < RED_DISK_LAYOUT_DIRCRC)
  #define RED_DISK_LAYOUT_VERSION RED_DISK_LAYOUT_ORIGINAL
#elif RED_DISK_LAYOUT_MINIMUM < RED_DISK_LAYOUT_DIRCRC
  #define RED_DISK_LAYOUT_VERSION RED_DISK_LAYOUT_DIRCRC
#else
  #define RED_DISK_LAYOUT_VERSION RED_DISK_LAYOUT_MINIMUM
#endif


/** @brief Base name of the file system product.
*/
#define RED_PRODUCT_BASE_NAME "Reliance Edge"


/*  Specifies whether the product is in alpha stage, beta stage, or neither.
*/
#if 1
  #if 1
    #define ALPHABETA   " (Alpha)"
  #else
    #define ALPHABETA   " (Beta)"
  #endif
#else
  #define ALPHABETA     ""
#endif

/*  RED_VERSION_SUFFIX, if defined, is a custom string that will be suffixed to
    the version number in the sign-on.
*/
#ifdef RED_VERSION_SUFFIX
#define VERSION_SUFFIX_STR " (" RED_VERSION_SUFFIX ")"
#else
#define VERSION_SUFFIX_STR ""
#endif

/** @brief Full product name and version.
*/
#define RED_PRODUCT_NAME "Tuxera " RED_PRODUCT_BASE_NAME " " RED_VERSION VERSION_SUFFIX_STR ALPHABETA


/** @brief Product copyright.
*/
#define RED_PRODUCT_LEGAL "Copyright (c) 2014-2024 Tuxera US Inc.  All Rights Reserved Worldwide."


/** @brief Product patents.
*/
#define RED_PRODUCT_PATENT "Patents:  US#7284101."


/** @brief Product edition.
*/
#define RED_PRODUCT_EDITION "Open-Source GPLv2 Edition -- Compiled " __DATE__ " at " __TIME__


#endif
