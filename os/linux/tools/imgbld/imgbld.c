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
    @brief Implements a Win32 command-line image builder tool
*/
//#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <redfs.h>

#if REDCONF_IMAGE_BUILDER == 1

#include <redvolume.h>
#include <redgetopt.h>
#include <redtoolcmn.h>
#include <redcoreapi.h>

#include "../wintlcmn.h"
#include "ibheader.h"


#define COPY_BUFFER_SIZE_MIN    (1024U)
#define COPY_BUFFER_SIZE_MAX    (32UL * 1024 * 1024)


static void TryParsePrgmArgs(int argc, char *argv[], IMGBLDOPTIONS *pOptions);
static void Usage(const char *pszProgramName, bool fError);
static bool PathNamesVolume(const char *pszPath);


void *gpCopyBuffer = NULL;
uint32_t gulCopyBufferSize;


/** @brief Entry point for the Reliance Edge image builder utility.

    @param argc The size of the @p argv array.
    @param argv The arguments to the program.

    @return Zero on success, nonzero on failure.
*/
int main(
    int             argc,
    char           *argv[])
{
    IMGBLDOPTIONS   options;
    int             ret = 0;

    TryParsePrgmArgs(argc, argv, &options);

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

      #if REDCONF_API_POSIX == 0
        if(ret == 0)
        {
            if(options.pszMapFile != NULL)
            {
                ret = GetFileList(options.pszMapFile, options.pszInputDir, &psFileListHead);
            }
            else
            {
                ret = CreateFileListWin(options.pszInputDir, &psFileListHead);
            }
        }
      #endif

        if(ret == 0)
        {
            REDSTATUS err = RedOsBDevConfig(options.bVolNumber, options.pszOutputFile);

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
            if(RedCoreVolSetCurrent(options.bVolNumber) != 0)
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
            ret = IbPosixCopyDir(options.pszVolName, options.pszInputDir);
        }
      #else

        if(ret == 0)
        {
            ret = IbFseCopyFiles(options.bVolNumber, psFileListHead);
        }

        if((ret == 0) && (options.pszDefineFile != NULL))
        {
            ret = OutputDefinesFile(psFileListHead, &options);
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
            printf("Successfully created Reliance Edge image at %s.\n", options.pszOutputFile);
        }
        else
        {
            fprintf(stderr, "Error creating Reliance Edge image.\n");

            if(fFormatted && !PathNamesVolume(options.pszOutputFile))
            {
                fprintf(stderr, "Removing image file %s\n", options.pszOutputFile);
                if(remove(options.pszOutputFile) != 0)
                {
                    fprintf(stderr, "Error removing image file.\n");
                }
            }
        }
    }

    return -ret;
}


/** @brief Helper function to parse command line arguments

    @brief argc     The number of arguments.
    @brief argv     The argument array.
    @param pOptions IMGBLDOPTIONS structure to fill
*/
void TryParsePrgmArgs(
    int             argc,
    char           *argv[],
    IMGBLDOPTIONS  *pOptions)
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
    memset(pOptions, 0, sizeof(*pOptions));

    while((c = RedGetoptLong(argc, argv, pszOptions, aLongopts, NULL)) != -1)
    {
        switch(c)
        {
            case 'i': /* --dir */
                pOptions->pszInputDir = red_optarg;
                break;
          #if REDCONF_API_FSE == 1
            case 'm': /* --map */
                pOptions->pszMapFile = red_optarg;
                break;
            case 'd': /* --defines */
                pOptions->pszDefineFile = red_optarg;
                break;
            case 'W': /* --no-warn */
                pOptions->fNowarn = true;
                break;
          #endif
            case 'D': /* --dev */
                pOptions->pszOutputFile = MassageDriveName(red_optarg);
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

    pOptions->bVolNumber = RedFindVolumeNumber(argv[red_optind]);
    if(pOptions->bVolNumber == REDCONF_VOLUME_COUNT)
    {
        fprintf(stderr, "Error: \"%s\" is not a valid volume identifier.\n", argv[red_optind]);
        goto BadOpt;
    }

  #if REDCONF_API_POSIX == 1
    pOptions->pszVolName = gaRedVolConf[pOptions->bVolNumber].pszPathPrefix;
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
    if(pOptions->pszInputDir == NULL)
    {
        fprintf(stderr, "Input directory must be specified (--dir).\n");
        goto BadOpt;
    }
  #else
    if((pOptions->pszInputDir == NULL) && (pOptions->pszMapFile == NULL))
    {
        fprintf(stderr, "Either input directory (--dir) or input file map (--map) must be specified.\n");
        goto BadOpt;
    }
  #endif

    if(pOptions->pszOutputFile == NULL)
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
"      (e.g., red.bin); or an OS-specific reference to a device (on Windows, a\n"
"      drive letter like G: or a device name like \\\\.\\PhysicalDrive7; the\n"
"      latter might be better than using a drive letter, which might only format\n"
"      a partition instead of the entire physical media).\n"
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

/*  Checks whether the given path appears to name a volume or not. Expects the
    path to be in massaged "//./diskname" format if it names a volume.
*/
static bool PathNamesVolume(
    const char *pszPath)
{
    return ((pszPath[0] == '\\')
         && (pszPath[1] == '\\')
         && (pszPath[2] == '.')
         && (pszPath[3] == '\\')
         && (strchr(&pszPath[4], '\\') == NULL)
         && (strchr(&pszPath[4], '/') == NULL));
}

#else

/** @brief Stubbed entry point for the Reliance Edge image builder.

    @return Returns 1
*/
int main(void)
{
    fprintf(stderr, "Reliance Edge image builder tool disabled\n");
    return 1;
}

#endif

