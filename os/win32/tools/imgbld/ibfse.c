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
    @brief Implements methods of the image builder tool specific to the FSE
           configuration.
*/
#include <redfs.h>

#if REDCONF_IMAGE_BUILDER == 1 && REDCONF_API_POSIX == 0

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <redfse.h>

#include "../wintlcmn.h"
#include "ibheader.h"


typedef struct sSTRLISTENTRY STRLISTENTRY;
struct sSTRLISTENTRY
{
    char            asStr[MACRO_NAME_MAX_LEN + 1];
    STRLISTENTRY   *pNext;
};


static int WriteDefineOut(FILE *pFileOut, const FILEMAPPING *pFileMapping, STRLISTENTRY **ppListNames);

/*  Private helper method to get the tail entry of a linked list
*/
static STRLISTENTRY *GetLastEntry(STRLISTENTRY *pStrList)
{
    STRLISTENTRY *pCurrEntry = pStrList;

    while(pCurrEntry != NULL && pCurrEntry->pNext != NULL)
    {
        pCurrEntry = pCurrEntry->pNext;
    }

    return pCurrEntry;
}


/*  Private helper function to free a linked list of STRLISTENTRY's. *ppStrList
    will be NULL after calling

    @param ppStrList    Pointer to the base STRLISTENTRY of the linked list.
                        Returns without taking action if this is NULL.
*/
static void FreeStrList(
    STRLISTENTRY **ppStrList)
{
    while(*ppStrList != NULL)
    {
        STRLISTENTRY *pLastEntry = *ppStrList;

        *ppStrList = (*ppStrList)->pNext;
        free(pLastEntry);
    }
}


/** Helper function to free a linked list of FILELISTENTRY's.
    *ppFileList will be NULL after calling.

    @param ppFileList   Pointer to the base FILELISTENTRY of the linked list.
                        Returns without taking action if this is NULL.
*/
void FreeFileList(
    FILELISTENTRY **ppFileList)
{
    while(*ppFileList != NULL)
    {
        FILELISTENTRY *pLastEntry = *ppFileList;

        *ppFileList = (*ppFileList)->pNext;
        free(pLastEntry);
    }
}


int IbApiInit(void)
{
    REDSTATUS rstat = RedFseInit();

    fprintf(stdout, "\n");

    if(rstat != 0)
    {
        fprintf(stderr, "Error number %d initializing file system.\n", -rstat);
    }

    return (rstat == 0) ? 0 : -1;
}


int IbApiUninit(void)
{
    REDSTATUS rstat = RedFseUninit();

    if(rstat != 0)
    {
        fprintf(stderr, "Error number %d uninitializing file system.\n", -rstat);
    }

    return (rstat == 0) ? 0 : -1;
}


