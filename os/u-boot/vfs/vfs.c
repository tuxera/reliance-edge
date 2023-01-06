/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2023 Tuxera US Inc.
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
    @brief Implements file system interface for u-boot.
*/


#include <fs.h>
#include <blk.h>
#include <part.h>
#include <version.h>


#include <rederrno.h>
#include <redfs.h>
#include <redvolume.h>
#include <redposix.h>
#include <redosbdev.h>

#include <stdio.h>
#include <stdlib.h>


#if REDCONF_API_POSIX != 1
#error REDCONF_API_POSIX expected to be 1
#endif
#if REDCONF_TASK_COUNT != 1
#error REDCONF_TASK_COUNT expected to be 1
#endif


/*  This is the entry in the disk table that will be used for all disk
    access.
*/
#ifndef REDFS_DISK
#define REDFS_DISK  0U
#endif


/*  Verify disk entry is within bounds
*/
#if REDFS_DISK >= REDCONF_VOLUME_COUNT
#error Invalid disk.  REDFS_DISK must be less than REDCONF_VOLUME_COUNT.
#endif


/** @brief Attempt to mount edge on a disk.

    This function is called by u-boot fs interface when determining which
    file system recognizes the on-disk format of a disk.

    Upon successful return, the file system is initialized and mounted and
    ready to service read, exists, size, and ls requests.

    @param fs_dev_desc  Block device handle.
    @param fs_partition Partition information.

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
*/
int redfs_probe(
    struct blk_desc        *fs_dev_desc,
    struct disk_partition  *fs_partition)
{
    UBOOT_DEV   devctx;
    int32_t     result;
    int         ret = 0;


    devctx.block_dev = fs_dev_desc;
    devctx.fs_partition = fs_partition;
    result = RedOsBDevConfig(REDFS_DISK, &devctx);
    if(result != 0)
    {
        ret = -1;
    }
    else
    {
        result = red_init();
        if(result != 0)
        {
            ret = -1;
        }
        else
        {
            result = red_mount(gaRedVolConf[REDFS_DISK].pszPathPrefix);
            if((result != 0) && (red_errno != RED_EBUSY))
            {
                red_uninit();
                ret = -1;
            }
        }
    }

    return ret;
}


/** @brief Unmount and uninitialize.

    This function is called by u-boot fs interface when uninitializing the file
    system.

    Upon return, the file system is unmounted and uninitialized.
*/
void redfs_close(void)
{
    red_umount(gaRedVolConf[REDFS_DISK].pszPathPrefix);
    red_uninit();
}


/** @brief List files and directories

    This function is called by u-boot fs interface to list files and
    directories for the specified path.

    @param pszPath  Path to perform listing.

    @return A value indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int redfs_ls(
    const char *pszPath)
{
    REDDIR *pDir;
    int     ret = 0;


    pDir = red_opendir(pszPath);
    if(!pDir)
    {
        ret = -1;
    }
    else
    {
        REDDIRENT  *pDirEnt;

        pDirEnt = red_readdir(pDir);
        while(pDirEnt)
        {
            if(RED_S_ISDIR(pDirEnt->d_stat.st_mode))
            {
                printf("%10s  %s\n", "<DIR>", pDirEnt->d_name);
            }
          #if REDCONF_API_POSIX_SYMLINK == 1
            else if(RED_S_ISLNK(pDirEnt->d_stat.st_mode))
            {
                printf("%10s  %s\n", "<LNK>", pDirEnt->d_name);
            }
         #endif
            else
            {
                printf("%10llu  %s\n", pDirEnt->d_stat.st_size, pDirEnt->d_name);
            }
            pDirEnt = red_readdir(pDir);
        }

        red_closedir(pDir);
    }

    return ret;
}


/** @brief Path exists

    This function is called by the U-Boot FS interface to determine if a file
    or directory exists at the specified path.

    @param pszPath  Path to validate.

    @return A value indicating the operation result.

    @retval 0       Exists
    @retval -1      Does not exist
*/
int redfs_exists(
    const char *pszPath)
{
    int32_t fd;
    int     ret;


    fd = red_open(pszPath, RED_O_RDONLY);
    if(fd < 0)
    {
        ret = -1;
    }
    else
    {
        red_close(fd);
        ret = 0;
    }

    return ret;
}


