/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                   Copyright (c) 2014-2015 Datalight, Inc.
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
    comply with the terms of the GPLv2 license may obtain a commercial license
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

/** @brief Prompt the user to confirm an operation by typing in y or n.

    @param pszMessage   The message to show the user to prompt for input. The
                        string " [y/n] " is appended to the same line.

    @return Whether the user typed a y to confirm the operation.
*/
bool ConfirmOperation(
    const char *pszMessage)
{
    int iAnswer;
    int iChar;

    fprintf(stderr, "%s [y/n] ", pszMessage);
    fflush(stderr);

    while(true)
    {
        iAnswer = getchar();

        /*  Burn through the rest of the answer.  If the user typed
            "Affirmative", we don't want to complain twelve times.
        */
        iChar = iAnswer;
        while(iChar != '\n')
        {
            iChar = getchar();
        }

        if((iAnswer == 'y') || (iAnswer == 'Y') || (iAnswer == 'n') || (iAnswer == 'N'))
        {
            break;
        }

        fprintf(stderr, "Answer 'y' or 'n': ");
        fflush(stderr);
    }

    return ((iAnswer == 'y') || (iAnswer == 'Y'));
}