/** @brief Reads a file map file off the disk and fills a linked structure with
           the file indexes and names therein specified. Prints any error
           messages to stderr.

    @param pszMapPath       The path to the file map file.
    @param pszIndirPath     The path to the input directory. Should be set to
                            NULL if no input directory was specified.
    @param ppFileListHead   A pointer to a FILELISTENTRY to be filled. A linked
                            list is allocated onto this pointer if successful,
                            and thus should be freed after use by passing it to
                            FreeFileList.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int GetFileList(
    const char     *pszMapPath,
    const char     *pszIndirPath,
    FILELISTENTRY **ppFileListHead)
{
    int             ret = 0;
    FILELISTENTRY  *pCurrEntry = NULL;
    FILE           *pFile = NULL;
    uint32_t        lastIndex = 0; /* Used to ensure no duplicate indexes. */

    *ppFileListHead = NULL;
    if(pszMapPath == NULL)
    {
        REDASSERT(false);
        ret = -1;
    }

    if(ret == 0)
    {
        pFile = fopen(pszMapPath, "rt");
        if(pFile == NULL)
        {
            fprintf(stderr, "Error openning specified mapping file.\n");
            ret = -1;
        }
    }

    /*  Parse each line of the map file and store the mapping information as a
        new entry in ppFileListHead
    */
    while (ret == 0)
    {
        uint32_t    currIndex = 0;
        char        currPath[WIN_FILENAME_MAX];
        int         currChar = fgetc(pFile);

        /*  Skip over comment lines and whitespace between lines (allowing
            indentation etc).
        */
        while(isspace(currChar) || (currChar == '#'))
        {
            if(currChar == '#')
            {
                /*  Skip over the entire comment line.
                */
                while(currChar != '\n' && currChar != EOF)
                {
                    currChar = fgetc(pFile);
                }
            }
            else
            {
                currChar = fgetc(pFile);
            }
        }

        if(currChar == EOF)
        {
            if(ferror(pFile) != 0)
            {
                ret = -1;
            }

            break;
        }

        /*  Put the last char, which was neither whitespace nor a comment, back
            in the stream.
        */
        currChar = ungetc(currChar, pFile);

        if(currChar == EOF)
        {
            ret = -1;
        }
        else
        {
            /*  Read out the index number.
            */
            if(fscanf(pFile, "%u\t", &currIndex) != 1)
            {
                ret = -1;
            }
            else if(currIndex <= 1)
            {
                fprintf(stderr, "Error in mapping file: file indexes 0 and 1 are reserved.\n");
                ret = -1;
            }
            else if(currIndex <= lastIndex)
            {
                fprintf(stderr, "Syntax error in mapping file: file indexes must unique and in ascending order.\n");
                ret = -1;
            }
            else
            {
                lastIndex = currIndex;
            }
        }

        /*  Read the host path to the file for the index number.
        */
        if(ret == 0)
        {
            currChar = fgetc(pFile);

            if(currChar == EOF)
            {
                ret = -1;
            }
        }

        if(ret == 0)
        {
            char asScanFormat[14];

            /*  The host path may be surrounded with quotes, in which case the
                string between the quotes is the host path, or it may not be
                surrounded with quotes, in which case the host path terminates
                with the next whitespace character.
            */
            if(currChar == '"')
            {
                if(sprintf(asScanFormat, "%%%u[^\"]\"", WIN_FILENAME_MAX - 1) < 0)
                {
                    ret = -1;
                }
                else if(fscanf(pFile, asScanFormat, currPath) != 1)
                {
                    ret = -1;
                }
            }
            else
            {
                /*  No quotes, so the character is part of the path and needs to
                    be put back into the stream.
                */
                currChar = ungetc(currChar, pFile);
                if(currChar == EOF)
                {
                    ret = -1;
                }
                else if(sprintf(asScanFormat, "%%%us", WIN_FILENAME_MAX - 1) < 0)
                {
                    ret = -1;
                }
                else if(fscanf(pFile, asScanFormat, currPath) != 1)
                {
                    ret = -1;
                }
            }
        }

        /*  Ensure the rest of the line is whitespace
        */
        while(ret == 0 && currChar != '\n' && currChar != EOF)
        {
            currChar = fgetc(pFile);
            if(!isspace(currChar) && currChar != EOF)
            {
                fprintf(stderr, "Syntax error in mapping file: unexpected token %c at char #%d.\n", currChar, ftell(pFile));
                ret = -1;
            }
        }

        /*  If a relative path was specified, set it to be relative to the input
            directory.
        */
        if(ret == 0 && !PathIsAbsolute(currPath))
        {
            if(pszIndirPath == NULL)
            {
                fprintf(stderr, "Error: paths in mapping file must be absolute if no input directory is specified.\n");
                ret = -1;
            }
            else
            {
                char    asTemp[WIN_FILENAME_MAX];
                int     len;
                char   *pszToAppend;
                size_t  indirLen = strlen(pszIndirPath);

                REDASSERT(indirLen != 0);

                strcpy(asTemp, currPath);

                /*  Ensure a path separator comes between the input directory
                    and the specified relative path.
                */
                if((pszIndirPath[indirLen - 1] == '/') || (pszIndirPath[indirLen - 1] == '\\'))
                {
                    pszToAppend = "";
                }
                else
                {
                    pszToAppend = "\\";
                }

                len = _snprintf(currPath, WIN_FILENAME_MAX, "%s%s%s", pszIndirPath, pszToAppend, asTemp);

                if((len < 0) || (len >= WIN_FILENAME_MAX))
                {
                    fprintf(stderr, "Error: file path too long: %s%s%s", pszIndirPath, pszToAppend, asTemp);
                    ret = -1;
                }
            }
        }

        /*  Store index and host file path as a new entry in ppFileListHead
        */
        if(ret == 0)
        {
            FILELISTENTRY *pNewEntry = malloc(sizeof(*pNewEntry));

            if(!pNewEntry)
            {
                fprintf(stderr, "Error allocating memory.\n");
                ret = -1;
            }

            strncpy(pNewEntry->fileMapping.asInFilePath, currPath, WIN_FILENAME_MAX);
            pNewEntry->fileMapping.ulOutFileIndex = currIndex;
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
        }

        if(currChar == EOF)
        {
            break;
        }
    }

    if(pFile != NULL)
    {
        (void) fclose(pFile);
    }

    if(ret == 0 && pCurrEntry == NULL)
    {
        fprintf(stderr, "Warning: empty or invalid mapping file specified.\n");
    }
    else if(ret != 0)
    {
        fprintf(stderr, "Error reading specified mapping file.\n");

        FreeFileList(ppFileListHead);
    }

    return ret;
}