/** @brief Size of file or directory

    This function is called by u-boot fs interface to determine the size of
    a file, directory or symlink.

    @param pszPath  Path to size.
    @param pSize    Populated with the size of pszPath.

    @return A value indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int redfs_size(
    const char *pszPath,
    loff_t     *pSize)
{
    int     ret = 0;
    int32_t fd;

    fd = red_open(pszPath, RED_O_RDONLY);
    if(fd < 0)
    {
        ret = -1;
    }
    else
    {
        REDSTAT sb;

        if(red_stat(pszPath, &sb) != 0)
        {
            ret = -1;
        }
        else
        {
            *pSize = sb.st_size;
        }
    }

    red_close(fd);

    return ret;
}


/** @brief Read from file

    This function is called by u-boot fs interface to read a number of bytes
    from a file.

    @param pszPath  Path to the file to read from.
    @param pBuffer  Location to store the read data.
    @param offset   Byte offset to begin reading.
    @param len      Number of bytes to read. 0 means whole file.
    @param pActual  Populated with the number of bytes read.

    @return A value indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int redfs_read_file(
    const char *pszPath,
    void       *pBuffer,
    loff_t      offset,
    loff_t      len,
    loff_t     *pActual)
{
    int         ret = 0;


    if((offset < 0) || (len < 0) || (len > UINT32_MAX) || (pActual == NULL))
    {
        ret = -1;
    }
    else
    {
        int32_t fd;


        fd = red_open(pszPath, RED_O_RDONLY);
        if(fd < 0)
        {
            printf("** Cannot open file %s **\n", pszPath);
            ret = -1;
        }
        else
        {
            int64_t result;

            result = red_lseek(fd, offset, RED_SEEK_SET);
            if(result != offset)
            {
                ret = -1;
            }
            else
            {
                if(len == 0)
                {
                    REDSTAT sb;

                    ret = red_fstat(fd, &sb);
                    if(sb.st_size > INT32_MAX)
                    {
                        red_close(fd);
                        printf("REDFS: ** File, %s, is too large to read **\n",pszPath);
                        return -1;
                    }
                    else
                    {
                        len = sb.st_size;
                    }
                }

                red_errno = 0;
                *pActual = red_read(fd, pBuffer, len);

                if((*pActual != len) && (*pActual > 0))
                {
                    printf("REDFS: ** Unable to read full size %lld, %lld read of %s **\n", len, *pActual, pszPath);
                    ret = -1;
                }
                else if(red_errno != 0)
                {
                    printf("REDFS: ** Unable to read file %s **\n", pszPath);
                    ret = -1;
                }
            }

            red_close(fd);
        }
    }

    return ret;
}


#if REDCONF_READ_ONLY == 0
/** @brief Write to file

    This function is called by u-boot fs interface to write a number of bytes
    to a file.

    @param pszPath  Path to the file to write into.
    @param pBuffer  Location of the write data.
    @param offset   Byte offset to begin writing
    @param len      Number of bytes to write.
    @param pActual  Populated with the number of bytes written.

    @return A value indicating the operation result.

    @retval 0   Operation was successful.
    @retval -1  An error occurred.
*/
int redfs_write_file(
    const char *pszPath,
    void       *pcBuffer,
    loff_t      offset,
    loff_t      len,
    loff_t     *pActual)
{
    int         ret = 0;


    if((offset < 0) || (len < 0) || (len > UINT32_MAX) || (pActual == 0))
    {
        ret = -1;
    }
    else
    {
        int32_t fd;


        fd = red_open(pszPath, RED_O_WRONLY | RED_O_CREAT);
        if(fd < 0)
        {
            printf("REDFS: ** Cannot open file %s **\n", pszPath);
            ret = -1;
        }
        else
        {
            int64_t result;

            result = red_lseek(fd, offset, RED_SEEK_SET);
            if(result != offset)
            {
                ret = -1;
            }
            else
            {
                red_errno = 0;
                *pActual = red_write(fd, pcBuffer, (uint32_t)len);

                if((*pActual != len) && (*pActual > 0))
                {
                    printf("REDFS: ** Unable to write full size %lld, %lld written to %s **\n", len, *pActual, pszPath);
                    ret = -1;
                }
                else if(red_errno != 0)
                {
                    printf("REDFS: ** Unable to write file %s **\n", pszPath);
                    ret = -1;
                }
            }

            red_close(fd);
        }
    }

    return ret;
}

#if REDCONF_API_POSIX_MKDIR == 1
/** @brief Create a new directory

    This function is called by u-boot fs interface to make a new directory.

    @param pszPath  Path of directory to create.

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.

*/
int redfs_mkdir(
    const char *pszPath)
{
    return red_mkdir(pszPath);
}
#endif /* REDCONF_API_POSIX_MKDIR == 1 */
#endif /* REDCONF_READ_ONLY == 0 */


/*  readdir added in v2017.11 release; not in v2017.09 or earlier
*/
#if (U_BOOT_VERSION_NUM > 2017) || ((U_BOOT_VERSION_NUM == 2017) && (U_BOOT_VERSION_NUM_PATCH >= 11))
#if REDCONF_API_POSIX_READDIR == 1

/*  Abstract directory entry type that includes a directory entry container
*/
typedef struct {
    REDDIR             *pDirectory;
    struct fs_dirent    sEntry;
} REDFS_DIR;


/** @brief Open a directory.

    This function is called by the U-Boot FS interface to open a directory in
    preparation for a readddir and closedir.

    @param pszPath  Path with the directory to open.
    @param dirsp    Populated with an open directory handle.

    @return A value indicating the operation result.

    @return Zero on success or a negative errno value on error.
*/
int redfs_opendir(
    const char             *pszPath,
    struct fs_dir_stream  **dirsp)
{
    REDFS_DIR  *pDir;
    int         ret = 0;


