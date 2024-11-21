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
    with this program; if not, see <https://www.gnu.org/licenses/>.
*/
/*  Businesses and individuals that for commercial or other reasons cannot
    comply with the terms of the GPLv2 license must obtain a commercial
    license before incorporating Reliance Edge into proprietary software
    for distribution in any form.

    Visit https://www.tuxera.com/products/reliance-edge/ for more information.
*/
/** @file
    @brief Implements methods of the image builder tool that require Windows
           OS-specific function calls.
*/
#include <redfs.h>

#if REDCONF_IMAGE_BUILDER == 1

#include <stdio.h>
#include <windows.h>

#include <redtools.h>
#include <redfse.h> /* for RED_FILENUM_FIRST_VALID */
#include <redstat.h>


static bool FileLooksExecutable(const char *pszPath);
static uint32_t WinFileTimeToUnixTime(FILETIME winTime);


#if REDCONF_API_POSIX == 1
/** @brief Recursively copy a host directory to a Reliance Edge volume.

    @param pszVolName   The name of the target Reliance Edge volume.
    @param pszInDir     The path to the directory to copy.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbPosixCopyDirRecursive(
    const char         *pszVolName,
    const char         *pszInDir)
{
    /*  Used to record pszVolName the first time called in a recursion series.
    */
    static bool         fIsRecursing = false;
    static const char  *pszBaseDir;
    bool                fRememberIsRecursing = fIsRecursing;
    int                 ret = 0;
    char                szCurrPath[HOST_PATH_MAX];
    HANDLE              h;
    WIN32_FIND_DATA     findData;
    int                 len;

    if(!fIsRecursing)
    {
        pszBaseDir = pszInDir;
        fIsRecursing = true;
    }

    len = _snprintf(szCurrPath, HOST_PATH_MAX, "%s\\*", pszInDir);
    if((len < 0) || (len >= HOST_PATH_MAX))
    {
        ret = -1;
    }

    if(ret == 0)
    {
        h = FindFirstFile(szCurrPath, &findData);
        if(h == INVALID_HANDLE_VALUE)
        {
            fprintf(stderr, "Error reading from input directory.\n");
            ret = -1;
        }
    }

    while(ret == 0)
    {
        BOOL fFindSuccess;

        if((strcmp(findData.cFileName, ".") != 0) && (strcmp(findData.cFileName, "..") != 0))
        {
            len = _snprintf(szCurrPath, HOST_PATH_MAX, "%s\\%s", pszInDir, findData.cFileName);

            if((len == HOST_PATH_MAX) || (len < 0))
            {
                fprintf(stderr, "Error: file path too long: %s\\%s\n", pszInDir, findData.cFileName);
            }
            else if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                /*  Create the directory, then recurse!
                */
                ret = IbPosixCreateDir(pszVolName, szCurrPath, pszBaseDir);
                if(ret == 0)
                {
                    ret = IbPosixCopyDirRecursive(pszVolName, szCurrPath);
                }
            }
            else
            {
                FILEMAPPING mapping;

                strcpy(mapping.szInFilePath, szCurrPath);
                ret = IbConvertPath(pszVolName, szCurrPath, pszBaseDir, mapping.szOutFilePath);
                if(ret == 0)
                {
                    ret = IbCopyFile(0U /* Unused */, &mapping);
                }
            }
        }

        fFindSuccess = FindNextFile(h, &findData);
        if(!fFindSuccess)
        {
            DWORD err = GetLastError();

            if(err == ERROR_NO_MORE_FILES)
            {
                break;
            }
            else
            {
                fprintf(stderr, "Error traversing input directory %s.  Error code: %d\n", pszInDir, (int)err);
                ret = -1;
            }
        }
    }

    if(h != INVALID_HANDLE_VALUE)
    {
        (void)FindClose(h);
    }

  #if HAVE_SETTABLE_ATTR
    /*  Update directory attributes.  As this function is invoked recursively,
        this will be done for every copied directory, including the root
        directory.
    */
    if(ret == 0)
    {
        char szRedPath[HOST_PATH_MAX];

        ret = IbConvertPath(pszVolName, pszInDir, pszBaseDir, szRedPath);

        if(ret == 0)
        {
            ret = IbCopyAttr(pszInDir, szRedPath);
        }
    }
  #endif

    fIsRecursing = fRememberIsRecursing;

    return ret;
}
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
    char            szSPath[HOST_PATH_MAX];
    uint32_t        ulCurrFileIndex = RED_FILENUM_FIRST_VALID;
    FILELISTENTRY  *pCurrEntry = NULL;
    HANDLE          searchHandle = INVALID_HANDLE_VALUE;
    size_t          nPathLen = strlen(pszDirPath);
    WIN32_FIND_DATA findData;
    const char     *pszToAppend;

    *ppFileListHead = NULL;

    REDASSERT(pszDirPath != NULL);

    /*  Assign host path separator to pszToAppend if pszDirPath does not already
        end with one.
    */
    if(IB_ISPATHSEP(pszDirPath[nPathLen - 1U]))
    {
        pszToAppend = "";
    }
    else
    {
        pszToAppend = "\\";
    }

    if((nPathLen + strlen(pszToAppend)) >= HOST_PATH_MAX)
    {
        fprintf(stderr, "Input directory path exceeds maximum supported length.\n");
        ret = -1;
    }

    if(ret == 0)
    {
        int stat;

        stat = sprintf(szSPath, "%s%s*", pszDirPath, pszToAppend);

        /*  Strings are already tested; sprintf shouldn't fail.
        */
        REDASSERT(stat >= 0);
        (void)stat;
    }

    if(ret == 0)
    {
        searchHandle = FindFirstFile(szSPath, &findData);

        if(searchHandle == INVALID_HANDLE_VALUE)
        {
            if(GetLastError() == ERROR_FILE_NOT_FOUND)
            {
                fprintf(stderr, "Specified input directory empty or not found.\n");
            }
            else
            {
                fprintf(stderr, "Could not read input directory contents or empty input directory.\n");
            }

            ret = -1;
        }
    }

    /*  Find each file in the directory and populate ppFileListHead
    */
    while(ret == 0)
    {
        /*  Skip over directories.  Create a new entry for each file and add it
            to ppFileListHead
        */
        if(!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            FILELISTENTRY *pNewEntry = malloc(sizeof(*pNewEntry));

            if(pNewEntry == NULL)
            {
                fprintf(stderr, "Error allocating memory.\n");
                ret = -1;
            }
            else
            {
                int len;

                len = _snprintf(pNewEntry->fileMapping.szInFilePath, HOST_PATH_MAX, "%s%s%s",
                    pszDirPath, pszToAppend, findData.cFileName);

                if((len < 0) || (len >= HOST_PATH_MAX))
                {
                    fprintf(stderr, "Error: file path too long: %s%s%s\n", pszDirPath, pszToAppend, findData.cFileName);
                    ret = -1;
                }
                else
                {
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

        if(ret == 0)
        {
            if(!FindNextFile(searchHandle, &findData))
            {
                if(GetLastError() != ERROR_NO_MORE_FILES)
                {
                    fprintf(stderr, "Error traversing input directory.\n");
                    ret = -1;
                }
                else
                {
                    break;
                }
            }
        }
    }

    if(searchHandle != INVALID_HANDLE_VALUE)
    {
        (void)FindClose(searchHandle);
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

    if(    (    ((pszPath[0U] >= 'A') && (pszPath[0U] <= 'Z'))
             || ((pszPath[0U] >= 'a') && (pszPath[0U] <= 'z')))
        && (pszPath[1U] == ':')
        && ((pszPath[2U] == '\\') || (pszPath[2U] == '/')))
    {
        /*  The path appears to be absolute; no need to modify it.
        */
        ret = 0;
    }
    else if(pszParentPath == NULL)
    {
        fprintf(stderr, "Error: paths in mapping file must be absolute if no input directory is specified.\n");
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

        /*  Ensure a path separator comes between the input directory and the
            specified relative path.
        */
        if(IB_ISPATHSEP(pszParentPath[nInDirLen - 1U]))
        {
            pszToAppend = "";
        }
        else
        {
            pszToAppend = "\\";
        }

        len = _snprintf(pszPath, HOST_PATH_MAX, "%s%s%s", pszParentPath, pszToAppend, szTemp);

        if((len < 0) || (len >= HOST_PATH_MAX))
        {
            fprintf(stderr, "Error: file path too long: %s%s%s\n", pszParentPath, pszToAppend, szTemp);
            ret = -1;
        }
    }

    return ret;
}

#endif /* REDCONF_API_FSE == 1 */


/** @brief Determine whether a path names a regular file (_not_ a volume).

    @param pszPath  The path to examine.

    @return Whether the given path appears to name a regular file.
*/
bool IsRegularFile(
    const char *pszPath)
{
    bool        fRet = true;

    if(    (strncmp(pszPath, "\\\\.\\", 4U) == 0)
        && (strchr(&pszPath[4U], '\\') == NULL)
        && (strchr(&pszPath[4U], '/') == NULL))
    {
        /*  DOS device paths (which start with "\\.\") are assumed to name
            a device like "\\.\PhysicalDrive4" if there are no further path
            separator characters after the initial "\\.\".
        */
        fRet = false;
    }
    else if(    (    ((pszPath[0U] >= 'A') && (pszPath[0U] <= 'Z'))
                  || ((pszPath[0U] >= 'a') && (pszPath[0U] <= 'z')))
             && (pszPath[1U] == ':')
             && (    (pszPath[2U] == '\0')
                  || ((pszPath[2U] == '\\') && (pszPath[3U] == '\0'))))
    {
        /*  A device letter is not a regular file.
        */
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
    const char         *pszPath,
    IBSTAT             *pStat)
{
    HANDLE              hFindFile;
    WIN32_FIND_DATAA    findData;
    int                 ret = 0;

    hFindFile = FindFirstFileA(pszPath, &findData);
    if(hFindFile == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "FindFirstFileA(\"%s\") failed with error %lu\n", pszPath, (unsigned long)GetLastError());
        ret = -1;
    }
    else
    {
        (void)FindClose(hFindFile);

        memset(pStat, 0, sizeof(*pStat));

        if((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U)
        {
            pStat->uMode = RED_S_IFDIR;

            /*  0755: rwx for owner, rx for group and other
            */
            pStat->uMode |= RED_S_IRWXU|RED_S_IRGRP|RED_S_IXGRP|RED_S_IROTH|RED_S_IXOTH;
        }
        else
        {
            pStat->uMode = RED_S_IFREG;

            /*  0644: rw for owner, r for group and other
            */
            pStat->uMode |= RED_S_IRUSR|RED_S_IWUSR|RED_S_IRGRP|RED_S_IROTH;

            if(FileLooksExecutable(pszPath))
            {
                /*  0755: add execute permissions for everyone.
                */
                pStat->uMode |= RED_S_IXUSR|RED_S_IXGRP|RED_S_IXOTH;
            }
        }

        /*  Clear write permissions if read-only.
        */
        if((findData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0U)
        {
            pStat->uMode &= ~(RED_S_IWUSR|RED_S_IWGRP|RED_S_IWOTH);
        }

      #if (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1)
        /*  Windows doesn't have uid/gid.  Use whatever the OS service was
            implemented to return.
        */
        pStat->ulUid = RedOsUserId();
        pStat->ulGid = RedOsGroupId();
      #endif

        pStat->ullSize = (findData.nFileSizeHigh * ((uint64_t)MAXDWORD + 1U)) + findData.nFileSizeLow;

        pStat->ulATime = WinFileTimeToUnixTime(findData.ftLastAccessTime);
        pStat->ulMTime = WinFileTimeToUnixTime(findData.ftLastWriteTime);
    }

    return ret;
}


/** @brief Probe a file to see if it looks like a Unix executable.

    @param pszPath  Path to the file on the host file system.

    @return Whether the file looks like a Unix executable.
*/
static bool FileLooksExecutable(
    const char *pszPath)
{
    FILE       *pFile;
    bool        fExecutable = false;

    pFile = fopen(pszPath, "rb");
    if(pFile == NULL)
    {
        perror("fopen");
        fprintf(stderr, "warning: unable to open file \"%s\" to probe for executable\n", pszPath);
    }
    else
    {
        char    acMagic[4U];
        size_t  nLen = fread(acMagic, 1U, sizeof(acMagic), pFile);

        if((nLen < sizeof(acMagic)) && ferror(pFile))
        {
            fprintf(stderr, "warning: unable to read file \"%s\" to probe for executable\n", pszPath);
        }
        else if((nLen >= 2U) && (acMagic[0U] == '#') && (acMagic[1U] == '!'))
        {
            /*  Found shebang, assume it's a script.
            */
            fExecutable = true;
        }
        else if((nLen >= 4U) && (acMagic[0U] == 0x7F) && (strncmp(&acMagic[1U], "ELF", 3U) == 0U))
        {
            /*  Found magic number of an ELF executable.
            */
            fExecutable = true;
        }
        else
        {
            /*  Other files are not executable.

                Windows/DOS executables (.exe, .bat, .ps1, .com, etc.) are _not_
                executable in any operating system where execute permissions
                actually matter, and so they are treated as normal files.
            */
        }

        if(fclose(pFile) == EOF)
        {
            perror("fclose");
        }
    }

    return fExecutable;
}


/** @brief Convert Windows file time to Unix time.

    @param winTime  The Windows file time to convert.

    @return The Unix time equivalent to @p winTime.
*/
static uint32_t WinFileTimeToUnixTime(
    FILETIME        winTime)
{
    ULARGE_INTEGER  unixTime;

    /*  Place the file time in a union so that we can access it as one 64-bit
        value.
    */
    unixTime.LowPart = winTime.dwLowDateTime;
    unixTime.HighPart = winTime.dwHighDateTime;

    /*  Account for the difference in base-points for the timestamps.
        Windows file time starts at January 1, 1601.
        Unix time starts at January 1, 1970.
    */
    unixTime.QuadPart -= TIME_1601_TO_1970_DAYS * TIME_100NANOS_PER_DAY;

    /*  For dates prior to the Unix epoch, use the Unix epoch.
    */
    if(unixTime.QuadPart < 0)
    {
        unixTime.QuadPart = 0;
    }

    /*  Translate from "100-nanosecond intervals" to one-second intervals.
    */
    unixTime.QuadPart /= 10000000;

    /*  For dates which cannot be represented by uint32_t (> circa 2106 A.D.),
        use the maximum value.
    */
    if(unixTime.QuadPart > UINT32_MAX)
    {
        unixTime.QuadPart = UINT32_MAX;
    }

    return (uint32_t)unixTime.QuadPart;
}

#endif /* REDCONF_IMAGE_BUILDER == 1 */
