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
    @brief Implements methods of the image builder tool that require Windows
           OS-specific function calls.
*/
#include <redfs.h>

#if REDCONF_IMAGE_BUILDER == 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <ftw.h>        /* Makefile must set _XOPEN_SOURCE >= 500 */

#include <redtools.h>


#if REDCONF_API_POSIX == 1
static int FtwCopyFile(const char *pszPath, const struct stat *pStat, int flag, struct FTW *pFtw);

static const char *gVolName;
static const char *gBaseDir;


/*  @brief  Recurses through a host directory and copies its contents to a
            Reliance Edge volume.

    @param pszVolName   The name of the target Reliance Edge volume.
    @param pszInDir     The path to the directory to copy.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbPosixCopyDirRecursive(
    const char *pszVolName,
    const char *pszInDir)
{
    int32_t     ret = 0;
    struct stat pathStat;

    /* Check that the pszInDir is a directory */
    if (stat(pszInDir, &pathStat))
    {
        fprintf(stderr, "failed to stat %s: ", pszInDir);
        perror("");
        ret = -1;
    }

    if (ret == 0)
    {
        if (!S_ISDIR(pathStat.st_mode))
        {
            fprintf(stderr, "%s is not a dir\n", pszInDir);
            ret = -1;
        }
    }

    if (ret == 0)
    {
        gVolName = pszVolName;
        gBaseDir = pszInDir;

        ret = nftw(pszInDir, FtwCopyFile, 20, 0);

        /*  Print message if errno != 0; otherwise assume an error message has
            already been printed.
        */
        if((ret != 0) && (errno != 0))
        {
            fprintf(stderr, "Error copying from input directory: ");
            perror("");
        }
    }

    return ret;
}


/** @brief Worker function for ntfw(); copies the given file/creates the
           given directory.

    @param pszPath  The file to copy.
    @param pStat    Unused.
    @param flag     File type flag.
    @param pFtw     Unused.
*/
static int FtwCopyFile(
    const char         *pszPath,
    const struct stat  *pStat,
    int                 flag,
    struct FTW         *pFtw)
{
    int                 ret = 0;
    FILEMAPPING         mapping;

    (void) pStat;
    (void) pFtw;

    errno = 0;  /* Protect from extra error messages. */

    switch (flag) {
        case FTW_D:
            /*  Don't try to create the root dir; it always exists.
            */
            if(strcmp(pszPath, gBaseDir) != 0)
            {
                ret = IbPosixCreateDir(gVolName, pszPath, gBaseDir);
            }
            break;

        case FTW_F:
            if (strlen(pszPath) >= HOST_PATH_MAX)
            {
                fprintf(stderr, "Error: file path too long: %s\n", pszPath);
                ret = -1;
            }
            else
            {
                strcpy(mapping.asInFilePath, pszPath);
                ret = IbConvertPath(gVolName, pszPath, gBaseDir, mapping.asOutFilePath);

                if(ret == 0)
                {
                    ret = IbCopyFile(-1, &mapping);
                }
            }
            break;

        default:
            /*  Don't copy special files.
            */
            break;
    }

    return ret;
}
#endif


