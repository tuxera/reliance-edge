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
    @brief Defines macros and types for red_stat() and red_statvfs().
*/
#ifndef REDSTAT_H
#define REDSTAT_H


/** Mode bit for a regular file. */
#define RED_S_IFREG     0100000U

/** Mode bit for a directory. */
#define RED_S_IFDIR     040000U

/** Mode bit for a symbolic link. */
#define RED_S_IFLNK     0120000U

/** Mode type bit valid mask. */
#define RED_S_IFMT      (   RED_S_IFREG \
                          | ((REDCONF_API_POSIX == 1) ? RED_S_IFDIR : 0U) \
                          | (((REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_SYMLINK == 1)) ? RED_S_IFLNK : 0U))

/** Set-user-ID bit. */
#define RED_S_ISUID     04000U

/** Set-group-ID bit.

    Take a new file's group from parent directory.
*/
#define RED_S_ISGID     02000U

/** Sticky bit.

    When set on a directory, restricts ability to remove/rename within that
    directory.
*/
#define RED_S_ISVTX     01000U

/** Read permission, owner. */
#define RED_S_IRUSR     00400U

/** Write permission, owner. */
#define RED_S_IWUSR     00200U

/** Execute/search permission, owner. */
#define RED_S_IXUSR     00100U

/** Read, write, execute/search by owner. */
#define RED_S_IRWXU     (RED_S_IRUSR | RED_S_IWUSR | RED_S_IXUSR)

/** Read permission, group. */
#define RED_S_IRGRP     00040U

/** Write permission, group. */
#define RED_S_IWGRP     00020U

/** Execute/search permission, group. */
#define RED_S_IXGRP     00010U

/** Read, write, execute/search by group. */
#define RED_S_IRWXG     (RED_S_IRGRP | RED_S_IWGRP | RED_S_IXGRP)

/** Read permission, others. */
#define RED_S_IROTH     00004U

/** Write permission, others. */
#define RED_S_IWOTH     00002U

/** Execute/search permission, others. */
#define RED_S_IXOTH     00001U

/** Read, write, execute/search by others. */
#define RED_S_IRWXO     (RED_S_IROTH | RED_S_IWOTH | RED_S_IXOTH)

/** Read, write, execute/search by owner/group/others. */
#define RED_S_IRWXUGO   (RED_S_IRWXU | RED_S_IRWXG | RED_S_IRWXO)

/** Bits that can be set/cleared by chmod. */
#define RED_S_IALLUGO   (RED_S_ISUID | RED_S_ISGID | RED_S_ISVTX | RED_S_IRWXUGO)

/** Mode bit permission valid mask. */
#if (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1)
#define RED_S_IFVALID   (RED_S_IFMT | RED_S_IALLUGO)
#else
#define RED_S_IFVALID   RED_S_IFMT
#endif

/** Default permissions for a regular file. */
#define RED_S_IREG_DEFAULT   ((RED_S_IRUSR | RED_S_IWUSR | RED_S_IRGRP | RED_S_IROTH) & RED_S_IFVALID)

/** Default permissions for a directory. */
#define RED_S_IDIR_DEFAULT   ((RED_S_IRWXU | RED_S_IRGRP | RED_S_IXGRP | RED_S_IROTH | RED_S_IXOTH) & RED_S_IFVALID)

#if REDCONF_API_POSIX == 1
  /** @brief Test for a directory.
  */
  #define RED_S_ISDIR(m)  (((m) & RED_S_IFMT) == RED_S_IFDIR)
#endif

/** @brief Test for a regular file.
*/
#define RED_S_ISREG(m)  (((m) & RED_S_IFMT) == RED_S_IFREG)

#if (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_SYMLINK == 1)
  /** @brief Test for a symbolic link.
  */
  #define RED_S_ISLNK(m)  (((m) & RED_S_IFMT) == RED_S_IFLNK)
#endif


/** File system is read-only. */
#define RED_ST_RDONLY   0x00000001U

/** File system ignores suid and sgid bits. */
#define RED_ST_NOSUID   0x00000002U


/** @brief Status information on an inode.
*/
typedef struct
{
    uint8_t     st_dev;     /**< Volume number of volume containing file. */
    uint32_t    st_ino;     /**< File serial number (inode number). */
    uint16_t    st_mode;    /**< Mode of file. */
    uint16_t    st_nlink;   /**< Number of hard links to the file. */
  #if (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1)
    uint32_t    st_uid;     /**< User ID of owner. */
    uint32_t    st_gid;     /**< Group ID of owner. */
  #endif
    uint64_t    st_size;    /**< File size in bytes. */
  #if REDCONF_INODE_TIMESTAMPS == 1
    uint32_t    st_atime;   /**< Time of last access (seconds since 01-01-1970). */
    uint32_t    st_mtime;   /**< Time of last data modification (seconds since 01-01-1970). */
    uint32_t    st_ctime;   /**< Time of last status change (seconds since 01-01-1970). */
  #endif
  #if REDCONF_INODE_BLOCKS == 1
    uint32_t    st_blocks;  /**< Number of blocks allocated for this object. */
  #endif
} REDSTAT;


/** @brief Status information on a file system volume.
*/
typedef struct
{
    uint32_t    f_bsize;    /**< File system block size. */
  #if REDCONF_API_POSIX == 1
    uint32_t    f_frsize;   /**< Fundamental file system block size. */
  #endif
    uint32_t    f_blocks;   /**< Total number of blocks on file system in units of f_frsize. */
    uint32_t    f_bfree;    /**< Total number of free blocks. */
  #if REDCONF_API_POSIX == 1
    uint32_t    f_bavail;   /**< Number of free blocks available to non-privileged process. */
  #endif
    uint32_t    f_files;    /**< Total number of file serial numbers. */
  #if REDCONF_API_POSIX == 1
    uint32_t    f_ffree;    /**< Total number of free file serial numbers. */
    uint32_t    f_favail;   /**< Number of file serial numbers available to non-privileged process. */
    uint32_t    f_fsid;     /**< File system ID (useless, populated with zero). */
  #endif
    uint32_t    f_flag;     /**< Bit mask of f_flag values.  Includes read-only file system flag. */
  #if REDCONF_API_POSIX == 1
    uint32_t    f_namemax;  /**< Maximum filename length. */
  #endif
    uint64_t    f_maxfsize; /**< Maximum file size (POSIX extension). */
    uint32_t    f_dev;      /**< Volume number (POSIX extension). */
    uint32_t    f_diskver;  /**< On-disk layout version (POSIX extension).  Values defined in redver.h. */
} REDSTATFS;


#endif

