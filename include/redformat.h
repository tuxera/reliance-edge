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
    @brief Defines the format option structure.
*/
#ifndef REDFORMAT_H
#define REDFORMAT_H


#include "redexclude.h" /* for FORMAT_SUPPORTED */


#if FORMAT_SUPPORTED

/** @brief Use VOLCONF::ulInodeCount for the volume inode count.
*/
#define RED_FORMAT_INODE_COUNT_CONFIG 0U /* Must be zero! */

/** @brief Automatically compute an appropriate inode count.
*/
#define RED_FORMAT_INODE_COUNT_AUTO UINT32_MAX


/** @brief Configurable format parameters for red_format2().

    @note Members may be added to this structure in the future.  Applications
          should zero-initialize the entire structure to ensure forward
          compatibility.
*/
typedef struct
{
    /** Which on-disk layout version to create.  The defined version values are:
        - #RED_DISK_LAYOUT_ORIGINAL: layout for Reliance Edge v0.9 through v2.5.x
        - #RED_DISK_LAYOUT_DIRCRC: updated v2.6 layout with directory data CRCs.
        - #RED_DISK_LAYOUT_POSIXIER: updated v3.0 layout with support for
          additional POSIX features.
        - #RED_DISK_LAYOUT_VERSION: alias for the default on-disk layout.
        - `0`: alternate alias for the default on-disk layout.

        In certain compile-time configurations, only a subset of the defined
        versions are supported.
    */
    uint32_t ulVersion;

    /** The number of inodes the volume should contain.  Two special values are
        supported:
        - #RED_FORMAT_INODE_COUNT_CONFIG: use the inode count from ::VOLCONF.
        - #RED_FORMAT_INODE_COUNT_AUTO: automatically compute an appropriate
          inode count.

        Any other value will be interpreted as a literal inode count.
    */
    uint32_t ulInodeCount;
} REDFMTOPT;
#endif


#endif /* REDFORMAT_H */
