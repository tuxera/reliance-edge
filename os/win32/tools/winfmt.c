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
    @brief Implements a Win32 command-line front-end for the Reliance Edge file
           system formatter.
*/
#include <stdio.h>
#include <stdlib.h>

#include <redfs.h>

#if (REDCONF_READ_ONLY == 0) && (((REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FORMAT == 1)) || (REDCONF_IMAGE_BUILDER == 1))

#include <redcoreapi.h>
#include "wintlcmn.h"


static void Usage(bool fError);


static const char *gpszFormatterName;


/** @brief Entry point for the Reliance Edge file system formatter.

    @param argc The size of the @p argv array.
    @param argv The arguments to the program.

    @return Zero on success, nonzero on failure.
*/
int main(
    int     argc,
    char   *argv[])
{
    gpszFormatterName = argv[0U];

    printf("Reliance Edge File System Formatter\n");

    if((argc > 1) && IsHelpRequest(argv[1U]))
    {
        Usage(false);
    }
    else if(argc != 4)
    {
        Usage(true);
    }
    else
    {
        const char *pszDrive;
        uint8_t     bVolNum;
        REDSTATUS   ret;

        /*  Initialize early on since this also prints the signon message.
        */
        ret = RedCoreInit();
        if(ret != 0)
        {
            fprintf(stderr, "Unexpected error %d from RedCoreInit()\n", (int)ret);
            exit(ret);
        }

        bVolNum = FindVolumeNumber(argv[1U]);
        if(bVolNum == REDCONF_VOLUME_COUNT)
        {
          #if REDCONF_API_POSIX == 1
            fprintf(stderr, "Error: \"%s\" is not a valid volume number or path prefix.\n", argv[1U]);
          #else
            fprintf(stderr, "Error: \"%s\" is not a valid volume number.\n", argv[1U]);
          #endif
            Usage(true);
        }

        if(_stricmp(argv[2U], "/dev") != 0)
        {
            fprintf(stderr, "Error: unexpected argument \"%s\"\n", argv[2U]);
            Usage(true);
        }

        pszDrive = MassageDriveName(argv[3U]);
        ret = RedOsBDevConfig(bVolNum, pszDrive);
        if(ret != 0)
        {
            fprintf(stderr, "Unexpected error %d from RedOsBDevConfig()\n", (int)ret);
            exit(ret);
        }

      #if REDCONF_VOLUME_COUNT > 1U
        ret = RedCoreVolSetCurrent(bVolNum);
        if(ret != 0)
        {
            fprintf(stderr, "Unexpected error %d from RedCoreVolSetCurrent()\n", (int)ret);
            exit(ret);
        }
      #endif

        ret = RedCoreVolFormat();
        if(ret == 0)
        {
            printf("Format successful.\n");
        }
        else
        {
            printf("Format failed with error %d!\n", (int)ret);
            exit(ret);
        }
    }

    return 0;
}


/** @brief Print usage information and exit.

    @param fError   Whether this function is being invoked due to an invocation
                    error.
*/
static void Usage(
    bool    fError)
{
    static const char szUsage[] =
"usage: %s <volume> /dev <device>\n"
"Format a Reliance Edge file system volume.\n"
"\n"
"Arguments:\n"
#if REDCONF_API_POSIX == 1
"<volume>       A volume number (e.g., 2) or a volume path prefix (e.g., VOL1:\n"
"               or /data) of the volume to check.\n"
#else
"<volume>       A volume number (e.g., 2) of the volume to check.\n"
#endif
"/dev <device>  The block device underlying the volume.  This can be:\n"
"                 1) The path and name of a file disk (e.g., red.bin);\n"
"                 2) A drive letter (e.g., G:); or\n"
"                 3) A Win32 device name (e.g., \\\\.\\PhysicalDrive7).  This\n"
"                    might be better than using a drive letter, since the latter\n"
"                    may format a partition instead of the entire physical media.\n";

    if(fError)
    {
        fprintf(stderr, szUsage, gpszFormatterName);
        exit(1);
    }
    else
    {
        fprintf(stdout, szUsage, gpszFormatterName);
        exit(0);
    }
}

#else

#error "Misconfigured host redconf.h file: the formatter should be enabled!"

#endif


