/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2022 Tuxera US Inc.
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
    @brief Implements methods of the image builder tool specific to the FSE
           configuration.
*/
#include <redfs.h>

#if (REDCONF_IMAGE_BUILDER == 1) && (REDCONF_API_FSE == 1)

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include <redfse.h>
#include <redtools.h>
#include <redtoolcmn.h>


typedef struct sSTRLISTENTRY STRLISTENTRY;
struct sSTRLISTENTRY
{
    char            szStr[MACRO_NAME_MAX_LEN + 1U];
    STRLISTENTRY   *pNext;
};


static int WriteToFile(uint8_t bVolNum, const FILEMAPPING *pFileMapping, uint64_t ullOffset, const void *pData, uint32_t ulDataLen);
static int WriteDefineOut(FILE *pFileOut, const FILEMAPPING *pFileMapping, STRLISTENTRY **ppListNames);
static int CheckFileExists(const char *pszPath, bool *pfExists);
static int GetFileLen(FILE *fp, uint64_t *pullLength);


/** @brief Get the tail entry of the linked list.
*/
static STRLISTENTRY *GetLastEntry(
    STRLISTENTRY *pStrList)
{
    STRLISTENTRY *pCurrEntry = pStrList;

    while((pCurrEntry != NULL) && (pCurrEntry->pNext != NULL))
    {
        pCurrEntry = pCurrEntry->pNext;
    }

    return pCurrEntry;
}


