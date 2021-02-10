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
    @brief Implements a Win32 command-line image builder tool
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <redfs.h>

#if REDCONF_IMAGE_BUILDER == 1

#include <redvolume.h>
#include <redgetopt.h>
#include <redtoolcmn.h>
#include <redcoreapi.h>
#include <redtools.h>


#define COPY_BUFFER_SIZE_MIN    (1024U)
#define COPY_BUFFER_SIZE_MAX    (32UL * 1024 * 1024)


static void Usage(const char *pszProgramName, bool fError);


void *gpCopyBuffer = NULL;
uint32_t gulCopyBufferSize;


/** @brief Entry point for the Reliance Edge image builder utility.

    @param pParam   Params structure for the image builder tool.

    @return Zero on success, nonzero on failure.
*/
int ImgbldStart(
    IMGBLDPARAM    *pParam)
{
    int             ret = 0;

    /*  Prints sign-on message
    */
    ret = IbApiInit();

    if(ret == 0)
    {
      #if REDCONF_API_POSIX == 0
        FILELISTENTRY  *psFileListHead = NULL;
      #endif

        /*  Keep track of whether the target device has been formatted. If an
            operation fails before the device is formatted, then the image file
            does not need to be deleted.
        */
        bool            fFormatted = false;

      #if REDCONF_API_FSE == 1
        if(ret == 0)
        {
            if(pParam->pszMapFile != NULL)
            {
                ret = IbFseGetFileList(pParam->pszMapFile, pParam->pszInputDir, &psFileListHead);
            }
            else
            {
                ret = IbFseBuildFileList(pParam->pszInputDir, &psFileListHead);
            }
        }
      #endif

        if(ret == 0)
        {
            REDSTATUS err = RedOsBDevConfig(pParam->bVolNumber, pParam->pszOutputFile);

            if(err != 0)
            {
                ret = -1;
                if(err == -RED_EINVAL)
                {
                    fprintf(stderr, "Invalid volume number or empty output file name.\n");
                }
                else
                {
                    REDERROR();
                }
            }
        }

        if(ret == 0)
        {
            if(RedCoreVolSetCurrent(pParam->bVolNumber) != 0)
            {
                REDERROR();
                ret = -1;
            }
        }

        if(ret == 0)
        {
            REDSTATUS formaterr = RedCoreVolFormat();

            fFormatted = true;

            if(formaterr != 0)
            {
                ret = -1;
                fprintf(stderr, "Error number %d formatting volume.\n", -formaterr);
            }
        }

        if(ret == 0)
        {
            gulCopyBufferSize = COPY_BUFFER_SIZE_MAX;

            while((ret == 0) && (gpCopyBuffer == NULL))
            {
                gpCopyBuffer = malloc(gulCopyBufferSize);

                if(gpCopyBuffer == NULL)
                {
                    /*  Reloop and try allocating a smaller portion unless we're
                        already down to the minimum allowed size.
                    */
                    if(gulCopyBufferSize <= COPY_BUFFER_SIZE_MIN)
                    {
                        ret = -1;
                        fprintf(stderr, "Error: out of memory.\n");
                        break;
                    }

                    gulCopyBufferSize /= 2;
                }
            }
        }

      #if REDCONF_API_POSIX == 1
        if(ret == 0)
        {
            ret = IbPosixCopyDir(pParam->pszVolName, pParam->pszInputDir);
        }
      #else

        if(ret == 0)
        {
            ret = IbFseCopyFiles(pParam->bVolNumber, psFileListHead);
        }

        if((ret == 0) && (pParam->pszDefineFile != NULL))
        {
            ret = IbFseOutputDefines(psFileListHead, pParam);
        }

        FreeFileList(&psFileListHead);
      #endif

        if(gpCopyBuffer != NULL)
        {
            free(gpCopyBuffer);
        }

        if(IbApiUninit() != 0)
        {
            ret = -1;
        }

        if(ret == 0)
        {
            printf("Successfully created Reliance Edge image at %s.\n", pParam->pszOutputFile);
        }
        else
        {
            fprintf(stderr, "Error creating Reliance Edge image.\n");

            if(fFormatted && IsRegularFile(pParam->pszOutputFile))
            {
                fprintf(stderr, "\nRemoving image file %s\n", pParam->pszOutputFile);
                if(remove(pParam->pszOutputFile) != 0)
                {
                    fprintf(stderr, "Error removing image file.\n");
                }
            }
        }
    }

    return -ret;
}