#if REDCONF_API_FSE == 1
/** @brief Reads the contents of the input directory, assignes a file index
           to each file name, and fills a linked list structure with the
           names and indexes. Does not inspect subdirectories. Prints any
           error messages to stderr.

    @param pszDirPath       The path to the input directory.
    @param ppFileListHead   A pointer to a FILELISTENTRY pointer to be filled.
                            A linked list is allocated onto this pointer if
                            successful, and thus should be freed after use by
                            passing it to FreeFileList.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbFseBuildFileList(
    const char     *pszDirPath,
    FILELISTENTRY **ppFileListHead)
{
    int             ret = 0;
    int             currFileIndex = 2; /* Indexes 0 and 1 are reserved */
    FILELISTENTRY  *pCurrEntry = NULL;
    DIR            *pDir;

    *ppFileListHead = NULL;
    REDASSERT(pszDirPath != NULL);

    if(ret == 0)
    {
        pDir = opendir(pszDirPath);

        if(pDir == NULL)
        {
            perror("Error opening input directory");
            ret = -1;
        }
    }

    /*  Find each file in the directory and populate ppFileListHead
    */
    while(ret == 0)
    {
        struct dirent  *pDirent;
        struct stat     sstat;

        errno = 0;
        pDirent = readdir(pDir);

        if(pDirent == NULL)
        {
            if(errno != 0)
            {
                REDASSERT(errno == EBADF);
                perror("Error reading from input directory");
            }

            break;
        }

        ret = stat(pDirent->d_name, &sstat);
        if(ret != 0)
        {
            perror("Error getting file information");
        }

        /*  Skip over "irregular" files.
        */
        if((ret == 0) && S_ISREG(sstat.st_mode))
        {
            int             len;
            FILELISTENTRY  *pNewEntry = malloc(sizeof(*pNewEntry));

            if(pNewEntry == NULL)
            {
                fprintf(stderr, "Error allocating memory.\n");
                ret = -1;
            }
            else
            {
                const char     *pszToAppend;

                /*  A path separator must be added if the directory path does
                    not already end with one.
                */
                if(pszDirPath[strlen(pszDirPath) - 1] == '/')
                {
                    pszToAppend = "";
                }
                else
                {
                    pszToAppend = "/";
                }

                len = snprintf(pNewEntry->fileMapping.asInFilePath, HOST_PATH_MAX, "%s%s%s",
                    pszDirPath, pszToAppend, pDirent->d_name);

                if((len < 0) || (len >= HOST_PATH_MAX))
                {
                    fprintf(stderr, "Error: file path too long: %s%s%s", pszDirPath, pszToAppend, pDirent->d_name);
                    ret = -1;
                }
                else
                {
                    pNewEntry->fileMapping.ulOutFileIndex = currFileIndex;
                    pNewEntry->pNext = NULL;

                    /*  If pCurrEntry is NULL, then pNewEntry will be the root entry.
                        Otherwise add it to the end of the linked list.
                    */
                    if(pCurrEntry == NULL)
                    {
                        *ppFileListHead = pNewEntry;
                    }
                    else
                    {
                        pCurrEntry->pNext = pNewEntry;
                    }
                    pCurrEntry = pNewEntry;

                    currFileIndex++;
                }
            }
        }
    }

    if(pDir != NULL)
    {
        (void) closedir(pDir);
    }

    if(ret != 0)
    {
        FreeFileList(ppFileListHead);
    }

    return ret;
}
#endif


#if REDCONF_API_FSE == 1
/** @brief  Set the given path to be relative to its parent path if it is
            is not an absolute path.
*/
int IbSetRelativePath(
    char       *pszPath,
    const char *pszParentPath)
{
    int         ret = 0;

    REDASSERT((pszPath != NULL) && (pszParentPath != NULL));

    if(pszPath[0] != '/')
    {
        if(pszParentPath == NULL)
        {
            fprintf(stderr, "Error: paths in mapping file must be absolute if no input directory is specified.\n");
            ret = -1;
        }
        else if(strlen(pszPath) >= HOST_PATH_MAX)
        {
            /*  Not expected; the length of pszPath should have already been checked.
            */
            fprintf(stderr, "Error: path too long: %s\n", pszPath);
            REDERROR();
            ret = -1;
        }
        else
        {
            char    asTemp[HOST_PATH_MAX];
            int     len;
            char   *pszToAppend;
            size_t  indirLen = strlen(pszParentPath);

            REDASSERT(indirLen != 0);

            strcpy(asTemp, pszPath);

            /*  Ensure a path separator comes between the input directory
                and the specified relative path.
            */
            if(pszParentPath[indirLen - 1] == '/')
            {
                pszToAppend = "";
            }
            else
            {
                pszToAppend = "/";
            }

            len = snprintf(pszPath, HOST_PATH_MAX, "%s%s%s", pszParentPath, pszToAppend, asTemp);

            if((len < 0) || (len >= HOST_PATH_MAX))
            {
                fprintf(stderr, "Error: file path too long: %s%s%s", pszParentPath, pszToAppend, asTemp);
                ret = -1;
            }
        }
    }

    return ret;
}
#endif


/*  Checks whether the given path appears NOT to name a volume.
*/
bool IsRegularFile(
    const char *pszPath)
{
    bool        fRet;
    struct stat sstat;

    if(stat(pszPath, &sstat) == 0)
    {
        fRet = S_ISREG(sstat.st_mode);
    }
    else
    {
        fRet = false;
    }

    return fRet;
}


#endif