/** @brief Free a linked list of STRLISTENTRY's.

    `*ppStrList` will be NULL after calling.

    @param ppStrList    Pointer to the base STRLISTENTRY of the linked list.
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


/** @brief Free a linked list of FILELISTENTRY's.

    `*ppFileList` will be NULL after calling.

    @param ppFileList   Pointer to the base FILELISTENTRY of the linked list.
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

    printf("\n");

    if(rstat != 0)
    {
        fprintf(stderr, "Error number %d initializing file system.\n", (int)-rstat);
    }

    return (rstat == 0) ? 0 : -1;
}


int IbApiUninit(void)
{
    REDSTATUS rstat = RedFseUninit();

    if(rstat != 0)
    {
        fprintf(stderr, "Error number %d uninitializing file system.\n", (int)-rstat);
    }

    return (rstat == 0) ? 0 : -1;
}


/** @brief Build a list of files from a map file.

    Reads a file map file off the disk and fills a linked structure with the
    file indexes and names therein specified.  Prints any error messages to
    stderr.

    @param pszMapPath       The path to the file map file.
    @param pszIndirPath     The path to the input directory.  Should be set to
                            NULL if no input directory was specified.
    @param ppFileListHead   A pointer to a FILELISTENTRY to be filled.  A linked
                            list is allocated onto this pointer if successful,
                            and thus should be freed after use by passing it to
                            FreeFileList().

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbFseGetFileList(
    const char     *pszMapPath,
    const char     *pszIndirPath,
    FILELISTENTRY **ppFileListHead)
{
    int             ret = 0;
    FILELISTENTRY  *pCurrEntry = NULL;
    FILE           *pFile = NULL;
    uint32_t        ulLastIndex = 0U; /* Used to ensure no duplicate indexes. */

    *ppFileListHead = NULL;

    if(pszMapPath == NULL)
    {
        REDERROR();
        ret = -1;
    }

    if(ret == 0)
    {
        pFile = fopen(pszMapPath, "rt");
        if(pFile == NULL)
        {
            fprintf(stderr, "Error opening specified mapping file.\n");
            ret = -1;
        }
    }

    /*  Parse each line of the map file and store the mapping information as a
        new entry in ppFileListHead
    */
    while(ret == 0)
    {
        uint32_t    ulCurrIndex = 0U;
        char        szCurrPath[HOST_PATH_MAX];
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
                while((currChar != '\n') && (currChar != EOF))
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
        else if(fscanf(pFile, "%u\t", &ulCurrIndex) != 1) /* Read out the index number. */
        {
            ret = -1;
        }
        else if(ulCurrIndex < RED_FILENUM_FIRST_VALID)
        {
            fprintf(stderr, "Error in mapping file: file indexes less than %u are reserved.\n", RED_FILENUM_FIRST_VALID);
            ret = -1;
        }
        else if(ulCurrIndex <= ulLastIndex)
        {
            fprintf(stderr, "Syntax error in mapping file: file indexes must unique and in ascending order.\n");
            ret = -1;
        }
        else
        {
            ulLastIndex = ulCurrIndex;
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
            char szScanFormat[14U];

            /*  The host path may be surrounded with quotes, in which case the
                string between the quotes is the host path, or it may not be
                surrounded with quotes, in which case the host path terminates
                with the next whitespace character.
            */
            if(currChar == '"')
            {
                if(sprintf(szScanFormat, "%%%u[^\"]\"", HOST_PATH_MAX - 1U) < 0)
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
                else if(sprintf(szScanFormat, "%%%us", HOST_PATH_MAX - 1U) < 0)
                {
                    ret = -1;
                }
            }

            if((ret == 0) && (fscanf(pFile, szScanFormat, szCurrPath) != 1))
            {
                ret = -1;
            }
        }

        /*  Ensure the rest of the line is whitespace
        */
        while((ret == 0) && (currChar != '\n') && (currChar != EOF))
        {
            currChar = fgetc(pFile);
            if(!isspace(currChar) && (currChar != EOF))
            {
                fprintf(stderr, "Syntax error in mapping file: unexpected token %c at char #%ld.\n", currChar, ftell(pFile));
                ret = -1;
            }
        }

        if(ret == 0)
        {
            /*  If a relative path was specified, set it to be relative to the input
                directory.
            */
            ret = IbSetRelativePath(szCurrPath, pszIndirPath);
        }

        /*  Store index and host file path as a new entry in ppFileListHead
        */
        if(ret == 0)
        {
            FILELISTENTRY *pNewEntry = malloc(sizeof(*pNewEntry));

            if(pNewEntry == NULL)
            {
                fprintf(stderr, "Error allocating memory.\n");
                ret = -1;
            }

            strcpy(pNewEntry->fileMapping.szInFilePath, szCurrPath);
            pNewEntry->fileMapping.ulOutFileIndex = ulCurrIndex;
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
        (void)fclose(pFile);
    }

    if((ret == 0) && (pCurrEntry == NULL))
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


/** @brief Mounts the volume and copies files to it.
*/
int IbFseCopyFiles(
    uint8_t                 bVolNum,
    const FILELISTENTRY    *pFileList)
{
    REDSTATUS               err;
    int                     ret = 0;

    REDASSERT(pFileList != NULL);

    err = RedFseMount(bVolNum);
    if(err != 0)
    {
        fprintf(stderr, "Error number %d mounting volume.\n", (int)-err);
        ret = -1;
    }
    else
    {
        const FILELISTENTRY *pCurrEntry = pFileList;

        /*  Iterate over pFileList and copy files
        */
        while((ret == 0) && (pCurrEntry != NULL))
        {
            ret = IbCopyFile(bVolNum, &pCurrEntry->fileMapping);
            pCurrEntry = pCurrEntry->pNext;
        }

        if(ret == 0)
        {
            err = RedFseTransact(bVolNum);
            if(err != 0)
            {
                fprintf(stderr, "Unexpected error number %d transacting volume.\n", (int)-err);
                ret = -1;
            }
        }

        err = RedFseUnmount(bVolNum);
        if(err != 0)
        {
            fprintf(stderr, "Error number %d unmounting volume.\n", (int)-err);
            ret = -1;
        }
    }

    return ret;
}


/** @brief Copies from the file at the given path to the file of the given
           index.

    @param bVolNum      The FSE volume to which to copy the file.
    @param pFileMapping Mapping for the file to be copied.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbCopyFile(
    uint8_t             bVolNum,
    const FILEMAPPING  *pFileMapping)
{
    int                 ret = 0;
    uint64_t            ullCurrOffset = 0U;
    uint64_t            ullFSize = 0U; /* Init'd for picky compilers */
    FILE               *pFile;

    /*  Open the file which is being copied and query its length.
    */
    pFile = fopen(pFileMapping->szInFilePath, "rb");
    if(pFile == NULL)
    {
        if(errno == ENOENT)
        {
            fprintf(stderr, "Input file not found: %s\n", pFileMapping->szInFilePath);
        }
        else
        {
            fprintf(stderr, "Error opening input file: %s\n", pFileMapping->szInFilePath);
        }

        ret = -1;
    }
    else
    {
        ret = GetFileLen(pFile, &ullFSize);
        if(ret != 0)
        {
            fprintf(stderr, "Error getting file length: %s\n", pFileMapping->szInFilePath);
        }
    }

    /*  Copy data from input to target file
    */
    while((ret == 0) && (ullCurrOffset < ullFSize))
    {
        uint32_t    ulCurrLen = 0U;
        size_t      rresult;

        ulCurrLen = ((ullFSize - ullCurrOffset) < gulCopyBufferSize) ?
            (uint32_t)(ullFSize - ullCurrOffset) : gulCopyBufferSize;
        rresult = fread(gpCopyBuffer, 1U, ulCurrLen, pFile);

        if(rresult != ulCurrLen)
        {
            if(feof(pFile))
            {
                /*  Shouldn't happen; we just checked file length.
                */
                REDERROR();
                fprintf(stderr, "Warning: file size changed while reading file.\n");

                ulCurrLen = (uint32_t)rresult;
                ullFSize = ullCurrOffset + rresult;
            }
            else
            {
                REDASSERT(ferror(pFile));
                ret = -1;
                fprintf(stderr, "Error reading input file %s\n", pFileMapping->szInFilePath);

                break;
            }
        }

        if(ret == 0)
        {
            ret = WriteToFile(bVolNum, pFileMapping, ullCurrOffset, gpCopyBuffer, ulCurrLen);

            ullCurrOffset += ulCurrLen;
        }
    }

    if(pFile != NULL)
    {
        (void)fclose(pFile);
    }

    return ret;
}