/** @brief Helper function to parse command line arguments.

    Does not return if params are invalid. (Prints usage and exits).

    @brief argc     The number of arguments.
    @brief argv     The argument array.
    @param pParam   IMGBLDPARAM structure to fill
*/
void ImgbldParseParams(
    int             argc,
    char           *argv[],
    IMGBLDPARAM    *pParam)
{
    int32_t         c;
    const REDOPTION aLongopts[] =
    {
        { "dir", red_required_argument, NULL, 'i' },
      #if REDCONF_API_FSE == 1
        { "map", red_required_argument, NULL, 'm' },
        { "defines", red_required_argument, NULL, 'd' },
        { "no-warn", red_no_argument, NULL, 'W' },
      #endif
        { "dev", red_required_argument, NULL, 'D' },
        { "help", red_no_argument, NULL, 'H' },
        { NULL }
    };
  #if REDCONF_API_FSE == 1
    const char     *pszOptions = "i:m:d:WD:H";
  #else
    const char     *pszOptions = "i:D:H";
  #endif

    /*  If run without parameters, treat as a help request.
    */
    if(argc <= 1)
    {
        goto Help;
    }

    /*  Set default parameters.
    */
    memset(pParam, 0, sizeof(*pParam));

    while((c = RedGetoptLong(argc, argv, pszOptions, aLongopts, NULL)) != -1)
    {
        switch(c)
        {
            case 'i': /* --dir */
                pParam->pszInputDir = red_optarg;
                break;
          #if REDCONF_API_FSE == 1
            case 'm': /* --map */
                pParam->pszMapFile = red_optarg;
                break;
            case 'd': /* --defines */
                pParam->pszDefineFile = red_optarg;
                break;
            case 'W': /* --no-warn */
                pParam->fNowarn = true;
                break;
          #endif
            case 'D': /* --dev */
                pParam->pszOutputFile = red_optarg;
                break;
            case 'H': /* --help */
                goto Help;
            case '?': /* Unknown or ambiguous option */
            case ':': /* Option missing required argument */
            default:
                goto BadOpt;
        }
    }

    /*  RedGetoptLong() has permuted argv to move all non-option arguments to
        the end.  We expect to find a volume identifier.
    */
    if(red_optind >= argc)
    {
        fprintf(stderr, "Missing volume argument\n");
        goto BadOpt;
    }

    pParam->bVolNumber = RedFindVolumeNumber(argv[red_optind]);
    if(pParam->bVolNumber == REDCONF_VOLUME_COUNT)
    {
        fprintf(stderr, "Error: \"%s\" is not a valid volume identifier.\n", argv[red_optind]);
        goto BadOpt;
    }

  #if REDCONF_API_POSIX == 1
    pParam->pszVolName = gaRedVolConf[pParam->bVolNumber].pszPathPrefix;
  #endif

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

  #if REDCONF_API_POSIX == 1
    if(pParam->pszInputDir == NULL)
    {
        fprintf(stderr, "Input directory must be specified (--dir).\n");
        goto BadOpt;
    }
  #else
    if((pParam->pszInputDir == NULL) && (pParam->pszMapFile == NULL))
    {
        fprintf(stderr, "Either input directory (--dir) or input file map (--map) must be specified.\n");
        goto BadOpt;
    }
  #endif

    if(pParam->pszOutputFile == NULL)
    {
        fprintf(stderr, "Output device (--dev) must be specified.\n");
        goto BadOpt;
    }

    return;

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
    FILE   *fout = fError ? stderr : stdout;

  #if REDCONF_API_POSIX == 1
    fprintf(fout,
"usage: %s VolumeID --dev=devname --dir=inputDir [--help]\n"
"Build a Reliance Edge volume image which includes the given set of input files.\n"
"\n"
"Where:\n"
"  VolumeID\n"
"      A volume number (e.g., 2) or a volume path prefix (e.g., VOL1: or /data)\n"
"      of the volume to format.\n"
"  --dev=devname, -D devname\n"
"      Specifies the device name.  This can be the path and name of a file disk\n"
  #if _WIN32
"      (e.g., red.bin); or an OS-specific reference to a device (on Windows, a\n"
"      drive letter like G: or a device name like \\\\.\\PhysicalDrive7; the\n"
"      latter might be better than using a drive letter, which might only format\n"
"      a partition instead of the entire physical media).\n"
  #else
"      (e.g., red.bin); or an OS-specific reference to a device (on Linux, a\n"
"      device file like /dev/sdb).\n"
  #endif
"  --dir=inputDir, -i inputDir\n"
"      A path to a directory that contains all of the files to be copied into\n"
"      the image.\n"
"  --help, -H\n"
"      Prints this usage text and exits.\n\n", pszProgramName);
  #else
    fprintf(fout,
"usage: %s VolumeID --dev=devname [--dir=inputDir] [--map=mappath]\n"
"                          [--defines=file] [--help]\n"
"Build a Reliance Edge volume image which includes the given set of input files.\n"
"\n"
"Where:\n"
"  VolumeID\n"
"      A volume number (e.g., 2) of the volume to format.\n"
"  --dev=devname, -D devname\n"
"      Specifies the device name.  This can be the path and name of a file disk\n"
"      (e.g., red.bin); or an OS-specific reference to a device (on Windows, a\n"
"      drive letter like G: or a device name like \\\\.\\PhysicalDrive7; the\n"
"      latter might be better than using a drive letter, which might only format\n"
"      a partition instead of the entire physical media).\n"
"  --dir=inputDir, -i inputDir\n"
"      A path to a directory that contains all of the files to be copied into\n"
"      the image.  If not specified, the file at --map=mappath must contain full\n"
"      absolute file paths for all input files.\n"
"  --map=mappath, -m mappath\n"
"      Path to the file which maps file names (or paths) in --dir=inputDir to\n"
"      file indices in the outputted image.\n"
"  --defines=file, -d file\n"
"      Path to the file to which to store a set of #define statements for\n"
"      accessing files by assigned index if --map=mappath is not specified.\n"
"  --no-warn, -W\n"
"      Replace the --defines file if it exists without prompting.\n"
"  --help, -H\n"
"      Prints this usage text and exits.\n\n", pszProgramName);
  #endif

    exit(fError ? 1 : 0);
}


#endif