    pDir = calloc(1, sizeof(*pDir));
    if(pDir == NULL)
    {
        ret = -RED_ENOMEM;
    }
    else
    {
        pDir->pDirectory = red_opendir(pszPath);
        if(pDir->pDirectory == NULL)
        {
            free(pDir);
            ret = -red_errno; /* U-Boot and Reliance Edge both use Linux errno numbers */
        }
        else
        {
            *dirsp = (struct fs_dir_stream *)pDir;
        }
    }


    return ret;
}


/** @brief Read from directory.

    This function is called by the U-Boot FS interface to read from a directory
    associated with an opendir.

    @param dirs     Handle for the directory to read from.
    @param dentp    Populated with an open directory handle.

    @return A value indicating the operation result.

    @return Zero on success or a negative errno value on error.
*/
int redfs_readdir(
    struct fs_dir_stream    *dirs,
    struct fs_dirent       **dentp)
{
    REDFS_DIR  *pDir = (REDFS_DIR *)dirs;
    REDDIRENT  *pDirEnt;
    int         ret = 0;


    red_errno = 0;
    pDirEnt = red_readdir(pDir->pDirectory);
    if(pDirEnt == NULL)
    {
        *dentp = NULL;

        if(red_errno != 0)
        {
            ret = -red_errno; /* U-Boot and Reliance Edge both use Linux errno numbers */
        }
    }
    else
    {
        memset(&pDir->sEntry, 0, sizeof(pDir->sEntry));

        /*  Copy name while ensuring null-termination if the file name is too
            long to fit into the name buffer and is truncated.
        */
        strncpy(pDir->sEntry.name, pDirEnt->d_name, sizeof(pDir->sEntry.name));
        pDir->sEntry.name[sizeof(pDir->sEntry.name) - 1U] = 0;

        if(RED_S_ISDIR(pDirEnt->d_stat.st_mode))
        {
            pDir->sEntry.type = FS_DT_DIR;
        }
      #if REDCONF_API_POSIX_SYMLINK == 1
        else if(RED_S_ISLNK(pDirEnt->d_stat.st_mode))
        {
            pDir->sEntry.type = FS_DT_LNK;
        }
      #endif
        else
        {
            pDir->sEntry.type = FS_DT_REG;
        }

        pDir->sEntry.size = (loff_t)pDirEnt->d_stat.st_size;

        *dentp = &pDir->sEntry;
    }


    return ret;
}


/** @brief Close directory

    This function is called by u-boot fs interface to close a directory
    associated with an opendir.

    @param dirs  Path of directory to open.
*/
void redfs_closedir(
    struct fs_dir_stream *dirs)
{
    REDFS_DIR  *pDir = (REDFS_DIR *)dirs;


    if(pDir != NULL)
    {
        red_closedir(pDir->pDirectory);
        free(pDir);
    }
}
#endif /* REDCONF_API_POSIX_READDIR == 1 */
#endif /* (U_BOOT_VERSION_NUM > 2017) || ((U_BOOT_VERSION_NUM == 2017) && (U_BOOT_VERSION_NUM_PATCH >= 11)) */


#if (U_BOOT_VERSION_NUM > 2018) || ((U_BOOT_VERSION_NUM == 2018) && (U_BOOT_VERSION_NUM_PATCH >= 11))
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_UNLINK == 1)
/** @brief Delete a file or directory.

    This function is called by u-boot fs interface to close a file or directory.

    @param pszPath  Path of file or directory to delete.

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
*/
int redfs_unlink(
    const char *pszPath)
{
    return red_unlink(pszPath);
}
#endif /* (REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1)) */
#endif /* (U_BOOT_VERSION_NUM > 2018) || ((U_BOOT_VERSION_NUM == 2018) && (U_BOOT_VERSION_NUM_PATCH >= 11)) */


#if (U_BOOT_VERSION_NUM > 2019) || ((U_BOOT_VERSION_NUM == 2019) && (U_BOOT_VERSION_NUM_PATCH >= 07))
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_SYMLINK == 1)
/** @brief Create a Symlink.

    This function is called by u-boot fs interface to create a hardlink.

    @param pszPath      The target for the symbolic link; i.e., the path that
                        the symbolic link will point at. This path will be
                        stored verbatim; it will not be parsed in any way.
    @param pszSymLink   The path to the symbolic link to create.

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
*/
int redfs_symlink(
    const char *pszPath,
    const char *pszSymLink)
{
    return red_symlink(pszPath, pszSymLink);
}

#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_SYMLINK == 1) */
#endif /* (U_BOOT_VERSION_NUM > 2019) || ((U_BOOT_VERSION_NUM == 2019) && (U_BOOT_VERSION_NUM_PATCH >= 07)) */

