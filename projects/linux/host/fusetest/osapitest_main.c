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
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <redfs.h>
#include <redtests.h>

#if POSIX_API_TEST_SUPPORTED == 1


int main(
    int     argc,
    char   *argv[])
{
    int             iRet;
    PARAMSTATUS     pstatus;
    OSAPITESTPARAM  param;

    pstatus = RedOsApiTestParseParams(argc, argv, &param, NULL);
    if(pstatus == PARAMSTATUS_OK)
    {
        iRet = RedOsApiTestStart(&param);
    }
    else if(pstatus == PARAMSTATUS_HELP)
    {
        iRet = 0; /* Help request: do nothing but indicate success. */
    }
    else
    {
        iRet = 1; /* Bad parameters: indicate failure. */
    }

    return iRet;
}

#else

int main(void)
{
    fprintf(stderr, "POSIX-like API test not supported in this configuration.\n");
    return 1;
}

#endif
