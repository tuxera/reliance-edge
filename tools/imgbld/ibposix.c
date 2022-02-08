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
    @brief Implements methods of the image builder tool specific to the POSIX
           configuration.
*/
#include <redfs.h>

#if (REDCONF_IMAGE_BUILDER == 1) && (REDCONF_API_POSIX == 1)

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <redposix.h>
#include <redtools.h>


int IbApiInit(void)
{
    int ret = 0;

    if(red_init() != 0)
    {
        printf("\n");
        fprintf(stderr, "Error number %d initializing file system.\n", (int)red_errno);
        ret = -1;
    }
    else
    {
        printf("\n");
    }

    return ret;
}


int IbApiUninit(void)
{
    int ret = 0;

    if(red_uninit() != 0)
    {
        ret = -1;
        fprintf(stderr, "Error number %d uninitializing file system.\n", (int)red_errno);
    }

    return ret;
}


/** @brief Copies from the file at the given path to the volume.

    @param bVolNum  The FSE volume to which to copy the file.  Unused in the
                    POSIX configuration; only here for FSE compatibility.
    @param pFile    Mapping for the file to be copied.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbCopyFile(
    uint8_t             bVolNum,
    const FILEMAPPING  *pFileMapping)
{
    int                 ret = 0;
    int32_t             iFildes = -1;
    FILE               *pFile;

    (void)bVolNum;

    printf("Copying file %s to %s\n", pFileMapping->szInFilePath, pFileMapping->szOutFilePath);

    /*  Open the file which is being copied.
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

    if(ret == 0)
    {
        iFildes = red_open(pFileMapping->szOutFilePath, RED_O_WRONLY|RED_O_CREAT|RED_O_EXCL|RED_O_APPEND);
        if(iFildes < 0)
        {
            ret = -1;
        }
    }

    /*  Copy data from input to target file
    */
    while(ret == 0)
    {
        size_t  rresult;
        int32_t pret;

        rresult = fread(gpCopyBuffer, 1U, gulCopyBufferSize, pFile);
        if(rresult > 0)
        {
            pret = red_write(iFildes, gpCopyBuffer, (uint32_t)rresult);
            if(pret < 0)
            {
                ret = -1;
            }
            else if((size_t)pret < rresult)
            {
                ret = -1;
                red_errno = RED_ENOSPC;
            }
        }

        if((ret == 0) && (rresult < gulCopyBufferSize))
        {
            if(ferror(pFile))
            {
                ret = -1;
                fprintf(stderr, "Error reading input file %s\n", pFileMapping->szInFilePath);
            }
            else
            {
                REDASSERT(feof(pFile));
            }

            break;
        }
    }

    if((iFildes > 0) && (red_close(iFildes) < 0))
    {
        ret = -1;
    }

    if(pFile != NULL)
    {
        (void)fclose(pFile);
    }

    if(ret == 0)
    {
        ret = IbCopyAttr(pFileMapping->szInFilePath, pFileMapping->szOutFilePath);
    }

    if(ret == -1)
    {
        switch(red_errno)
        {
            case RED_ENOSPC:
                fprintf(stderr, "Error: insufficient space to copy file %s.\n", pFileMapping->szInFilePath);
                break;
            case RED_EIO:
                fprintf(stderr, "Disk I/O error copying file %s.\n", pFileMapping->szInFilePath);
                break;
            case RED_ENFILE:
                fprintf(stderr, "Error: maximum number of files exceeded.\n");
                break;
            case RED_ENAMETOOLONG:
                fprintf(stderr, "Error: configured maximum file name length (%u) exceeded by file %s.\n", REDCONF_NAME_MAX, pFileMapping->szOutFilePath);
                break;
            case RED_EFBIG:
                fprintf(stderr, "Error: maximum file size exceeded.\n");
                break;
            default:
                /*  Other error types not expected.
                */
                fprintf(stderr, "Unexpected error %d in IbCopyFile()\n", (int)red_errno);
                REDERROR();
                break;
        }
    }

    return ret;
}


int IbPosixCopyDir(
    const char *pszVolName,
    const char *pszInDir)
{
    int32_t     ret = 0;
    bool        fMountFail = false;

    if(red_mount(pszVolName) != 0)
    {
        if(red_errno == RED_ENOENT)
        {
            fprintf(stderr, "Error mounting volume: invalid path prefix specified.\n");
        }
        else
        {
            fprintf(stderr, "Error number %d mounting volume.\n", (int)red_errno);
        }

        fMountFail = true;
        ret = -1;
    }

    if(ret == 0)
    {
        char    szInputDir[HOST_PATH_MAX];
        size_t  nInDirLen = strlen(pszInDir);

        if(nInDirLen >= sizeof(szInputDir))
        {
            /*  Not expected; the length of pszInDir should have already been checked.
            */
            fprintf(stderr, "Error: path too long: %s\n", pszInDir);
            REDERROR();
            ret = -1;
        }
        else
        {
            strcpy(szInputDir, pszInDir);

            /*  Get rid of any ending path separators.
            */
            while(IB_ISPATHSEP(szInputDir[nInDirLen - 1U]))
            {
                szInputDir[nInDirLen - 1U] = '\0';
                nInDirLen--;
            }

            ret = IbPosixCopyDirRecursive(pszVolName, szInputDir);
        }
    }

    if(ret == 0)
    {
        ret = red_transact(pszVolName);
        if(ret != 0)
        {
            fprintf(stderr, "Unexpected error number %d in red_transact.\n", (int)red_errno);
            ret = -1;
        }
    }

    if(!fMountFail)
    {
        if(red_umount(pszVolName) == -1)
        {
            fprintf(stderr, "Error number %d unmounting volume.\n", (int)red_errno);
            ret = -1;
        }
    }

    return ret;
}


