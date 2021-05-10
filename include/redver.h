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
    @brief Macros for version numbers, build number, and product information.

    RED_VERSION_SUFFIX, if defined, is a custom string that will be suffixed to
    the version number in the sign-on, replacing the build number.  It should
    only be defined by Tuxera when building a binary delivery: it should not be
    defined when building an SDK.  Since binary deliveries are always
    commercially licensed, defining RED_VERSION_SUFFIX also changes the default
    RED_KIT value.
*/
#ifndef REDVER_H
#define REDVER_H


/** @brief Consecutive number assigned to each automated build.

    <!-- This macro is updated automatically: do not edit! -->
*/
#define RED_BUILD_NUMBER "864-15"

#define RED_KIT_GPL         0U  /* Open source GPL kit. */
#define RED_KIT_COMMERCIAL  1U  /* Commercially-licensed kit. */
#define RED_KIT_SANDBOX     2U  /* Not a kit: developer sandbox. */

/** @brief Indicates the Reliance Edge kit.

    <!-- This macro is updated automatically: do not edit! -->
*/
#ifdef RED_VERSION_SUFFIX
#define RED_KIT RED_KIT_COMMERCIAL
#else
#define RED_KIT RED_KIT_GPL
#endif


/** @brief Version number to display in output.
*/
#define RED_VERSION "v2.5.1a"

/** @brief Version number in hex.

    The most significant byte is the major version number, etc.
*/
#define RED_VERSION_VAL 0x02050161U

/** @brief On-disk version number.

    This is incremented only when the on-disk layout is updated in such a way
    which is incompatible with previously released versions of the file system.
*/
#define RED_DISK_LAYOUT_VERSION 1U


/** @brief Base name of the file system product.
*/
#define RED_PRODUCT_BASE_NAME "Reliance Edge"


/*  Specifies whether the product is in alpha stage, beta stage, or neither.
*/
#if 0
  #if 0
    #define ALPHABETA   " (Alpha)"
  #else
    #define ALPHABETA   " (Beta)"
  #endif
#else
  #define ALPHABETA     ""
#endif

/*  Version suffix defaults to the SDK build number, but it can be otherwise
    defined for binary deliveries where the build number is not meaningful.
*/
#ifdef RED_VERSION_SUFFIX
#define VERSION_SUFFIX_STR "(" RED_VERSION_SUFFIX ")"
#else
#define VERSION_SUFFIX_STR "Build " RED_BUILD_NUMBER
#endif

/** @brief Full product name and version.
*/
#define RED_PRODUCT_NAME "Tuxera " RED_PRODUCT_BASE_NAME " " RED_VERSION " " VERSION_SUFFIX_STR ALPHABETA


/** @brief Product copyright.
*/
#define RED_PRODUCT_LEGAL "Copyright (c) 2014-2021 Tuxera US Inc.  All Rights Reserved Worldwide."


/** @brief Product patents.
*/
#define RED_PRODUCT_PATENT "Patents:  US#7284101."


/** @brief Product edition.
*/
#if RED_KIT == RED_KIT_GPL
#define RED_PRODUCT_EDITION "Open-Source GPLv2 Edition -- Compiled " __DATE__ " at " __TIME__
#elif RED_KIT == RED_KIT_COMMERCIAL
#define RED_PRODUCT_EDITION "Commercial Edition -- Compiled " __DATE__ " at " __TIME__
#else
#define RED_PRODUCT_EDITION "Developer Sandbox -- Compiled " __DATE__ " at " __TIME__
#endif


#endif