/** @brief Checks whether a Windows file path appears to be relative or
           absolute.

    @param pszPath  The file path to check.

    @return True if the @p pszPath appears to be an absolute path; false
            otherwise.
*/
bool PathIsAbsolute(
    const char *pszPath)
{
    bool    fIsAbsolute = false;

    /*  Check whether pszPath begins with a drive letter.
    */
    if(     (    ((pszPath[0U] >= 'A') && (pszPath[0U] <= 'Z'))
              || ((pszPath[0U] >= 'a') && (pszPath[0U] <= 'z')))
         && (pszPath[1U] == ':')
         && ((pszPath[2U] == '\\') || (pszPath[2U] == '/')))
    {
        fIsAbsolute = true;
    }

    return fIsAbsolute;
}


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
int CreateFileListWin(
    const char     *pszDirPath,
    FILELISTENTRY **ppFileListHead)
{
    int             ret = 0;
    char            asSPath[WIN_FILENAME_MAX];
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

    if(pathLen + strlen(pszToAppend) >= WIN_FILENAME_MAX)
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
                len = _snprintf(pNewEntry->fileMapping.asInFilePath, WIN_FILENAME_MAX, "%s%s%s",
                    pszDirPath, pszToAppend, sFindData.cFileName);

                if((len < 0) || (len >= WIN_FILENAME_MAX))
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


/** @brief Mounts the volume and copies files to it.
*/
int IbFseCopyFiles(
    int                     volNum,
    const FILELISTENTRY    *pFileList)
{
    REDSTATUS               err;
    int                     ret = 0;
    const FILELISTENTRY    *currEntry = pFileList;
    bool                    mountfail = false;

    REDASSERT(pFileList != NULL);

    err = RedFseMount(volNum);
    if(err != 0)
    {
        fprintf(stderr, "Error number %d mounting volume.\n", -err);
        mountfail = true;
        ret = -1;
    }
    else
    {
        /*  Iterate over pFileList and copy files
        */
        while((ret == 0) && (currEntry != NULL))
        {
            ret = IbCopyFile(volNum, &currEntry->fileMapping);
            currEntry = currEntry->pNext;
        }

        if(ret == 0)
        {
            err = RedFseTransact(volNum);
            if(err != 0)
            {
                fprintf(stderr, "Unexpected error number %d in RedFseTransact.\n", -err);
                ret = -1;
            }
        }

        if(!mountfail)
        {
            err = RedFseUnmount(volNum);
            if(err != 0)
            {
                fprintf(stderr, "Error number %d unmounting volume.\n", -err);
                ret = -1;
            }
        }
    }

    return ret;
}


int IbWriteFile(
    int                 volNum,
    const FILEMAPPING  *pFileMapping,
    uint64_t            ullOffset,
    void               *pData,
    uint32_t            ulDataLen)
{
    int                 ret = 0;
    int32_t             wResult;

    /*  Only print out a mesage for the first write to a file.
    */
    if(ullOffset == 0U)
    {
        fprintf(stdout, "Copying file %s to index %d\n", pFileMapping->asInFilePath, pFileMapping->ulOutFileIndex);
    }

    wResult = RedFseWrite(volNum, pFileMapping->ulOutFileIndex, ullOffset, ulDataLen, pData);

    if(wResult < 0)
    {
        ret = -1;

        switch(wResult)
        {
            case -RED_EFBIG:
                fprintf(stderr, "Error: input file too big: %s\n", pFileMapping->asInFilePath);
                break;
            case -RED_EBADF:
                fprintf(stderr, "Error: invalid file index %d\n", pFileMapping->ulOutFileIndex);
                break;
            case -RED_ENOSPC:
                fprintf(stderr, "Error: insufficient space on target volume.\n");
                break;
            case -RED_EIO:
                fprintf(stderr, "Error writing to target volume.\n");
                break;
            default:
                /*  Other errors not expected.
                */
                REDERROR();
                break;
        }
    }
    else if((uint32_t) wResult != ulDataLen)
    {
        ret = -1;
        fprintf(stderr, "Error: insufficient space on target volume.\n");
    }
    else
    {
        /*  Desired number of bytes were written, so the operation was
            successful and there's nothing else to do.
        */
    }

    return ret;
}


/** @brief Outputs a list of C/C++ macros identifying the files in the given
           file map and outputs them based on the given given imgbld options.
           If the imgbld options provide a defines output file but there are
           errors accessing it, then the user is alerted and the output is
           written to stdout.

    @param pFileList    The map of input files paths processed and their file
                        indexes
    @param sOptions     The struct of command line options.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int OutputDefinesFile(
    FILELISTENTRY          *pFileList,
    const IMGBLDOPTIONS    *sOptions)
{
    int                     ret = 0;
    FILELISTENTRY          *pCurrEntry = pFileList;
    STRLISTENTRY           *pStrList = NULL;
    FILE                   *pFileOut = NULL;
    bool                    fUseFile = (sOptions->pszDefineFile != NULL);

    /*  When using a defines file, check if file exists and confirm overwrite
        unless nowarn was specified
        */
    if(fUseFile && !sOptions->fNowarn)
    {
        bool fExists;

        ret = CheckFileExists(sOptions->pszDefineFile, &fExists);

        if((ret == 0) && fExists)
        {
            fprintf(stderr, "Specified defines file %s already exists.\n", sOptions->pszDefineFile);

            fUseFile = ConfirmOperation("Overwrite?");
        }
    }

    if((ret == 0) && fUseFile)
    {
        pFileOut = fopen(sOptions->pszDefineFile, "w");
        if(pFileOut == NULL)
        {
            ret = -1;
        }
    }

    /*  In the case of error accessing defines file, warn user and revert to
        console output.
    */
    if(ret == -1)
    {
        fprintf(stderr, "Error accessing specified defines output file.\n");
        fprintf(stdout, "Error accessing specified defines output file.\nWriting defines to stdout.\n");

        fUseFile = false;
        ret = 0;
    }

    if(!fUseFile)
    {
        pFileOut = stdout;
    }

    /*  Iterate over pFileList and output #DEFINE information.
    */
    while(ret == 0 && pCurrEntry != NULL)
    {
        ret = WriteDefineOut(pFileOut, &pCurrEntry->fileMapping, &pStrList);
        pCurrEntry = pCurrEntry->pNext;
    }

    FreeStrList(&pStrList);

    if(pFileOut != NULL)
    {
        fclose(pFileOut);
    }

    return ret;
}


