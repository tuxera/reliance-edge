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
#ifndef REDMISC_H
#define REDMISC_H


/** @brief Type of an inode or handle.

    Used to indicate the actual or expected type of an inode or handle.
*/
typedef uint8_t FTYPE;

/** Type is file. */
#define FTYPE_FILE      0x01U

/** Type is directory. */
#define FTYPE_DIR       0x02U

/** Type is symbolic link. */
#define FTYPE_SYMLINK   0x04U

#if REDCONF_API_POSIX == 1
  #if REDCONF_API_POSIX_SYMLINK == 1
   #define FTYPE_ANY        (FTYPE_FILE | FTYPE_DIR | FTYPE_SYMLINK)
   #define FTYPE_NOTDIR     (FTYPE_FILE | FTYPE_SYMLINK)
  #else
    #define FTYPE_ANY       (FTYPE_FILE | FTYPE_DIR)
    #define FTYPE_NOTDIR    FTYPE_FILE
  #endif
#else
  #define FTYPE_ANY         FTYPE_FILE
  #define FTYPE_NOTDIR      FTYPE_FILE
#endif

#endif

