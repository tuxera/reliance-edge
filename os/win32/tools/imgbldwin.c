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
#include <windows.h>

#include <redtools.h>


#if REDCONF_API_POSIX == 1
/*  @brief  Recurses through a Windows directory and copies its contents to a
            Reliance Edge volume.

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
    static bool         isRecursing = false;
    static const char  *pszBaseDir;
    bool                rememberIsRecursing = isRecursing;
    int                 ret = 0;
    char                asCurrPath[HOST_PATH_MAX];
    HANDLE              h;
    WIN32_FIND_DATA     sFindData;
    int                 len;

    if(!isRecursing)
    {
        pszBaseDir = pszInDir;
        isRecursing = true;
    }

    len = _snprintf(asCurrPath, HOST_PATH_MAX, "%s\\*", pszInDir);
    if((len < 0) || (len >= HOST_PATH_MAX))
    {
        ret = -1;
    }

    if(ret == 0)
    {
        h = FindFirstFile(asCurrPath, &sFindData);
        if(h == INVALID_HANDLE_VALUE)
        {
            fprintf(stderr, "Error reading from input directory.\n");
            ret = -1;
        }
    }

    while(ret == 0)
    {
        BOOL fFindSuccess;

        if((strcmp(sFindData.cFileName, ".") != 0) && (strcmp(sFindData.cFileName, "..") != 0))
        {
            len = _snprintf(asCurrPath, HOST_PATH_MAX, "%s\\%s", pszInDir, sFindData.cFileName);

            if((len == HOST_PATH_MAX) || (len < 0))
            {
                fprintf(stderr, "Error: file path too long: %s\\%s\n", pszInDir, sFindData.cFileName);
            }
            else if(sFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                /*  Create the direcctory, then recurse!
                */
                ret = IbPosixCreateDir(pszVolName, asCurrPath, pszBaseDir);
                if(ret == 0)
                {
                    ret = IbPosixCopyDirRecursive(pszVolName, asCurrPath);
                }
            }
            else
            {
                FILEMAPPING mapping;

                strcpy(mapping.asInFilePath, asCurrPath);
                ret = IbConvertPath(pszVolName, asCurrPath, pszBaseDir, mapping.asOutFilePath);
                if(ret == 0)
                {
                    ret = IbCopyFile(-1, &mapping);
                }
            }
        }

        fFindSuccess = FindNextFile(h, &sFindData);
        if(!fFindSuccess)
        {
            DWORD err = GetLastError();

            if(err == ERROR_NO_MORE_FILES)
            {
                break;
            }
            else
            {
                fprintf(stderr, "Error traversing input directory %s. Error code: %d\n", pszInDir, err);
                ret = -1;
            }
        }
    }

    if(h != INVALID_HANDLE_VALUE)
    {
        (void) FindClose(h);
    }

    isRecursing = rememberIsRecursing;

    return ret;
}
#endif


#if REDCONF_API_FSE == 1
/** @brief Reads the contents of the input directory, assignes a file index
           to each file name, and fills a linked list structure with the
           names and indexes. Does not inspect subdirectories. Prints any
           error messages to stderr.

    @param pszDirPath The path to the input directory.
    @param ppFileListHead a pointer to a FILELISTENTRY pointer to be
           filled. A linked list is allocated onto this pointer if
           successful, and thus should be freed after use by passing
           it to FreeFileList.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbFseBuildFileList(
    const char     *pszDirPath,
    FILELISTENTRY **ppFileListHead)
{
    int             ret = 0;
    char            asSPath[HOST_PATH_MAX];
    int             currFileIndex = 2; /* Indexes 0 and 1 are reserved */
    FILELISTENTRY  *pCurrEntry = NULL;
    HANDLE          searchHandle = INVALID_HANDLE_VALUE;
    size_t          pathLen = strlen(pszDirPath);
    WIN32_FIND_DATA sFindData;
    const char     *pszToAppend;

    *ppFileListHead = NULL;
    REDASSERT(pszDirPath != NULL);

    /*  Assign host path separator to pszToAppend if pszDirPath does not already
        end with one.
    */
    if((pszDirPath[pathLen - 1] == '/') || (pszDirPath[pathLen - 1] == '\\'))
    {
        pszToAppend = "";
    }
    else
    {
        pszToAppend = "\\";
    }

    if(pathLen + strlen(pszToAppend) >= HOST_PATH_MAX)
    {
        fprintf(stderr, "Input directory path exceeds maximum supported length.\n");
        ret = -1;
    }

    if(ret == 0)
    {
        int stat;

        stat = sprintf(asSPath, "%s%s*", pszDirPath, pszToAppend);

        /*  Strings are aready tested; sprintf shouldn't fail.
        */
        REDASSERT(stat >= 0);
        (void) stat;
    }

    if(ret == 0)
    {
        searchHandle = FindFirstFile(asSPath, &sFindData);

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
        /*  Skip over directories. Create a new entry for each file and add it
            to ppFileListHead
        */
        if(!(sFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
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
                len = _snprintf(pNewEntry->fileMapping.asInFilePath, HOST_PATH_MAX, "%s%s%s",
                    pszDirPath, pszToAppend, sFindData.cFileName);

                if((len < 0) || (len >= HOST_PATH_MAX))
                {
                    fprintf(stderr, "Error: file path too long: %s%s%s", pszDirPath, pszToAppend, sFindData.cFileName);
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

        if(ret == 0)
        {
            if(!FindNextFile(searchHandle, &sFindData))
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
        (void) FindClose(searchHandle);
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
    int         ret;

    REDASSERT((pszPath != NULL) && (pszParentPath != NULL));

    if(     (    ((pszPath[0U] >= 'A') && (pszPath[0U] <= 'Z'))
              || ((pszPath[0U] >= 'a') && (pszPath[0U] <= 'z')))
         && (pszPath[1U] == ':')
         && ((pszPath[2U] == '\\') || (pszPath[2U] == '/')))
    {
        /*  The path appears to be absolute; no need to modify it.
        */
    	ret = 0;
    }
    else
    {
        if(pszParentPath == NULL)
        {
            fprintf(stderr, "Error: paths in mapping file must be absolute if no input directory is specified.\n");
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
            if((pszParentPath[indirLen - 1] == '/') || (pszParentPath[indirLen - 1] == '\\'))
            {
                pszToAppend = "";
            }
            else
            {
                pszToAppend = "\\";
            }

            len = _snprintf(pszPath, HOST_PATH_MAX, "%s%s%s", pszParentPath, pszToAppend, asTemp);

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


/*  Checks whether the given path appears NOT to name a volume. Expects the
    path to be in massaged "//./diskname" format if it names a volume.
*/
bool IsRegularFile(
    const char *pszPath)
{
    return !((pszPath[0] == '\\')
          && (pszPath[1] == '\\')
          && (pszPath[2] == '.')
          && (pszPath[3] == '\\')
          && (strchr(&pszPath[4], '\\') == NULL)
          && (strchr(&pszPath[4], '/') == NULL));
}


#endif
