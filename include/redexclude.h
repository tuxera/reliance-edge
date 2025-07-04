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
#ifndef REDEXCLUDE_H
#define REDEXCLUDE_H


#define DELETE_SUPPORTED \
  ( \
       (REDCONF_READ_ONLY == 0) \
    && (    (REDCONF_API_POSIX == 1) \
         && (    (REDCONF_API_POSIX_RMDIR == 1) \
              || (REDCONF_API_POSIX_UNLINK == 1) \
              || ((REDCONF_API_POSIX_RENAME == 1) && (REDCONF_RENAME_ATOMIC == 1)))))

#define TRUNCATE_SUPPORTED \
  ( \
       (REDCONF_READ_ONLY == 0) \
    && (    ((REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FTRUNCATE == 1)) \
         || ((REDCONF_API_FSE == 1) && (REDCONF_API_FSE_TRUNCATE == 1))))

#define FORMAT_SUPPORTED \
    ( \
         (REDCONF_READ_ONLY == 0) \
      && (    ((REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FORMAT == 1)) \
           || ((REDCONF_API_FSE == 1) && (REDCONF_API_FSE_FORMAT == 1)) \
           || (REDCONF_IMAGE_BUILDER == 1)))

#define DISCARD_SUPPORTED \
    ( \
         (REDCONF_READ_ONLY == 0) \
      && (    ((REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FSTRIM == 1)) \
           || (REDCONF_DISCARDS == 1)))

#endif
