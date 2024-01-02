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
    @brief Interfaces of path utilities for the POSIX-like API layer.
*/
#ifndef REDPATH_H
#define REDPATH_H


REDSTATUS RedPathVolumePrefixLookup(const char *pszPath, uint8_t *pbVolNum);
REDSTATUS RedPathVolumeLookup(const char *pszVolume, uint8_t *pbVolNum);
REDSTATUS RedPathLookup(uint32_t ulDirInode, const char *pszLocalPath, uint32_t ulFlags, uint32_t *pulInode);
REDSTATUS RedPathToName(uint32_t ulDirInode, const char *pszLocalPath, REDSTATUS rootDirError, uint32_t *pulPInode, const char **ppszName);


#endif
