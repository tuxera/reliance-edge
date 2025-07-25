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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <redfs.h>
#include <redtests.h>

#if FSSTRESS_SUPPORTED && (REDCONF_API_POSIX_FORMAT == 1)

#include <redposix.h>
#include <redvolume.h>


/** @brief Entry point for the fsstress test.
*/
int main(
    int             argc,
    char           *argv[])
{
    int             iRet;
    PARAMSTATUS     pstatus;
    FSSTRESSPARAM   param;
    uint8_t         bVolNum;
    const char     *pszDrive;

    pstatus = FsstressParseParams(argc, argv, &param, &bVolNum, &pszDrive);
    if(pstatus == PARAMSTATUS_OK)
    {
        const char *pszVolume = gaRedVolConf[bVolNum].pszPathPrefix;
        int32_t     iErr;

        iErr = red_init();
        if(iErr == -1)
        {
            fprintf(stderr, "Unexpected error %d from red_init()\n", (int)red_errno);
            exit(red_errno);
        }

        if(pszDrive != NULL)
        {
            REDSTATUS   ret;

            ret = RedOsBDevConfig(bVolNum, pszDrive);
            if(ret != 0)
            {
                fprintf(stderr, "Unexpected error %d from RedOsBDevConfig()\n", (int)ret);
                exit(ret);
            }
        }

        iErr = RedTestFmtOptionsPreserve(pszVolume);
        if(iErr == -1)
        {
            fprintf(stderr, "Unexpected error %d from RedTestFmtOptionsPreserve()\n", (int)red_errno);
            exit(red_errno);
        }

        iErr = red_mount(pszVolume);
        if(iErr == -1)
        {
            fprintf(stderr, "Unexpected error %d from red_mount()\n", (int)red_errno);
            exit(red_errno);
        }

        iErr = red_chdir(pszVolume);
        if(iErr == -1)
        {
            fprintf(stderr, "Unexpected error %d from red_chdir()\n", (int)red_errno);
            exit(red_errno);
        }

        printf("fsstress begin...\n");
        iRet = FsstressStart(&param);
        printf("fsstress end, return %d\n", iRet);
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
    fprintf(stderr, "fsstress test is not supported in this configuration.\n");
    return 1;
}

#endif