/** @brief Writes file data to a file.

    This method may be called multiple times to write consecutive chunks of file
    data.

    @param bVolNum      Volume number.
    @param pFileMapping The file being copied.  File data will be written to
                        ::ulOutFileIndex.
    @param ullOffset    The position in the file to which to write data.
    @param pData        Data to write to the file.
    @param ulDataLen    The number of bytes in @p pData to write.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
static int WriteToFile(
    uint8_t             bVolNum,
    const FILEMAPPING  *pFileMapping,
    uint64_t            ullOffset,
    const void         *pData,
    uint32_t            ulDataLen)
{
    int                 ret = 0;
    int32_t             wResult;

    /*  Only print out a message for the first write to a file.
    */
    if(ullOffset == 0U)
    {
        printf("Copying file %s to index %lu\n", pFileMapping->szInFilePath, (unsigned long)pFileMapping->ulOutFileIndex);
    }

    wResult = RedFseWrite(bVolNum, pFileMapping->ulOutFileIndex, ullOffset, ulDataLen, pData);

    if(wResult < 0)
    {
        ret = -1;

        switch(wResult)
        {
            case -RED_EFBIG:
                fprintf(stderr, "Error: input file too big: %s\n", pFileMapping->szInFilePath);
                break;
            case -RED_EBADF:
                fprintf(stderr, "Error: invalid file index %lu\n", (unsigned long)pFileMapping->ulOutFileIndex);
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
                fprintf(stderr, "Unexpected error %d from RedFseWrite()\n", (int)wResult);
                REDERROR();
                break;
        }
    }
    else if((uint32_t)wResult != ulDataLen)
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


