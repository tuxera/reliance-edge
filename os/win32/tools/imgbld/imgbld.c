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
#include <redfs.h>

#if REDCONF_IMAGE_BUILDER == 1

#include <redcoreapi.h>

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "../wintlcmn.h"
#include "ibheader.h"


static int TryParsePrgmArgs(int argc, const char **argv, IMGBLDOPTIONS *pOptions);
static void Usage(bool fError);
static bool PathNamesVolume(const char *pszPath);

static const char *pszPrgmName;

#define COPY_BUFFER_SIZE_MIN    (1024U)
#define COPY_BUFFER_SIZE_MAX    (32UL * 1024 * 1024)

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

    pszPrgmName = argv[0];

    /*  Prints sign-on message
    */
    ret = IbApiInit();

    if(ret == 0)
    {
        ret = TryParsePrgmArgs(argc, argv, &options);
        if(ret != 0)
        {
            Usage(!options.fHelp);
        }
    }

    if((ret == 0) && !options.fHelp)
    {
      #if REDCONF_API_POSIX == 0
        FILELISTENTRY  *psFileListHead = NULL;
      #endif

        /*  Keep track of whether the target device has been formatted. If an
            operation fails before the device is formatted, then the image file
            does not need to be deleted.
        */
        bool            fFormatted = false;

        if(!options.fNowarn)
        {
            bool fWarn = true;

            if(PathNamesVolume(options.pszOutputFile))
            {
                fprintf(stderr, "Are you sure you want to format the volume %s?", options.pszOutputFile);
            }
            else
            {
                ret = CheckFileExists(options.pszOutputFile, &fWarn);

                if(ret != 0)
                {
                    fWarn = false;
                    fprintf(stderr, "Error accessing output device %s\n", options.pszOutputFile);
                }

                if(fWarn)
                {
                    fprintf(stderr, "Output image file %s exists.\nOverwrite?", options.pszOutputFile);
                }
            }

            if(fWarn && !ConfirmOperation(""))
            {
                fprintf(stderr, "Image build operation cancelled.\n");
                ret = -1;
            }
        }

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
            fprintf(stdout, "Successfully created Reliance Edge image at %s.\n", options.pszOutputFile);
        }
        else
        {
            fprintf(stdout, "Error creating Reliance Edge image.\n");

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

    @param pOptions    IMGBLDOPTIONS structure to fill

    @return Returns false if the command line arguments are malformatted or
            otherwise insufficient; including in the case where the user
            requests help. Returns true otherwise.
*/
int TryParsePrgmArgs(
    int                 argc,
    const char        **argv,
    IMGBLDOPTIONS      *pOptions)
{
    const static int    ret_help = 1;

    int                 ret = 0;
    int                 argIndex = 1;

    pOptions->pszInputDir = NULL;
    pOptions->pszOutputFile = NULL;
    pOptions->fHelp = false;
    pOptions->fNowarn = false;
  #if REDCONF_API_POSIX == 1
    pOptions->pszVolName = NULL;
  #else
    pOptions->pszDefineFile = NULL;
    pOptions->pszMapFile = NULL;
  #endif

    if(argc <= argIndex)
    {
        ret = -1;
    }

    if(ret == 0)
    {
        pOptions->fHelp = IsHelpRequest(argv[argIndex]);
        if(pOptions->fHelp)
        {
            /*  Tested for at the end of this function
            */
            ret = ret_help;
        }
    }

    if(ret == 0)
    {
        pOptions->bVolNumber = FindVolumeNumber(argv[argIndex]);
      #if REDCONF_API_POSIX == 1
        pOptions->pszVolName = argv[argIndex];
      #endif

        if(pOptions->bVolNumber == REDCONF_VOLUME_COUNT)
        {
          #if REDCONF_API_POSIX == 1
            fprintf(stderr, "Error: \"%s\" is not a valid path prefix or volume number.\n", argv[argIndex]);
          #else
            fprintf(stderr, "Error: \"%s\" is not a valid volume number.\n", argv[argIndex]);
          #endif
            ret = -1;
        }
    }

    argIndex++;

    /*  Test each param against valid param names and read values passed in.
    */
    while((ret == 0) && (argIndex < argc))
    {
        if(_stricmp(argv[argIndex], "/dir") == 0)
        {
            if(argIndex + 1 >= argc)
            {
                ret = -1;
                break;
            }

            pOptions->pszInputDir = argv[argIndex + 1];

            argIndex += 2;
        }
      #if REDCONF_API_POSIX == 0
        else if(_stricmp(argv[argIndex], "/map") == 0)
        {
            if(argIndex + 1 >= argc)
            {
                ret = -1;
                break;
            }

            pOptions->pszMapFile = argv[argIndex + 1];

            argIndex += 2;
        }
        else if(_stricmp(argv[argIndex], "/defines") == 0)
        {
            if(argIndex + 1 >= argc)
            {
                ret = -1;
                break;
            }

            pOptions->pszDefineFile = argv[argIndex + 1];

            argIndex += 2;
        }
      #endif
        else if(_stricmp(argv[argIndex], "/dev") == 0)
        {
            if(argIndex + 1 >= argc)
            {
                ret = -1;
                break;
            }
            else if(pOptions->pszOutputFile != NULL)
            {
                fprintf(stderr, "Only one device may be specified.\n");
                ret = -1;
                break;
            }

            pOptions->pszOutputFile = MassageDriveName(argv[argIndex + 1]);

            argIndex += 2;
        }
        else if(_stricmp(argv[argIndex], "/nowarn") == 0)
        {
            pOptions->fNowarn = true;

            argIndex++;
        }
        else
        {
            fprintf(stderr, "Unrecognized argument.\n");
            ret = -1;
        }
    }

  #if REDCONF_API_POSIX == 1
    if((ret == 0) && (pOptions->pszInputDir == NULL))
    {
        fprintf(stderr, "Input directory must be specified.\n");
        ret = -1;
    }

  #else

    if((ret == 0) && (pOptions->pszInputDir == NULL) && (pOptions->pszMapFile == NULL))
    {
        fprintf(stderr, "Either input directory or input file map must be specified.\n");
        ret = -1;
    }

  #endif

    if((ret == 0) && (pOptions->pszOutputFile == NULL))
    {
        fprintf(stderr, "Output device must be specified.\n");
        ret = -1;
    }

    if(ret == ret_help)
    {
        ret = 0;
    }

    return ret;
}


/** @brief Print usage information and exit.

    @param fError   Whether this function is being invoked due to an error
*/
void Usage(
    bool    fError)
{
    FILE   *fout = (fError ? stderr : stdout);

  #if REDCONF_API_POSIX == 1
    fprintf(fout,
"usage: %s <volume> /dev <device> /dir <indir> [/nowarn]\n"
"Build a Reliance Edge volume image which includes the given set of input files.\n"
"\n"
"Arguments:\n"
"<volume>       A  volume path prefix (e.g., VOL1: or /data) of the volume to\n"
"               build.\n"
"/dev <device>  The block device underlying the volume to which to write the\n"
"               image.  This can be:\n"
"                 1) The path and name of a file disk (e.g., red.bin);\n"
"                 2) A drive letter (e.g., G:); or\n"
"                 3) A Win32 device name (e.g., \\\\.\\PhysicalDrive7).  This\n"
"                    might be better than using a drive letter, since the latter\n"
"                    may format a partition instead of the entire physical media.\n"
"/dir <indir>   A path to a directory that contains all of the files to be\n"
"               copied into the image.\n"
"/nowarn        Prevents confirmation messages from blocking the interface\n"
"               when overwriting files or formatting a drive.\n", pszPrgmName);
  #else
    fprintf(fout,
        "usage: %s <volume> /dev <device> [/dir <indir>] [/map <mappath>] [/defines <defines>] [/nowarn]\n"
"Build a Reliance Edge volume image which includes the given set of input files.\n"
"\n"
"Arguments:\n"
"<volume>           A volume number (e.g., 2) of the volume to build.\n"
"/dev <device>      The block device underlying the volume to which to write the\n"
"                   image.  This can be:\n"
"                     1) The path and name of a file disk (e.g., red.bin);\n"
"                     2) A drive letter (e.g., G:); or\n"
"                     3) A Win32 device name (e.g., \\\\.\\PhysicalDrive7).\n"
"                        This might be better than using a drive letter, since\n"
"                        the latter may format a partition instead of the entire\n"
"                        physical media.\n"
"/dir <indir>       A path to a directory that contains all of the files to be\n"
"                   copied into the image.  If not specified, the file at\n"
"                   <mappath> must contain full absolute file paths for all\n"
"                   input files.\n"
"/map <mappath>     Path to the file which maps file names (or paths) in\n"
"                   <indir> to file indices in the outputted image.\n"
"/defines <defines> Path to the file to which to store a set of #define\n"
"                   statements for accessing files by assigned index if\n"
"                   mappath is not specified.\n"
"/nowarn            Prevents confirmation messages from blocking the interface\n"
"                   when overwriting files or formatting a drive.\n", pszPrgmName);
  #endif
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
