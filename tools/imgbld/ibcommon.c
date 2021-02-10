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
    @brief implements image builder methods shared between POSIX and FSE
*/
#include <redfs.h>

#if REDCONF_IMAGE_BUILDER == 1

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <redtools.h>


static int GetFileLen(FILE *fp, uint64_t *pLength);


/** @brief Copies from the file at the given path to the file of the given
           index.

    @param volNum       The FSE volume to which to copy the file. Unused in
                        POSIX configuration.
    @param pFileEntry   Mapping for the file to be copied.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbCopyFile(
    int                 volNum,
    const FILEMAPPING  *pFileMapping)
{
    int                 ret = 0;
    uint32_t            ulCurrLen = 0U;
    uint64_t            ullCurrOffset = 0U;
    uint64_t            ullFSize;
    FILE               *pFile;

    /*  Open the file which is being copied and query its length.
    */
    pFile = fopen(pFileMapping->asInFilePath, "rb");
    if(pFile == NULL)
    {
        if(errno == ENOENT)
        {
            fprintf(stderr, "Input file not found: %s\n", pFileMapping->asInFilePath);
        }
        else
        {
            fprintf(stderr, "Error opening input file: %s\n", pFileMapping->asInFilePath);
        }

        ret = -1;
    }
    else
    {
        ret = GetFileLen(pFile, &ullFSize);
        if(ret != 0)
        {
            fprintf(stderr, "Error getting file length: %s\n", pFileMapping->asInFilePath);
        }
    }

    /*  Force copy empty files on POSIX configuration.
    */
  #if REDCONF_API_POSIX == 1
    if((ret == 0) && (ullFSize == 0))
    {
        IbWriteFile(volNum, pFileMapping, 0, gpCopyBuffer, 0);
    }
  #endif

    /*  Copy data from input to target file
    */
    while((ret == 0) && (ullCurrOffset < ullFSize))
    {
        size_t rresult;

        ulCurrLen = ((ullFSize - ullCurrOffset) < gulCopyBufferSize ?
            (uint32_t) (ullFSize - ullCurrOffset) : gulCopyBufferSize);
        rresult = fread(gpCopyBuffer, 1U, ulCurrLen, pFile);

        if(rresult != ulCurrLen)
        {
            if(feof(pFile))
            {
                /*  Shouldn't happen; we just checked file length.
                */
                REDERROR();
                fprintf(stderr, "Warning: file size changed while reading file.\n");

                ulCurrLen = (uint32_t) rresult;
                ullFSize = ullCurrOffset + rresult;
            }
            else
            {
                REDASSERT(ferror(pFile));
                ret = -1;
                fprintf(stderr, "Error reading input file %s\n", pFileMapping->asInFilePath);

                break;
            }
        }

        if(ret == 0)
        {
            ret = IbWriteFile(volNum, pFileMapping, ullCurrOffset, gpCopyBuffer, ulCurrLen);

            ullCurrOffset += (uint64_t) ulCurrLen;
        }
    }

    if(pFile != NULL)
    {
        (void) fclose(pFile);
    }

    return ret;
}


/*  Gets the file length by seeking to the end and reading the file pointer
    offset. Seeks back to the beginning of the file on completion.
*/
static int GetFileLen(
    FILE       *fp,
    uint64_t   *pLength)
{
    int         ret = -1;

    if(fseek(fp, 0, SEEK_END) == 0)
    {
        long length = ftell(fp);

        if(length >= 0)
        {
            ret = 0;
            *pLength = (uint64_t) length;
        }

        (void) fseek(fp, 0, SEEK_SET);
    }

    return ret;
}


/** @brief Uses fopen(pszPath, "r") to determine if the given file path refers
           to an existing file.

    @param pszPath  File path to check.
    @param pfExists   Non-null pointer to bool. Assigned true if the file is
                    found, false if not found, indeterminate if an error occurs.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbCheckFileExists(
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
            (void) fclose(pFile);
        }
    }

    return ret;
}

#endif

