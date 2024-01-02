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
    @brief Implements a Linux command-line front-end for the Reliance Edge file
           system formatter.
*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include <redfs.h>

#if (REDCONF_READ_ONLY == 0) && (((REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FORMAT == 1)) || (REDCONF_IMAGE_BUILDER == 1))

#include <redgetopt.h>
#include <redtoolcmn.h>
#include <redcoreapi.h>


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
    REDFMTOPT       fo = {0U};
    uint8_t         bVolNum;
    REDSTATUS       ret;
    const REDOPTION aLongopts[] =
    {
        { "version", red_required_argument, NULL, 'V' },
        { "inodes", red_required_argument, NULL, 'N' },
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

    while((c = RedGetoptLong(argc, argv, "V:N:D:H", aLongopts, NULL)) != -1)
    {
        switch(c)
        {
            case 'V': /* --version */
            {
                unsigned long   ulVer;
                char           *pszEnd;

                errno = 0;
                ulVer = strtoul(red_optarg, &pszEnd, 10);
                if((ulVer == ULONG_MAX) || (errno != 0) || (*pszEnd != '\0'))
                {
                    fprintf(stderr, "Invalid on-disk layout version number: %s\n", red_optarg);
                    goto BadOpt;
                }
                if(!RED_DISK_LAYOUT_IS_SUPPORTED(ulVer))
                {
                    fprintf(stderr, "Unsupported on-disk layout version number: %lu\n", ulVer);
                    goto BadOpt;
                }

                fo.ulVersion = (uint32_t)ulVer;
                break;
            }
            case 'N': /* --inodes */
            {
                unsigned long ulInodes;

                if(strcmp(red_optarg, "auto") == 0)
                {
                    ulInodes = RED_FORMAT_INODE_COUNT_AUTO;
                }
                else
                {
                    char *pszEnd;

                    errno = 0;
                    ulInodes = strtoul(red_optarg, &pszEnd, 10);
                    if((ulInodes == 0U) || (ulInodes == ULONG_MAX) || (errno != 0) || (*pszEnd != '\0'))
                    {
                        fprintf(stderr, "Invalid inode count: %s\n", red_optarg);
                        goto BadOpt;
                    }
                  #if ULONG_MAX > UINT32_MAX
                    if(ulInodes > UINT32_MAX)
                    {
                        fprintf(stderr, "Invalid inode count: %lu\n", ulInodes);
                        goto BadOpt;
                    }
                  #endif
                }

                fo.ulInodeCount = (uint32_t)ulInodes;
                break;
            }
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

    ret = RedCoreVolFormat(&fo);
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
    return 0; /* Unreachable, but keep it to suppress warnings */

  BadOpt:
    fprintf(stderr, "Invalid command line arguments\n");
    Usage(argv[0U], true);
    return 0; /* Unreachable, but keep it to suppress warnings */
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
    int         iExitStatus = fError ? 1 : 0;
    FILE       *pOut = fError ? stderr : stdout;
    static const char szUsage[] =
"usage: %s VolumeID --dev=devname [--version=layout_ver] [--inodes=count] [--help]\n"
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
"      (e.g., red.bin); or an OS-specific reference to a device (on Linux, a\n"
"      device file like /dev/sdb).\n"
"  --version=layout_ver, -V layout_ver\n"
"      Specify the on-disk layout version to use.  If unspecified, the default\n"
"      is %u.  With the current file system configuration, supported version(s)\n"
"      are: %s.\n"
"  --inodes=count, -N count\n"
"      Specify the inode count to use.  If unspecified, the inode count in the\n"
"      volume configuration is used.  A value of \"auto\" may be specified to\n"
"      automatically compute an appropriate inode count for the volume size.\n"
"  --help, -H\n"
"      Prints this usage text and exits.\n\n";

    fprintf(pOut, szUsage, pszProgramName, RED_DISK_LAYOUT_VERSION, RED_DISK_LAYOUT_SUPPORTED_STR);
    exit(iExitStatus);
}

#else

#error "Misconfigured host redconf.h file: the formatter should be enabled!"

#endif