/** @brief Creates a macro name for the given file and outputs it on the given
           stream.

    @param pfileOut      The open output stream to write to.
    @param fmapEntry    The file for which to create a macro
    @param ppListNames  A linked list of macro names already used. The name
                        generated by this function will be appended.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
static int WriteDefineOut(
    FILE               *pfileOut,
    const FILEMAPPING  *pFileMapping,
    STRLISTENTRY      **ppListNames)
{
    int                 ret = 0;
    int                 fromIndex = 0;
    int                 toIndex = 5;    /* Index of next char after "FILE_" */
    STRLISTENTRY       *pCurrEntry = malloc(sizeof(*pCurrEntry));

    REDASSERT(ppListNames != NULL);
    REDASSERT(pFileMapping->asInFilePath != NULL);

    if(pCurrEntry == NULL)
    {
        fprintf(stderr, "Error allocating memory.\n");
        ret = -1;
    }

    if(ret == 0)
    {
        pCurrEntry->pNext = NULL;

        (void) strcpy(pCurrEntry->asStr, "FILE_");

        /*  Copy host file path to current entry, replacing non-compatible
            characters for preprocessor symbols with underscores.
        */
        while((toIndex < MACRO_NAME_MAX_LEN) && (pFileMapping->asInFilePath[fromIndex] != '\0'))
        {
            char c = pFileMapping->asInFilePath[fromIndex];

            if((c == '\\') || (c == '/'))
            {
                toIndex = 5; /* Reset output: only use the file name, not path */
                fromIndex++;
            }
            else
            {
                if(!isalnum(c) && (c != '_'))
                {
                    c = '_';
                }

                pCurrEntry->asStr[toIndex] = c;

                fromIndex++;
                toIndex++;
            }
        }

        pCurrEntry->asStr[toIndex] = '\0';
    }

    /*  Ensure current entry is not a duplicate, appending a number (or
        increment a number already appended) if an identical string entry is
        found.
    */
    while(ret == 0)
    {
        STRLISTENTRY *cmpEntry = *ppListNames;

        while(cmpEntry != NULL)
        {
            if(strncmp(cmpEntry->asStr, pCurrEntry->asStr, MACRO_NAME_MAX_LEN) == 0)
            {
                /*  Duplicate name found. Append a 0 or increment the number
                    found at the end.
                */
                uint32_t ulEndStr = strlen(pCurrEntry->asStr);
                uint32_t ulBeginNum = ulEndStr;



                /*  Don't allow pBeginNum closer than 6 from the beginning for
                    "FILE_" plus one character
                */
                while((ulBeginNum > 6U) && isdigit(pCurrEntry->asStr[ulBeginNum - 1]))
                {
                    ulBeginNum--;
                }

                if(ulBeginNum == ulEndStr)
                {
                    if(ulEndStr == MACRO_NAME_MAX_LEN)
                    {
                        pCurrEntry->asStr[ulEndStr - 1] = '\0';
                    }

                    strcat(pCurrEntry->asStr, "0");
                }
                else
                {
                    uint32_t num;
                    int stat = sscanf(&pCurrEntry->asStr[ulBeginNum], "%lu", &num);

                    /*  We just checked and found decimal digits. Scanf should
                        find them too.
                    */
                    REDASSERT(stat == 1);

                    num++;
                    if(((num % 10) == 0) && (ulEndStr == MACRO_NAME_MAX_LEN))
                    {
                        /*  Overwrite with the new digit--no space is left.
                        */
                        ulBeginNum--;
                    }

                    sprintf(&pCurrEntry->asStr[ulBeginNum], "%d", num);
                }

                /*  Break to outer loop; check again and see if the new name is
                    a duplicate.
                */
                break;
            }

            cmpEntry = cmpEntry->pNext;
        }

        if(cmpEntry == NULL)
        {
            /*  Final entry reached without finding a duplicate.
            */
            break;
        }
    }

    fprintf(pfileOut, "#define %s %d\n", pCurrEntry->asStr, pFileMapping->ulOutFileIndex);

    if(*ppListNames == NULL)
    {
        *ppListNames = pCurrEntry;
    }
    else
    {
        GetLastEntry(*ppListNames)->pNext = pCurrEntry;
    }

    return ret;
}

#endif /* REDCONF_IMAGE_BUILDER == 1 && REDCONF_API_POSIX == 0 */

