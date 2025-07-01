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
/** @file
    @brief Implements methods of the image builder tool that require Linux
           OS-specific function calls.
*/
#define _GNU_SOURCE /* for FTW_ACTIONRETVAL */
#include <redfs.h>

#if REDCONF_IMAGE_BUILDER == 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ftw.h>        /* Makefile must set _XOPEN_SOURCE >= 500 */

#include <redtools.h>
#include <redposix.h> /* for red_symlink() */
#include <redfse.h> /* for RED_FILENUM_FIRST_VALID */


#if REDCONF_API_POSIX == 1

static int FtwCopyFile(const char *pszPath, const struct stat *pStat, int flag, struct FTW *pFtw);
#if HAVE_SETTABLE_ATTR
static int FtwCopyDirAttr(const char *pszPath, const struct stat *pStat, int flag, struct FTW *pFtw);
#endif

static const char *gpszVolName;
static const char *gpszBaseDir;


/** @brief Recursively copy a host directory to a Reliance Edge volume.

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
    int         ret = 0;
    struct stat pathStat;

    /*  Check that the pszInDir is a directory
    */
    if(stat(pszInDir, &pathStat))
    {
        fprintf(stderr, "failed to stat %s: ", pszInDir);
        perror("");
        ret = -1;
    }

    if(ret == 0)
    {
        if(!S_ISDIR(pathStat.st_mode))
        {
            fprintf(stderr, "%s is not a directory\n", pszInDir);
            ret = -1;
        }
    }

    if(ret == 0)
    {
        gpszVolName = pszVolName;
        gpszBaseDir = pszInDir;

        /*  FTW_PHYS causes nftw() to include symbolic links.
        */
        ret = nftw(pszInDir, FtwCopyFile, 20, FTW_PHYS);

      #if HAVE_SETTABLE_ATTR
        /*  The attributes for directories have to be copied in a second pass.
            nftw() returns directories prior to their entries, so if we tried to
            update the directory attributes first and then created entries
            within that directory, we have an issue: the directory mtime would
            get stomped on as part of creat() or mkdir().  Furthermore, if the
            POSIX-like API is configured to enforce permissions, it would be a
            problem if we copied restrictive permissions (no write or execute)
            before creating the entries.
        */
        if(ret == 0)
        {
            /*  FTW_ACTIONRETVAL is needed so we can use FTW_SKIP_SUBTREE; see
                comment in FtwCopyDirAttr() for details.  FTW_PHYS is also
                required for the FTW_SL case in that function.
            */
            ret = nftw(pszInDir, FtwCopyDirAttr, 20, FTW_PHYS | FTW_ACTIONRETVAL);
        }
      #endif

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


/** @brief Worker function for ntfw().

    Copies each given file and creates each given directory.

    @param pszPath  The file to copy.
    @param pStat    Unused.
    @param flag     File type flag.
    @param pFtw     Unused.

    @return Zero to tell ntfw() to continue, nonzero to tell it to stop.
*/
static int FtwCopyFile(
    const char         *pszPath,
    const struct stat  *pStat,
    int                 flag,
    struct FTW         *pFtw)
{
    int                 ret = 0;
    FILEMAPPING         mapping;

    (void)pStat;
    (void)pFtw;

    errno = 0;  /* Protect from extra error messages. */

    switch(flag)
    {
        case FTW_D:
            /*  Don't try to create the root dir; it always exists.
            */
            if(strcmp(pszPath, gpszBaseDir) != 0)
            {
                ret = IbPosixCreateDir(gpszVolName, pszPath, gpszBaseDir);
            }
            break;

        case FTW_F:
            if(strlen(pszPath) >= HOST_PATH_MAX)
            {
                fprintf(stderr, "Error: file path too long: %s\n", pszPath);
                ret = -1;
            }
            else
            {
                strcpy(mapping.szInFilePath, pszPath);
                ret = IbConvertPath(gpszVolName, pszPath, gpszBaseDir, mapping.szOutFilePath);

                if(ret == 0)
                {
                    ret = IbCopyFile(0U /* Unused */, &mapping);
                }
            }
            break;

      #if REDCONF_API_POSIX_SYMLINK == 1
        case FTW_SL:
            if(strlen(pszPath) >= HOST_PATH_MAX)
            {
                fprintf(stderr, "Error: file path too long: %s\n", pszPath);
                ret = -1;
            }
            else
            {
                strcpy(mapping.szInFilePath, pszPath);
                ret = IbConvertPath(gpszVolName, pszPath, gpszBaseDir, mapping.szOutFilePath);

                if(ret == 0)
                {
                    char szBuffer[HOST_PATH_MAX] = {'\0'};

                    printf("Copying symlink %s to %s\n", pszPath, mapping.szOutFilePath);

                    ret = readlink(pszPath, szBuffer, sizeof(szBuffer));

                    if(ret != -1)
                    {
                        if(szBuffer[sizeof(szBuffer) - 1U] != '\0')
                        {
                            fprintf(stderr, "Error: symlink target in \"%s\" is too long\n", pszPath);
                            ret = -1;
                        }
                        else
                        {
                          #if REDCONF_PATH_SEPARATOR != '/'
                            uint32_t ulIdx = 0U;

                            while(szBuffer[ulIdx] != '\0')
                            {
                                if(szBuffer[ulIdx] == '/')
                                {
                                    szBuffer[ulIdx] = REDCONF_PATH_SEPARATOR;
                                }

                                ulIdx++;
                            }
                          #endif

                            ret = red_symlink(szBuffer, mapping.szOutFilePath);
                            if(ret == -1)
                            {
                                fprintf(stderr, "Error: red_symlink() failed with error %d\n", (int)red_errno);
                            }
                        }
                    }
                }
            }

            break;
      #endif

        default:
            /*  Don't copy special files.
            */
            break;
    }

    return ret;
}


#if HAVE_SETTABLE_ATTR
/** @brief Worker function for ntfw().

    Copies the attributes for directory files.

    @param pszPath  The file to copy.
    @param pStat    Unused.
    @param flag     File type flag.
    @param pFtw     Unused.

    @return Zero to tell ntfw() to continue, nonzero to tell it to stop.
*/
static int FtwCopyDirAttr(
    const char         *pszPath,
    const struct stat  *pStat,
    int                 flag,
    struct FTW         *pFtw)
{
    int                 ret = 0;

    (void)pStat;
    (void)pFtw;

    /*  Only interested in directories.  Unlike FtwCopyFile(), we're also
        interested in the root directory, so we don't filter it out.
    */
    if(flag == FTW_D)
    {
        char szRedPath[HOST_PATH_MAX];

        ret = IbConvertPath(gpszVolName, pszPath, gpszBaseDir, szRedPath);
        if(ret == 0)
        {
            ret = IbCopyAttr(pszPath, szRedPath);
        }

        if(ret == 0)
        {
            ret = FTW_CONTINUE;
        }
        else
        {
            ret = FTW_STOP;
        }
    }
    else if(flag == FTW_SL)
    {
        /*  Prevent ntfw() from traversing symbolic links which point at
            directories.  Such traversal isn't necessary, because we will reach
            each directory which exists on the volume via non-symlink paths, so
            reaching them again via symlink paths is either redundant or, for
            targets outside the volume or which don't exist, unwanted.
            Additionally, following symlink directories will fail in
            IbCopyAttr(), because it will construct Reliance Edge paths which
            have symlinks as part of the path prefix component, leading to
            RED_ENOLINK errors.
        */
        ret = FTW_SKIP_SUBTREE;
    }

    return ret;
}
#endif

#endif /* REDCONF_API_POSIX == 1 */


#if REDCONF_API_FSE == 1

/** @brief Build a list of files in a given directory.

    Reads the contents of the input directory, assigns a file index to each file
    name, and fills a linked list structure with the names and indexes.  Does
    not inspect subdirectories.  Prints any error messages to stderr.

    @param pszDirPath       The path to the input directory.
    @param ppFileListHead   A pointer to a FILELISTENTRY pointer to be filled.
                            A linked list is allocated onto this pointer if
                            successful, and thus should be freed after use by
                            passing it to FreeFileList().

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbFseBuildFileList(
    const char     *pszDirPath,
    FILELISTENTRY **ppFileListHead)
{
    int             ret = 0;
    uint32_t        ulCurrFileIndex = RED_FILENUM_FIRST_VALID;
    FILELISTENTRY  *pCurrEntry = NULL;
    DIR            *pDir;
    const char     *pszToAppend;

    *ppFileListHead = NULL;

    REDASSERT(pszDirPath != NULL);

    /*  A path separator must be added if the directory path does not already
        end with one.
    */
    if(IB_ISPATHSEP(pszDirPath[strlen(pszDirPath) - 1U]))
    {
        pszToAppend = "";
    }
    else
    {
        pszToAppend = "/";
    }

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
        char            szDirentPath[HOST_PATH_MAX];
        struct stat     sstat;
        int             len;

        errno = 0;
        pDirent = readdir(pDir);

        if(pDirent == NULL)
        {
            if(errno != 0)
            {
                perror("Error reading from input directory");
                ret = -1;
            }

            break;
        }

        len = snprintf(szDirentPath, sizeof(szDirentPath), "%s%s%s", pszDirPath, pszToAppend, pDirent->d_name);
        if((len < 0) || (len >= HOST_PATH_MAX))
        {
            fprintf(stderr, "Error: file path too long: %s%s%s\n", pszDirPath, pszToAppend, pDirent->d_name);
            ret = -1;
        }

        if(ret == 0)
        {
            ret = stat(szDirentPath, &sstat);
            if(ret != 0)
            {
                perror("Error getting file information");
                ret = -1;
            }
        }

        /*  Skip over "irregular" files.
        */
        if((ret == 0) && S_ISREG(sstat.st_mode))
        {
            FILELISTENTRY *pNewEntry = malloc(sizeof(*pNewEntry));

            if(pNewEntry == NULL)
            {
                fprintf(stderr, "Error allocating memory.\n");
                ret = -1;
            }
            else
            {
                strcpy(pNewEntry->fileMapping.szInFilePath, szDirentPath);

                pNewEntry->fileMapping.ulOutFileIndex = ulCurrFileIndex;
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

                ulCurrFileIndex++;
            }
        }
    }

    if(pDir != NULL)
    {
        (void)closedir(pDir);
    }

    if(ret != 0)
    {
        FreeFileList(ppFileListHead);
    }

    return ret;
}