/** @brief Create a directory using the Reliance Edge POSIX API.
*/
int IbPosixCreateDir(
    const char *pszVolName,
    const char *pszFullPath,
    const char *pszBasePath)
{
    int         ret = 0;
    char        szOutPath[HOST_PATH_MAX];

    ret = IbConvertPath(pszVolName, pszFullPath, pszBasePath, szOutPath);

    if(ret == 0)
    {
        if(red_mkdir(szOutPath) != 0)
        {
            ret = -1;
        }

        if(ret == -1)
        {
            switch(red_errno)
            {
                case RED_EIO:
                    fprintf(stderr, "Disk I/O error creating directory %s.\n", szOutPath);
                    break;
                case RED_ENOSPC:
                    fprintf(stderr, "Insufficient space on target volume.\n");
                    break;
                case RED_ENFILE:
                    fprintf(stderr, "Error: maximum number of files for volume %s exceeded.\n", pszVolName);
                    break;
                case RED_ENAMETOOLONG:
                    fprintf(stderr, "Error: configured maximum file name length (%u) exceeded by directory %s.\n", REDCONF_NAME_MAX, pszFullPath);
                    break;
                default:
                    /*  Other errors not expected.
                    */
                    fprintf(stderr, "Unexpected error %d in IbPosixCreateDir()\n", (int)red_errno);
                    REDERROR();
                    break;
            }
        }
    }

    return ret;
}


/** @brief Convert a host path to a Reliance Edge path.

    Takes a host system file path and converts it to a compatible path for the
    Reliance Edge POSIX API.

    @param pszVolName   The Reliance Edge volume name.
    @param pszFullPath  The full host file path.
    @param pszBasePath  A base path which will be removed from the back of
                        @p pszFullPath.
    @param pszOutPath   A char array pointer allocated at least HOST_PATH_MAX
                        chars at which to store the converted path.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbConvertPath(
    const char *pszVolName,
    const char *pszFullPath,
    const char *pszBasePath,
    char       *pszOutPath)
{
    int         ret = 0;
    const char *pszInPath = pszFullPath;
    size_t      nVolNameLen = strlen(pszVolName);
    size_t      nIndex = 0;

    while((pszInPath[0U] == pszBasePath[nIndex]) && (pszInPath[0U] != '\0'))
    {
        pszInPath++;
        nIndex++;
    }

    /*  After skipping the base path, the next char should be a path separator.
        Skip this too.
    */
    if(IB_ISPATHSEP(pszInPath[0U]))
    {
        pszInPath++;
    }

    if((strlen(pszInPath) + 1U + strlen(pszVolName)) >= (HOST_PATH_MAX - 1U))
    {
        fprintf(stderr, "Error: path name too long: %s\n", pszFullPath);
        ret = -1;
    }
    else
    {
        int len = sprintf(pszOutPath, "%s%c%s", pszVolName, REDCONF_PATH_SEPARATOR, pszInPath);

        REDASSERT(len >= (int)nVolNameLen);
        (void)len; /* Avoid build warnings when asserts are disabled. */

        for(nIndex = nVolNameLen + 1U; pszOutPath[nIndex] != '\0'; nIndex++)
        {
            if(IB_ISPATHSEP(pszOutPath[nIndex]))
            {
                pszOutPath[nIndex] = REDCONF_PATH_SEPARATOR;
            }
            else if(pszOutPath[nIndex] == REDCONF_PATH_SEPARATOR)
            {
                fprintf(stderr, "Error: unexpected target path separator character in path %s\n", pszInPath);
                ret = -1;
                break;
            }
        }
    }

    return ret;
}


#if HAVE_SETTABLE_ATTR
/** @brief Copies attributes from host file system to Reliance Edge.

    @param pszHostPath  Path to host file or directory whose attributes are to
                        be copied.
    @param pszRedPath   Path to Reliance Edge file or directory whose attributes
                        should be updated to match @p pszHostPath.

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbCopyAttr(
    const char *pszHostPath,
    const char *pszRedPath)
{
    int         ret;
    IBSTAT      sb;

    ret = IbStat(pszHostPath, &sb);
    if(ret == 0)
    {
      #if REDCONF_POSIX_OWNER_PERM == 1
        if(red_chmod(pszRedPath, sb.uMode & RED_S_IALLUGO) == -1)
        {
            fprintf(stderr, "Unexpected error %d from red_chmod()\n", (int)red_errno);
            ret = -1;
        }
        else if(red_chown(pszRedPath, sb.ulUid, sb.ulGid) == -1)
        {
            fprintf(stderr, "Unexpected error %d from red_chown()\n", (int)red_errno);
            ret = -1;
        }
        else
      #endif
        {
          #if REDCONF_INODE_TIMESTAMPS == 1
            uint32_t aulTimes[] = {sb.ulATime, sb.ulMTime};

            if(red_utimes(pszRedPath, aulTimes) == -1)
            {
                fprintf(stderr, "Unexpected error %d from red_utimes()\n", (int)red_errno);
                ret = -1;
            }
          #endif
        }
    }

    return ret;
}
#endif /* HAVE_SETTABLE_ATTR */

#endif /* (REDCONF_IMAGE_BUILDER == 1) && (REDCONF_API_POSIX == 1) */
