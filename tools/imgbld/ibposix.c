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
    @brief Implements methods of the image builder tool specific to the POSIX
           configuration.
*/
#include <redfs.h>

#if (REDCONF_IMAGE_BUILDER == 1) && (REDCONF_API_POSIX == 1)

#include <stdio.h>
#include <stdlib.h>

#include <redposix.h>
#include <redtools.h>


int IbApiInit(void)
{
    int ret = 0;

    if(red_init() != 0)
    {
        printf("\n");
        fprintf(stderr, "Error number %d initializing file system.\n", red_errno);
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
        fprintf(stderr, "Error number %d uninitializing file system.\n", red_errno);
    }

    return ret;
}


/** @brief Writes file data to a file. This method may be called multiple times
           to write consecutive chunks of file data.

    @param volNum           Unused parameter; maintained for compatability with
                            FSE IbWriteFile.
    @param psFileMapping    The file being copied. File data will be written to
                            ::asOutFilePath.
    @param ullOffset        The position in the file to which to write data.
    @param pData            Data to write to the file.
    @param ulDataLen        The number of bytes in @p pData to write

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbWriteFile(
    int                 volNum,
    const FILEMAPPING  *psFileMapping,
    uint64_t            ullOffset,
    void               *pData,
    uint32_t            ulDataLen)
{
    int                 ret = 0;
    int32_t             fd;

    (void) volNum;

    /*  Only print out a mesage for the first write to a file.
    */
    if(ullOffset == 0U)
    {
        printf("Copying file %s to %s\n", psFileMapping->asInFilePath, psFileMapping->asOutFilePath);
    }

    fd = red_open(psFileMapping->asOutFilePath, RED_O_WRONLY|RED_O_CREAT|RED_O_APPEND);
    if(fd == -1)
    {
        ret = -1;
    }
    else
    {
        int32_t pret;

        REDASSERT(ullOffset <= INT64_MAX);
        if(red_lseek(fd, (int64_t) ullOffset, RED_SEEK_SET) == -1)
        {
            ret = -1;
        }
        else
        {
            pret = red_write(fd, pData, ulDataLen);
            if(pret < 0)
            {
                ret = -1;
            }
            else if((uint32_t)pret < ulDataLen)
            {
                ret = -1;
                red_errno = RED_ENOSPC;
            }
        }

        pret = red_close(fd);
        if(pret == -1)
        {
            ret = -1;
        }
    }

    if(ret == -1)
    {
        switch(red_errno)
        {
            case RED_ENOSPC:
                fprintf(stderr, "Error: insufficient space to copy file %s.\n", psFileMapping->asInFilePath);
                break;
            case RED_EIO:
                fprintf(stderr, "Disk IO error copying file %s.\n", psFileMapping->asInFilePath);
                break;
            case RED_ENFILE:
                fprintf(stderr, "Error: maximum number of files exceeded.\n");
                break;
            case RED_ENAMETOOLONG:
                fprintf(stderr, "Error: maximum file name length exceeded. Max length: %d.\n", REDCONF_NAME_MAX);
                break;
            case RED_EFBIG:
                fprintf(stderr, "Error: maximum file size exceeded.");
                break;
            default:
                /*  Other error types not expected.
                */
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
    bool        mountfail = false;

    if(red_mount(pszVolName) != 0)
    {
        if(red_errno == RED_ENOENT)
        {
            fprintf(stderr, "Error mounting volume: invalid path prefix specified.\n");
        }
        else
        {
            fprintf(stderr, "Error number %d mounting volume.\n", red_errno);
        }

        mountfail = true;
        ret = -1;
    }

    if(ret == 0)
    {
        char    asInputDir[HOST_PATH_MAX];
        size_t  inDirLen = strlen(pszInDir);

        if(inDirLen >= HOST_PATH_MAX)
        {
            /*  Not expected; the length of pszInDir should have already been checked.
            */
            fprintf(stderr, "Error: path too long: %s\n", pszInDir);
            REDERROR();
            ret = -1;
        }
        else
        {
            strcpy(asInputDir, pszInDir);

            /*  Get rid of any ending path separators.
            */
            while(     asInputDir[inDirLen - 1] == '/'
          #ifdef _WIN32
                    || asInputDir[inDirLen - 1] == '\\'
          #endif
                 )
            {
                asInputDir[inDirLen - 1] = '\0';
                inDirLen--;
            }

            ret = IbPosixCopyDirRecursive(pszVolName, asInputDir);
        }
    }

    if(ret == 0)
    {
        ret = red_transact(pszVolName);
        if(ret != 0)
        {
            fprintf(stderr, "Unexpected error number %d in red_transact.\n", -ret);
            ret = -1;
        }
    }

    if(!mountfail)
    {
        if(red_umount(pszVolName) == -1)
        {
            fprintf(stderr, "Error number %d unmounting volume.\n", red_errno);
            ret = -1;
        }
    }

    return ret;
}


/** @brief  Create a directory using the Reliance Edge POSIX API.
*/
int IbPosixCreateDir(
    const char *pszVolName,
    const char *pszFullPath,
    const char *pszBasePath)
{
    int         ret = 0;
    char        asOutPath[HOST_PATH_MAX];

    ret = IbConvertPath(pszVolName, pszFullPath, pszBasePath, asOutPath);

    if(ret == 0)
    {
        if(red_mkdir(asOutPath) != 0)
        {
            ret = -1;

            switch(red_errno)
            {
                case RED_EIO:
                    fprintf(stderr, "Disk I/O creating directory %s.\n", asOutPath);
                    break;
                case RED_ENOSPC:
                    fprintf(stderr, "Insufficient space on target volume.\n");
                    break;
                case RED_ENFILE:
                    fprintf(stderr, "Error: maximum number of files for volume %s exceeded.\n", pszVolName);
                    break;
                case RED_ENAMETOOLONG:
                    fprintf(stderr, "Error: configured maximum file name length (%d) exceeded by directory %s.\n", REDCONF_NAME_MAX, pszFullPath);
                    break;
                default:
                    /*  Other errors not expected.
                    */
                    REDERROR();
                    break;
            }
        }
    }

    return ret;
}


/** @brief Takes a host system file path and converts it to a compatible path
           for the Reliance Edge POSIX API

    @param pszVolName   The Reliance Edge volume name
    @param pszFullPath  The full host file path
    @param pszBasePath  A base path which will be removed from the back of
                        @p pszFullPath
    @param szOutPath    A char array pointer allocated at least HOST_PATH_MAX
                        chars at which to store the converted path

    @return An integer indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int IbConvertPath(
    const char *pszVolName,
    const char *pszFullPath,
    const char *pszBasePath,
    char       *szOutPath)
{
    int         ret = 0;
    const char *pszInPath = pszFullPath;
    size_t      volNameLen = strlen(pszVolName);
    size_t      index = 0;

    while((pszInPath[0] == pszBasePath[index]) && (pszInPath[0] != '\0'))
    {
        pszInPath++;
        index++;
    }

    /*  After skipping the base path, the next char should be a path separator.
        Skip this too.
    */
    if(    (pszInPath[0] == '/')
      #ifdef _WIN32
        || (pszInPath[0] == '\\')
      #endif
      )
    {
        pszInPath++;
    }

    if((strlen(pszInPath) + 1 + strlen(pszVolName)) >= (HOST_PATH_MAX - 1))
    {
        fprintf(stderr, "Error: path name too long: %s\n", pszFullPath);
        ret = -1;
    }
    else
    {
        /*  Roundabout way of avoiding build warnings when REDCONF_ASSERTS
            is disabled.
        */
      #if REDCONF_ASSERTS == 1
        int len =
      #endif
        sprintf(szOutPath, "%s%c%s", pszVolName, REDCONF_PATH_SEPARATOR, pszInPath);

        REDASSERT(len >= (int) volNameLen);

        for(index = volNameLen + 1; szOutPath[index] != '\0'; index++)
        {
            if(    (szOutPath[index] == '/')
              #ifdef _WIN32
                || (szOutPath[index] == '\\')
              #endif
              )
            {
                szOutPath[index] = REDCONF_PATH_SEPARATOR;
            }
            else if (szOutPath[index] == REDCONF_PATH_SEPARATOR)
            {
                fprintf(stderr, "Error: unexpected target path separator character in path %s\n", pszInPath);
                ret = -1;
                break;
            }
        }
    }

    return ret;
}

#endif

