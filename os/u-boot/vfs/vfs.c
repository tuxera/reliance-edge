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
    @brief Implements file system interface for u-boot.
*/


#include <fs.h>
#include <blk.h>


#include <redfs.h>
#include <redvolume.h>
#include <redposix.h>
#include <redosbdev.h>


#if REDCONF_READ_ONLY != 1
#error REDCONF_READ_ONLY expected to be 1
#endif
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
#define REDFS_DISK      0
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
    struct blk_desc *fs_dev_desc,
    disk_partition_t *fs_partition)
{
    int32_t result;
    int ret = 0;


    result = RedOsBDevConfig2(REDFS_DISK, fs_dev_desc, fs_partition);
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

    This function is called by u-boot fs interface when uninitialing the file
    system.

    Upon successful return, the file system is unmounted and uninitialized.

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
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

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
*/
int redfs_ls(
    const char * pszPath)
{
    REDDIR * pDir;
    int ret = 0;


    pDir = red_opendir(pszPath);
    if(!pDir)
    {
        ret = -1;
    }
    else
    {
        REDDIRENT * pDirEnt;

        red_errno = 0; //todo:remove
        pDirEnt = red_readdir(pDir);
        while(pDirEnt)
        {
            if(RED_S_ISDIR(pDirEnt->d_stat.st_mode))
            {
                printf("%10s  %s\n", "<DIR>", pDirEnt->d_name);
            }
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

    This function is called by u-boot fs interface to determine if a file
    or directory exists at the specified path.

    @param pszPath  Path to validate.

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
*/
int redfs_exists(
    const char * pszPath)
{
    int32_t fd;
    int ret = 0;


    fd = red_open(pszPath, RED_O_RDONLY);
    if(fd < 0)
    {
        ret = -1;
    }
    else
    {
        red_close(fd);
    }


    return ret;
}


/** @brief Size of file or directory

    This function is called by u-boot fs interface to determine the size of
    a file or directory.

    @param pszPath  Path to size.

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
*/
int redfs_size(
    const char * pszPath,
    loff_t * pSize)
{
    int32_t fd;
    int ret = 0;


    fd = red_open(pszPath, RED_O_RDONLY);
    if(fd < 0)
    {
        ret = -1;
    }
    else
    {
        REDSTAT Stat;
        int32_t result;


        result = red_fstat(fd, &Stat);
        if(result != 0)
        {
            ret = -1;
        }
        else
        {
            *pSize = (loff_t)Stat.st_size;
        }


        red_close(fd);
    }


    return ret;
}


/** @brief Read from file

    This function is called by u-boot fs interface to read a number of bytes
    from a file.

    @param pszPath  Path to read.
    @param pBuffer  Location to store the read data.
    @param offset   Byte offset to begin reading
    @param len      Number of bytes to read. 0 means whole file.
    @param pActual  Populated with the number of bytes read.

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
*/
int redfs_read_file(
    const char * pszPath,
    void * pBuffer,
    loff_t offset,
    loff_t len,
    loff_t * pActual)
{
    int ret = 0;


    if((offset < 0) || (len < 0) || (len > UINT32_MAX) || !pActual)
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
                /*  In U-Boot, len == 0 means read the whole file.
                */
                if(len == 0)
                {
                    len = INT32_MAX;
                }

                *pActual = red_read(fd, pBuffer, len);
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

    @param pszPath  Path to write.
    @param pBuffer  Location of the write data.
    @param offset   Byte offset to begin writing
    @param len      Number of bytes to write.
    @param pActual  Populated with the number of bytes written.

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
*/
int redfs_write_file(
    const char * pszPath,
    void * pBuffer,
    loff_t offset,
    loff_t len,
    loff_t * pActual)
{
    int ret = 0;


    if((offset < 0) || (len < 0) || (len > UINT32_MAX) || !pActual)
    {
        ret = -1;
    }
    else
    {
        int32_t fd;


        fd = red_open(pszPath, RED_O_WRONLY);
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
                *pActual = red_write(fd, pBuffer, (uint32_t)len);
            }
            red_close(fd);
        }
    }


    return ret;
}


#endif /* #if REDCONF_READ_ONLY == 0 */


/*  readdir added in v2017.11 release; not in v2017.09 or earlier

    TODO: condition this code to be included for v2017.11 and later
*/
#if 0


/*  Abstract directory entry type that includes a directory entry container
*/
typedef struct {
    REDDIR * pDirectory;
    struct fs_dirent sEntry;
} REDFS_DIR;


/** @brief Open a directory

    This function is called by u-boot fs interface to open a directory in
    preparation for a readddir and closedir.

    @param pszPath  Path of directory to open.
    @param dirsp    Populated with a open directory handle

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
*/
int redfs_opendir(
    const char * pszPath,
    struct fs_dir_stream **dirsp)
{
    REDFS_DIR * pDir;
    int ret = 0;


    pDir = calloc(1, sizeof(*pDir));
    if(!pDir)
    {
        ret = -1;
    }
    else
    {
        pDir->pDirectory = red_opendir(pszPath);
        if(!pDir->pDirectory)
        {
            free(pDir);
            ret = -1;
        }
        else
        {
            *dirsp = (struct fs_dir_stream *)pDir;
        }
    }


    return ret;
}


/** @brief Read from directory

    This function is called by u-boot fs interface to read from a directory
    associated with an opendir.

    @param dirs  Path of directory to open.
    @param dirsp    Populated with a open directory handle

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
*/
int redfs_readdir(
    struct fs_dir_stream *dirs,
    struct fs_dirent **dentp)
{
    REDFS_DIR * pDir = (REDFS_DIR *)dirs;
    REDDIRENT * pDirEnt;
    int ret = 0;


    pDirEnt = red_readdir(pDir->pDirectory);
    if(!pDirEnt)
    {
        *dentp = NULL;
        ret = -1;
    }
    else
    {
        memset(&pDir->sEntry, 0, sizeof(pDir->sEntry));
        strncpy(pDir->sEntry.name, sizeof(pDir->sEntry.name), pDir->pDirectory->dirent.d_name);
        pDir->sEntry.name[254] = 0;
        if(RED_S_ISDIR(pDir->pDirectory->dirent.d_stat.st_mode))
        {
            pDir->sEntry.type = FS_DT_DIR;
        }
        else
        {
            pDir->sEntry.type = FS_DT_REG;
        }
        *dentp = &pDir->sEntry;
    }


    return ret;
}


/** @brief Close directory

    This function is called by u-boot fs interface to close a directory
    associated with an opendir.

    @param dirs  Path of directory to open.

    @return A value indicating the operation result.

    @retval 0       Operation was successful.
    @retval -1      An error occurred.
*/
void redfs_closedir(
    struct fs_dir_stream *dirs)
{
    REDFS_DIR * pDir = (REDFS_DIR *)dirs;


    if(pDir)
    {
        red_closedir(pDir->pDirectory);
        free(pDir);
    }
}


#endif  /* #if 0 */

