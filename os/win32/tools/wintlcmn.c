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
    @brief Implements certain shared methods for Win32 command line tools.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <redfs.h>
#include <redcoreapi.h>
#include <redvolume.h>

#include "wintlcmn.h"


/** @brief Massage a drive name into a standardized format.

    @return pszDrive    The drive name to massage.

    @return Pointer to the massaged drive name.
*/
const char *MassageDriveName(
    const char *pszDrive)
{
    const char *pszRet;

    /*  If it looks like a drive letter followed by nothing else...
    */
    if(    (    ((pszDrive[0U] >= 'A') && (pszDrive[0U] <= 'Z'))
             || ((pszDrive[0U] >= 'a') && (pszDrive[0U] <= 'z')))
        && (pszDrive[1U] == ':')
        && (    (pszDrive[2U] == '\0')
             || ((pszDrive[2U] == '\\') && (pszDrive[3U] == '\0'))))
    {
        static char szBuffer[8U]; /* Big enough for "\\.\X:" */

        /*  Drives of the form "X:" or "X:\" are converted to "\\.\X:".
        */
        (void)strcpy(szBuffer, "\\\\.\\?:");
        szBuffer[4U] = pszDrive[0U];

        pszRet = szBuffer;
    }
    else
    {
        pszRet = pszDrive;
    }

    return pszRet;
}

