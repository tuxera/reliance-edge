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
    @brief Implements a Win32 command-line front-end for the Reliance Edge file
           system formatter.
*/
#include <stdio.h>
#include <stdlib.h>

#include <redfs.h>

#if (REDCONF_READ_ONLY == 0) && (((REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FORMAT == 1)) || (REDCONF_IMAGE_BUILDER == 1))

#include <redgetopt.h>
#include <redtoolcmn.h>
#include <redcoreapi.h>
#include "wintlcmn.h"


static void Usage(const char *pszProgramName, bool fError);


/** @brief Entry point for the Reliance Edge file system formatter.

    @param argc The size of the @p argv array.
    @param argv The arguments to the program.

    @return Zero on success, nonzero on failure.
*/
int main(
    int             argc,
    char           *argv[])
{
    int32_t         c;
    const char     *pszDrive = NULL;
    uint8_t         bVolNum;
    REDSTATUS       ret;
    const REDOPTION aLongopts[] =
    {
        { "dev", red_required_argument, NULL, 'D' },
        { "help", red_no_argument, NULL, 'H' },
        { NULL }
    };

    printf("Reliance Edge File System Formatter\n");;

    /*  If run without parameters, treat as a help request.
    */
    if(argc <= 1)
    {
        goto Help;
    }

    while((c = RedGetoptLong(argc, argv, "D:H", aLongopts, NULL)) != -1)
    {
        switch(c)
        {
            case 'D': /* --dev */
                pszDrive = red_optarg;
                break;
            case 'H': /* --help */
                goto Help;
            case '?': /* Unknown or ambiguous option */
            case ':': /* Option missing required argument */
            default:
                goto BadOpt;
        }
    }

    if(pszDrive == NULL)
    {
        fprintf(stderr, "Missing device name argument\n");
        goto BadOpt;
    }

    /*  RedGetoptLong() has permuted argv to move all non-option arguments to
        the end.  We expect to find a volume identifier.
    */
    if(red_optind >= argc)
    {
        fprintf(stderr, "Missing volume argument\n");
        goto BadOpt;
    }

    bVolNum = RedFindVolumeNumber(argv[red_optind]);
    if(bVolNum == REDCONF_VOLUME_COUNT)
    {
        fprintf(stderr, "Error: \"%s\" is not a valid volume identifier.\n", argv[red_optind]);
        goto BadOpt;
    }

    red_optind++; /* Move past volume parameter. */
    if(red_optind < argc)
    {
        int32_t ii;

        for(ii = red_optind; ii < argc; ii++)
        {
            fprintf(stderr, "Error: Unexpected command-line argument \"%s\".\n", argv[ii]);
        }

        goto BadOpt;
    }

    /*  Initialize early on since this also prints the signon message.
    */
    ret = RedCoreInit();
    if(ret != 0)
    {
        fprintf(stderr, "Unexpected error %d from RedCoreInit()\n", (int)ret);
        exit(ret);
    }

    pszDrive = MassageDriveName(pszDrive);
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
        fprintf(stderr, "Format failed with error %d!\n", (int)ret);
        exit(ret);
    }

    return 0;

  Help:
    Usage(argv[0U], false);

  BadOpt:
    fprintf(stderr, "Invalid command line arguments\n");
    Usage(argv[0U], true);
}


/** @brief Print usage information and exit.

    @param pszProgramName   The argv[0] from main().
    @param fError           Whether this function is being invoked due to an
                            invocation error.
*/
static void Usage(
    const char *pszProgramName,
    bool        fError)
{
    static const char szUsage[] =
"usage: %s VolumeID --dev=devname [--help]\n"
"Format a Reliance Edge file system volume.\n"
"\n"
"Where:\n"
"  VolumeID\n"
#if REDCONF_API_POSIX == 1
"      A volume number (e.g., 2) or a volume path prefix (e.g., VOL1: or /data)\n"
"      of the volume to format.\n"
#else
"      A volume number (e.g., 2) of the volume to format.\n"
#endif
"  --dev=devname, -D devname\n"
"      Specifies the device name.  This can be the path and name of a file disk\n"
"      (e.g., red.bin); or an OS-specific reference to a device (on Windows, a\n"
"      drive letter like G: or a device name like \\\\.\\PhysicalDrive7; the\n"
"      latter might be better than using a drive letter, which might only format\n"
"      a partition instead of the entire physical media).\n"
"  --help, -H\n"
"      Prints this usage text and exits.\n\n";

    if(fError)
    {
        fprintf(stderr, szUsage, pszProgramName);
        exit(1);
    }
    else
    {
        printf(szUsage, pszProgramName);
        exit(0);
    }
}

#else

#error "Misconfigured host redconf.h file: the formatter should be enabled!"

#endif


