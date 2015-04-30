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
#include <string.h>

#include <redfs.h>
#include <redtests.h>

#if FSSTRESS_SUPPORTED && (REDCONF_API_POSIX_FORMAT == 1)

#include <redposix.h>
#include <redvolume.h>
#include <wintlcmn.h>


static void Usage(bool fError);


static const char *gpszProgramName;


/** @brief Entry point for the fsstress test.
*/
int main(
    int     argc,
    char   *argv[])
{
    int     iRet = 0;

    gpszProgramName = argv[0U];

    if((argc > 1) && IsHelpRequest(argv[1U]))
    {
        Usage(false);
    }
    else if(argc < 3)
    {
        Usage(true);
    }
    else
    {
        REDSTATUS   ret;
        int32_t     iErr;
        uint8_t     bVolNum;
        const char *pszVolume;
        const char *pszDrive;

        iErr = red_init();
        if(iErr == -1)
        {
            fprintf(stderr, "Unexpected error %d from red_init()\n", (int)red_errno);
            exit(red_errno);
        }

        /*  Parse the command line arguments.
        */
        pszVolume = argv[1U];
        pszDrive = argv[2U];

        bVolNum = FindVolumeNumber(pszVolume);
        if(bVolNum == REDCONF_VOLUME_COUNT)
        {
          #if REDCONF_API_POSIX == 1
            fprintf(stderr, "Error: \"%s\" is not a valid volume number or path prefix.\n", pszVolume);
          #else
            fprintf(stderr, "Error: \"%s\" is not a valid volume number.\n", pszVolume);
          #endif
            Usage(true);
        }

        pszVolume = gaRedVolConf[bVolNum].pszPathPrefix;

        if(_stricmp(pszDrive, "ram") != 0)
        {
            /*  Let the Win32 block device layer know the path/drive to be used
                for this volume's block device.
            */
            pszDrive = MassageDriveName(pszDrive);
            ret = RedOsBDevConfig(bVolNum, pszDrive);
            if(ret != 0)
            {
                fprintf(stderr, "Unexpected error %d from RedOsBDevConfig()\n", (int)ret);
                exit(ret);
            }
        }

        iErr = red_format(pszVolume);
        if(iErr == -1)
        {
            fprintf(stderr, "Unexpected error %d from red_mount()\n", (int)red_errno);
            exit(red_errno);
        }

        iErr = red_mount(pszVolume);
        if(iErr == -1)
        {
            fprintf(stderr, "Unexpected error %d from red_mount()\n", (int)red_errno);
            exit(red_errno);
        }

        /*  Remove volume and drive from the arguments.
        */
        RedMemMove(&argv[1U], &argv[3U], (argc - 3) * sizeof(argv[0U]));
        argc -= 2;

        printf("fsstress begin...\n");
        iRet = fsstress_main(argc, argv);
        printf("fsstress end, return %d\n", iRet);
    }

    return iRet;
}


/** @brief Print usage information and exit.

    @param fError   Whether this function is being invoked due to an invocation
                    error.
*/
static void Usage(
    bool    fError)
{
    static const char szUsage[] =
"usage: %s <volume> <device> <fssargs>\n"
"Run fsstress, a stress test for the POSIX-like API.\n"
"\n"
"Arguments:\n"
"<volume>    A volume number (e.g., 2) or a volume path prefix (e.g., VOL1:\n"
"            or /data) of the volume to test.\n"
"<device>    The block device underlying the volume.  This can be:\n"
"              1) The string \"ram\" to test on a RAM disk;\n"
"              2) The path and name of a file disk (e.g., relrt.bin);\n"
"              3) A drive letter (e.g., G:); or\n"
"              4) A Win32 device name (e.g., \\\\.\\PhysicalDrive7).\n"
"<fssargs>   The numerous arguments to the fsstress test.  For usage information,\n"
"            run this program with just the <volume> and <device> arguments.\n";

    if(fError)
    {
        fprintf(stderr, szUsage, gpszProgramName);
        exit(1);
    }
    else
    {
        fprintf(stdout, szUsage, gpszProgramName);
        exit(0);
    }
}

#else

int main(void)
{
    fprintf(stderr, "fsstress test is not supported in this configuration.\n");
    return 1;
}

#endif