/** @brief Output C macros for the file list.

    Outputs a list of C/C++ macros identifying the files in the given file map
    and outputs them based on the given given imgbld options.  If the imgbld
    options provide a defines output file but there are errors accessing it,
    then the user is alerted and the output is written to stdout.

    @param pFileList    The map of input files paths processed and their file
                        indexes.
    @param pParam       The struct of command line options.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbFseOutputDefines(
    FILELISTENTRY      *pFileList,
    const IMGBLDPARAM  *pParam)
{
    int                 ret = 0;
    FILELISTENTRY      *pCurrEntry = pFileList;
    STRLISTENTRY       *pStrList = NULL;
    FILE               *pFileOut = NULL;
    bool                fUseFile = (pParam->pszDefineFile != NULL);

    /*  When using a defines file, check if file exists and confirm overwrite
        unless nowarn was specified
    */
    if(fUseFile && !pParam->fNoWarn)
    {
        bool fExists;

        ret = CheckFileExists(pParam->pszDefineFile, &fExists);

        if((ret == 0) && fExists)
        {
            fprintf(stderr, "Specified defines file %s already exists.\n", pParam->pszDefineFile);

            fUseFile = RedConfirmOperation("Overwrite?");
        }
    }

    if((ret == 0) && fUseFile)
    {
        pFileOut = fopen(pParam->pszDefineFile, "w");
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
        printf("Error accessing specified defines output file.\nWriting defines to stdout.\n");

        fUseFile = false;
        ret = 0;
    }

    if(!fUseFile)
    {
        pFileOut = stdout;
    }

    /*  Iterate over pFileList and output #define information.
    */
    while((ret == 0) && (pCurrEntry != NULL))
    {
        ret = WriteDefineOut(pFileOut, &pCurrEntry->fileMapping, &pStrList);
        pCurrEntry = pCurrEntry->pNext;
    }

    FreeStrList(&pStrList);

    if((pFileOut != NULL) && (pFileOut != stdout))
    {
        fclose(pFileOut);
    }

    return ret;
}


