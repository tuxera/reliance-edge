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
    @brief Implements a utility to find the length of a name.
*/
#include <redfs.h>

#if REDCONF_API_POSIX == 1


/** @brief Determine the length of a name, terminated either by a null or a path
           separator character.

    @param pszName  The name whose length is to be determined.

    @return The length of the name.
*/
uint32_t RedNameLen(
    const char *pszName)
{
    uint32_t    ulIdx = 0U;

    if(pszName == NULL)
    {
        REDERROR();
    }
    else
    {
        while((pszName[ulIdx] != '\0') && (pszName[ulIdx] != REDCONF_PATH_SEPARATOR))
        {
            ulIdx++;
        }
    }

    return ulIdx;
}

#endif