/** @brief Set the given path to be relative to its parent path if it is
           is not an absolute path.
*/
int IbSetRelativePath(
    char       *pszPath,
    const char *pszParentPath)
{
    int         ret = 0;

    REDASSERT(pszPath != NULL);

    if(pszPath[0U] != '/')
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
            char    szTemp[HOST_PATH_MAX];
            int     len;
            char   *pszToAppend;
            size_t  nInDirLen = strlen(pszParentPath);

            REDASSERT(nInDirLen != 0U);

            strcpy(szTemp, pszPath);

            /*  Ensure a path separator comes between the input directory
                and the specified relative path.
            */
            if(IB_ISPATHSEP(pszParentPath[nInDirLen - 1U]))
            {
                pszToAppend = "";
            }
            else
            {
                pszToAppend = "/";
            }

            len = snprintf(pszPath, HOST_PATH_MAX, "%s%s%s", pszParentPath, pszToAppend, szTemp);

            if((len < 0) || (len >= HOST_PATH_MAX))
            {
                fprintf(stderr, "Error: file path too long: %s%s%s\n", pszParentPath, pszToAppend, szTemp);
                ret = -1;
            }
        }
    }

    return ret;
}

#endif /* REDCONF_API_FSE == 1 */


/** @brief Checks whether the given path appears NOT to name a volume.
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


/** @brief Retrieve information about a file or directory.

    @param pszPath  The host file system path.
    @param pStat    Stat structure to populate.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbStat(
    const char *pszPath,
    IBSTAT     *pStat)
{
    struct stat sb;
    int         ret;

    if(lstat(pszPath, &sb) < 0)
    {
        perror("stat");
        ret = -1;
    }
    else
    {
        memset(pStat, 0, sizeof(*pStat));
        pStat->uMode = (uint16_t)sb.st_mode;
        pStat->ulUid = (uint32_t)sb.st_uid;
        pStat->ulGid = (uint32_t)sb.st_gid;
        pStat->ullSize = (uint64_t)sb.st_size;
      #ifdef POSIX_2008_STAT
        pStat->ulATime = (uint32_t)sb.st_atim.tv_sec;
        pStat->ulMTime = (uint32_t)sb.st_mtim.tv_sec;
      #else
        pStat->ulATime = (uint32_t)sb.st_atime;
        pStat->ulMTime = (uint32_t)sb.st_mtime;
      #endif
        ret = 0;
    }

    return ret;
}

#endif /* REDCONF_IMAGE_BUILDER == 1 */