/** @brief Creates a macro name for the given file and outputs it on the given
           stream.

    @param pFileOut     The open output stream to write to.
    @param pFileMapping The file for which to create a macro.
    @param ppListNames  A linked list of macro names already used.  The name
                        generated by this function will be appended.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
static int WriteDefineOut(
    FILE               *pFileOut,
    const FILEMAPPING  *pFileMapping,
    STRLISTENTRY      **ppListNames)
{
    int                 ret = 0;
    size_t              nFromIndex = 0U;
    size_t              nToIndex = 5U;  /* Index of next char after "FILE_" */
    STRLISTENTRY       *pCurrEntry = malloc(sizeof(*pCurrEntry));

    REDASSERT(ppListNames != NULL);
    REDASSERT(pFileMapping->szInFilePath != NULL);

    if(pCurrEntry == NULL)
    {
        fprintf(stderr, "Error allocating memory.\n");
        ret = -1;
    }

    if(ret == 0)
    {
        pCurrEntry->pNext = NULL;

        (void)strcpy(pCurrEntry->szStr, "FILE_");

        /*  Copy host file path to current entry, replacing non-compatible
            characters for preprocessor symbols with underscores.
        */
        while((nToIndex < MACRO_NAME_MAX_LEN) && (pFileMapping->szInFilePath[nFromIndex] != '\0'))
        {
            char c = pFileMapping->szInFilePath[nFromIndex];

            if(IB_ISPATHSEP(c))
            {
                nToIndex = 5U; /* Reset output: only use the file name, not path */
                nFromIndex++;
            }
            else
            {
                if(!isalnum(c) && (c != '_'))
                {
                    c = '_';
                }

                pCurrEntry->szStr[nToIndex] = c;

                nFromIndex++;
                nToIndex++;
            }
        }

        pCurrEntry->szStr[nToIndex] = '\0';
    }

    /*  Ensure current entry is not a duplicate, appending a number (or
        increment a number already appended) if an identical string entry is
        found.
    */
    while(ret == 0)
    {
        STRLISTENTRY *pCmpEntry = *ppListNames;

        while(pCmpEntry != NULL)
        {
            if(strncmp(pCmpEntry->szStr, pCurrEntry->szStr, MACRO_NAME_MAX_LEN) == 0)
            {
                /*  Duplicate name found.  Append a 0 or increment the number
                    found at the end.
                */
                size_t nEndStr = strlen(pCurrEntry->szStr);
                size_t nBeginNum = nEndStr;

                /*  Don't allow nBeginNum closer than 6 from the beginning for
                    "FILE_" plus one character
                */
                while((nBeginNum > 6U) && isdigit(pCurrEntry->szStr[nBeginNum - 1U]))
                {
                    nBeginNum--;
                }

                if(nBeginNum == nEndStr)
                {
                    if(nEndStr == MACRO_NAME_MAX_LEN)
                    {
                        pCurrEntry->szStr[nEndStr - 1U] = '\0';
                    }

                    strcat(pCurrEntry->szStr, "0");
                }
                else
                {
                    unsigned num;
                    int stat = sscanf(&pCurrEntry->szStr[nBeginNum], "%u", &num);

                    /*  We just checked and found decimal digits. Scanf should
                        find them too.
                    */
                    REDASSERT(stat == 1);
                    (void)stat; /* Avoid warnings when assertions are disabled. */

                    num++;
                    if(((num % 10U) == 0U) && (nEndStr == MACRO_NAME_MAX_LEN))
                    {
                        /*  Overwrite with the new digit -- no space is left.
                        */
                        nBeginNum--;
                    }

                    sprintf(&pCurrEntry->szStr[nBeginNum], "%u", num);
                }

                /*  Break to outer loop; check again and see if the new name is
                    a duplicate.
                */
                break;
            }

            pCmpEntry = pCmpEntry->pNext;
        }

        if(pCmpEntry == NULL)
        {
            /*  Final entry reached without finding a duplicate.
            */
            break;
        }
    }

    fprintf(pFileOut, "#define %s %lu\n", pCurrEntry->szStr, (unsigned long)pFileMapping->ulOutFileIndex);

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


/** @brief Determine if the given file path refers to an existing file.

    @param pszPath  File path to check.
    @param pfExists Non-null pointer to bool.  Assigned true if the file is
                    found, false if not found, indeterminate if an error occurs.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
static int CheckFileExists(
    const char *pszPath,
    bool       *pfExists)
{
    int         ret = 0;
    FILE       *pFile;

    if((pfExists == NULL) || (pszPath == NULL))
    {
        ret = -1;
    }

    if(ret == 0)
    {
        pFile = fopen(pszPath, "r");

        if(pFile == NULL)
        {
            if(errno == ENOENT)
            {
                *pfExists = false;
            }
            else
            {
                ret = -1;
            }
        }
        else
        {
            *pfExists = true;
            (void)fclose(pFile);
        }
    }

    return ret;
}


/** @brief Get the file size.

    Gets the file length by seeking to the end and reading the file pointer
    offset.  Seeks back to the beginning of the file on completion.
*/
static int GetFileLen(
    FILE       *fp,
    uint64_t   *pullLength)
{
    int         ret = -1;

    if(fseek(fp, 0, SEEK_END) == 0)
    {
        long length = ftell(fp);

        if(length >= 0)
        {
            ret = 0;
            *pullLength = (uint64_t)length;
        }

        (void)fseek(fp, 0, SEEK_SET);
    }

    return ret;
}

#endif /* (REDCONF_IMAGE_BUILDER == 1) && (REDCONF_API_FSE == 1) */
