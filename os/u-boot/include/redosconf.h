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
    @brief Defines OS-specific configuration macros.
*/
#ifndef REDOSCONF_H
#define REDOSCONF_H


/** @brief Whether the POSIX-like API should follow symbolic links.

    When the POSIX-like API is provided with a path whose resolution requires
    following a symbolic link, then:
    - If this setting is enabled, the POSIX-like API will follow the link, using
      its internal implementation.
    - If this setting is disabled, the POSIX-like API will return an error and
      set #red_errno to #RED_ENOLINK.  This is useful if following symbolic
      links needs to be externally implemented, in the VFS.

    If #REDCONF_API_POSIX or #REDCONF_API_POSIX_SYMLINK are false, then this
    setting has no effect.
*/
#define REDOSCONF_SYMLINK_FOLLOW 1

/** @brief Whether the OS overrides the default permissions checks.

    This determines whether RedPermCheck() and RedPermCheckUnlink() defer to
    RedOsPermCheck() and RedOsPermCheckUnlink(), respectively, to check for
    permission.
*/
#define REDOSCONF_PERM_OVERRIDE 0

/** @brief Whether the ::VOLCONF array needs to be writable.

    The ::VOLCONF array was originally intended to be read-only; however, in
    some RTOS ports, being able to modify the array is expedient.

    @note For a new RTOS port, only enable this if you know what you are doing.
*/
#define REDOSCONF_MUTABLE_VOLCONF 0

/** @brief Whether RedOsFakeUidGid() is implemented by the OS services.

    Implementing RedOsFakeUidGid() is optional.  If implemented, it is used by
    test code to override the user ID and group ID with test values.  This
    allows testing functionality that otherwise could not be tested.

    Since RedOsFakeUidGid() is only useful for tests and would be unreachable
    code on a production system, it is only expected to be be implemented by the
    host operating systems.
*/
#define REDOSCONF_FAKE_UID_GID 0


#endif
