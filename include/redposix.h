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
    @brief Interface for the Reliance Edge POSIX-like API.

    The POSIX-like file system API is the primary file system API for
    Reliance Edge, which supports the full functionality of the file system.
    This API aims to be compatible with POSIX where reasonable, but it is
    simplified considerably to meet the needs of resource-constrained embedded
    systems.  The API has also been extended to provide access to the unique
    features of Reliance Edge, and to cover areas (like mountins and formatting)
    which do not have APIs in the POSIX specification.
*/
#ifndef REDPOSIX_H
#define REDPOSIX_H

/*  This header is intended for application use; some applications are written
    in C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

#include <redconf.h>

#if REDCONF_API_POSIX == 1

#include <redtypes.h>
#include "redapimacs.h"
#include "rederrno.h"
#include "redstat.h"
#include "redformat.h"

/** Open for reading only. */
#define RED_O_RDONLY    0x00000001U

/** Open for writing only. */
#define RED_O_WRONLY    0x00000002U

/** Open for reading and writing. */
#define RED_O_RDWR      0x00000004U

/** File offset for all writes is end-of-file. */
#define RED_O_APPEND    0x00000008U

/** Create the file. */
#define RED_O_CREAT     0x00000010U

/** Error if path already exists. */
#define RED_O_EXCL      0x00000020U

/** Truncate file to size zero. */
#define RED_O_TRUNC     0x00000040U

/** If last path component is a symbolic link, return #RED_ELOOP. */
#define RED_O_NOFOLLOW  0x00000080U

#if REDCONF_API_POSIX_SYMLINK == 1
/** Expect last path component to be a symbolic link (POSIX extension). */
#define RED_O_SYMLINK   0x00000100U
#endif


#if REDCONF_API_POSIX_CWD == 1
/** Pseudo file descriptor representing the current working directory.

    When used as the file descriptor parameter with the `red_*at()` APIs, this
    causes the corresponding relative path to be parsed from the current working
    directory.

    @note This macro only exists when #REDCONF_API_POSIX_CWD is enabled.  When
          #REDCONF_API_POSIX_CWD is false, #RED_AT_FDABS can be used instead of
          this macro.  Alternatively, #RED_AT_FDNONE can be used to do the
          "right thing" regardless of whether #REDCONF_API_POSIX_CWD is enabled.

    @note This value is _only_ understood by the `red_*at()` APIs.  Use with any
          other file descriptor API will result in a #RED_EBADF error.
*/
#define RED_AT_FDCWD    (-100) /* Any invalid fd value would work; Linux uses -100. */
#endif

/** Pseudo file descriptor indicating an absolute path.

    When used as the file descriptor parameter with the `red_*at()` APIs, this
    forces the corresponding path argument to be parsed as an absolute path.

    This macro has no POSIX equivalent.  It is provided as a POSIX extension.
    In POSIX, the `*at()` APIs can be supplied with `AT_CWD` as the file
    descriptor in order to be equivalent to the non-`*at()` versions: e.g.,
    `unlinkat(AT_CWD, path, 0)` is equivalent to `unlink(path)`.  In Reliance
    Edge, the CWD feature is optional, enabled by #REDCONF_API_POSIX_CWD.  It
    would be confusing to allow #RED_AT_FDCWD when CWDs are disabled.  Instead,
    when CWDs are disabled, this macro may be used to make the red_\*at() APIs
    equivalent to the non-`*at()` versions.

    @note Applications are recommended to use #RED_AT_FDNONE rather than using
          this macro directly.  Using #RED_AT_FDNONE will do the "right thing"
          regardless of whether #REDCONF_API_POSIX_CWD is enabled, so fewer
          changes will be required to the application code if the Reliance Edge
          configuration is changed.

    @note This value is _only_ understood by the `red_*at()` APIs.  Use with any
          other file descriptor API will result in a #RED_EBADF error.
*/
#define RED_AT_FDABS    (-101) /* Any invalid fd value would work. */

/** Pseudo file descriptor indicating that only the path should be used.

    When used as the file descriptor parameter with the `red_*at()` APIs, this
    indicates that only the corresponding path argument should be used.  The
    interpretation of the path argument depends on the Reliance Edge
    configuration:

    1. If #REDCONF_API_POSIX_CWD is false, the path is parsed as an absolute
       path.
    2. If #REDCONF_API_POSIX_CWD is true, the path is parsed as an absolute path
       if it looks like an absolute path, otherwise it is parsed relative to the
       current working directory.

    This macro has no POSIX equivalent.  It is provided as a POSIX extension.
    In cases where the `red_*at()` APIs are being used without a real file
    descriptor, this macro is convenient for doing the "right thing" regardless
    of whether #REDCONF_API_POSIX_CWD is enabled.

    @note This value is _only_ understood by the `red_*at()` APIs.  Use with any
          other file descriptor API will result in a #RED_EBADF error.
*/
#if REDCONF_API_POSIX_CWD == 1
#define RED_AT_FDNONE   RED_AT_FDCWD
#else
#define RED_AT_FDNONE   RED_AT_FDABS
#endif


#if REDCONF_API_POSIX_RMDIR == 1
/** red_unlinkat() flag which tells it to expect a directory. */
#define RED_AT_REMOVEDIR 0x1U
#endif

/** @brief If the final path component names a symbolic link, do not follow it.

    This flag is supported by the following APIs:
    - red_fchmodat()
    - red_fchownat()
    - red_utimesat()
    - red_fstatat()

    This flag only applies to the final path component.  Symbolic links in path
    prefix components are still followed when this flag is used.

    If #REDCONF_API_POSIX_SYMLINK is false, symbolic links do not exist, and
    this flag has no effect.  If #REDOSCONF_SYMLINK_FOLLOW is false, symbolic
    links are never followed, and this flag has no effect.
*/
#define RED_AT_SYMLINK_NOFOLLOW 0x2U

/** @brief If the final path component names a symbolic link, follow it.

    This flag is supported by red_linkat().

    This flag only applies to the final path component of the first path
    parameter of red_linkat() (@p pszPath).  Symbolic links in both path prefix
    components are still followed even if this flag is not specified.

    If #REDCONF_API_POSIX_SYMLINK is false, symbolic links do not exist, and
    this flag has no effect.  If #REDOSCONF_SYMLINK_FOLLOW is false, symbolic
    links are never followed, and this flag has no effect.
*/
#define RED_AT_SYMLINK_FOLLOW 0x4U


/** @brief Tell red_getdirpath() to exclude the volume name from the path.
*/
#define RED_GETDIRPATH_NOVOLUME 0x1U


/** @brief Last file system error (errno).

    Under normal circumstances, each task using the file system has an
    independent `red_errno` value.  Applications do not need to worry about
    one task obliterating an error value that another task needed to read.  The
    value is initially zero.  When one of the POSIX-like APIs return an
    indication of error, `red_errno` is set to an error value.

    In some circumstances, `red_errno` will be a global errno location which
    is shared by multiple tasks.  If the calling task is not registered as a
    file system user and all of the task slots are full, there can be no
    task-specific errno, so the global errno is used.  Likewise, if the file
    system driver is uninitialized, there are no registered file system users
    and `red_errno` always refers to the global errno.  Under these
    circumstances, multiple tasks manipulating `red_errno` could be
    problematic.

    Note that `red_errno` is usable as an lvalue; i.e., in addition to reading
    the error value, the error value can be set:

    ~~~{.c}
    red_errno = 0;
    ~~~
*/
#define red_errno (*red_errnoptr())


/** @brief Positions from which to seek within a file.
*/
typedef enum
{
    /*  0/1/2 are the traditional values for SET/CUR/END, respectively.  Prior
        to the release of Unix System V in 1983, the SEEK_* symbols did not
        exist and C programs hard-coded the 0/1/2 values with those meanings.
    */
    RED_SEEK_SET = 0,   /**< Set file offset to given offset. */
    RED_SEEK_CUR = 1,   /**< Set file offset to current offset plus signed offset. */
    RED_SEEK_END = 2    /**< Set file offset to EOF plus signed offset. */
} REDWHENCE;


#if REDCONF_API_POSIX_READDIR == 1
/** @brief Opaque directory handle.
*/
typedef struct sREDHANDLE REDDIR;


/** @brief Directory entry information.
*/
typedef struct
{
    uint32_t    d_ino;  /**< File serial number (inode number). */
    char        d_name[REDCONF_NAME_MAX+1U];    /**< Name of entry. */
    REDSTAT     d_stat; /**< File information (POSIX extension). */
} REDDIRENT;
#endif


int32_t red_init(void);
int32_t red_uninit(void);
int32_t red_mount(const char *pszVolume);
int32_t red_mount2(const char *pszVolume, uint32_t ulFlags);
int32_t red_umount(const char *pszVolume);
int32_t red_umount2(const char *pszVolume, uint32_t ulFlags);
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FORMAT == 1)
int32_t red_format(const char *pszVolume);
int32_t red_format2(const char *pszVolume, const REDFMTOPT *pOptions);
#endif
#if REDCONF_READ_ONLY == 0
int32_t red_transact(const char *pszVolume);
int32_t red_rollback(const char *pszVolume);
#endif
#if REDCONF_READ_ONLY == 0
int32_t red_settransmask(const char *pszVolume, uint32_t ulEventMask);
#endif
int32_t red_gettransmask(const char *pszVolume, uint32_t *pulEventMask);
int32_t red_statvfs(const char *pszVolume, REDSTATFS *pStatvfs);
#if DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1)
int32_t red_freeorphans(const char *pszVolume, uint32_t ulMaxDeletions);
#endif
#if REDCONF_READ_ONLY == 0
int32_t red_sync(void);
#endif
int32_t red_open(const char *pszPath, uint32_t ulOpenMode);
#if (REDCONF_READ_ONLY == 0) && (REDCONF_POSIX_OWNER_PERM == 1)
int32_t red_open2(const char *pszPath, uint32_t ulOpenFlags, uint16_t uMode);
#endif
int32_t red_openat(int32_t iDirFildes, const char *pszPath, uint32_t ulOpenFlags, uint16_t uMode);
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_SYMLINK == 1)
int32_t red_symlink(const char *pszPath, const char *pszSymlink);
#endif
#if REDCONF_API_POSIX_SYMLINK == 1
int32_t red_readlink(const char *pszSymlink, char *pszBuffer, uint32_t ulBufferSize);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_UNLINK == 1)
int32_t red_unlink(const char *pszPath);
#endif
#if (REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1))
int32_t red_unlinkat(int32_t iDirFildes, const char *pszPath, uint32_t ulFlags);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_POSIX_OWNER_PERM == 1)
int32_t red_chmod(const char *pszPath, uint16_t uMode);
int32_t red_fchmodat(int32_t iDirFildes, const char *pszPath, uint16_t uMode, uint32_t ulFlags);
int32_t red_fchmod(int32_t iFildes, uint16_t uMode);
int32_t red_chown(const char *pszPath, uint32_t ulUID, uint32_t ulGID);
int32_t red_fchownat(int32_t iDirFildes, const char *pszPath, uint32_t ulUID, uint32_t ulGID, uint32_t ulFlags);
int32_t red_fchown(int32_t iFildes, uint32_t ulUID, uint32_t ulGID);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_INODE_TIMESTAMPS == 1)
int32_t red_utimes(const char *pszPath, const uint32_t *pulTimes);
int32_t red_utimesat(int32_t iDirFildes, const char *pszPath, const uint32_t *pulTimes, uint32_t ulFlags);
int32_t red_futimes(int32_t iFildes, const uint32_t *pulTimes);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_MKDIR == 1)
int32_t red_mkdir(const char *pszPath);
#if REDCONF_POSIX_OWNER_PERM == 1
int32_t red_mkdir2(const char *pszPath, uint16_t uMode);
#endif
int32_t red_mkdirat(int32_t iDirFildes, const char *pszPath, uint16_t uMode);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_RMDIR == 1)
int32_t red_rmdir(const char *pszPath);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_RENAME == 1)
int32_t red_rename(const char *pszOldPath, const char *pszNewPath);
int32_t red_renameat(int32_t iOldDirFildes, const char *pszOldPath, int32_t iNewDirFildes, const char *pszNewPath);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_LINK == 1)
int32_t red_link(const char *pszPath, const char *pszHardLink);
int32_t red_linkat(int32_t iDirFildes, const char *pszPath, int32_t iHardLinkDirFildes, const char *pszHardLink, uint32_t ulFlags);
#endif
int32_t red_stat(const char *pszPath, REDSTAT *pStat);
int32_t red_fstatat(int32_t iDirFildes, const char *pszPath, REDSTAT *pStat, uint32_t ulFlags);
int32_t red_fstat(int32_t iFildes, REDSTAT *pStat);
int32_t red_close(int32_t iFildes);
int32_t red_read(int32_t iFildes, void *pBuffer, uint32_t ulLength);
int32_t red_pread(int32_t iFildes, void *pBuffer, uint32_t ulLength, uint64_t ullOffset);
#if REDCONF_READ_ONLY == 0
int32_t red_write(int32_t iFildes, const void *pBuffer, uint32_t ulLength);
int32_t red_pwrite(int32_t iFildes, const void *pBuffer, uint32_t ulLength, uint64_t ullOffset);
int32_t red_fsync(int32_t iFildes);
#endif
int64_t red_lseek(int32_t iFildes, int64_t llOffset, REDWHENCE whence);
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FTRUNCATE == 1)
int32_t red_ftruncate(int32_t iFildes, uint64_t ullSize);
#endif
#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FRESERVE == 1)
int32_t red_freserve(int32_t iFildes, uint64_t ullSize);
#endif
#if REDCONF_API_POSIX_READDIR == 1
REDDIR *red_opendir(const char *pszPath);
REDDIR *red_fdopendir(int32_t iFildes);
REDDIRENT *red_readdir(REDDIR *pDirStream);
void red_rewinddir(REDDIR *pDirStream);
void red_seekdir(REDDIR *pDirStream, uint32_t ulPosition);
uint32_t red_telldir(REDDIR *pDirStream);
int32_t red_closedir(REDDIR *pDirStream);
#endif
#if REDCONF_API_POSIX_CWD == 1
int32_t red_chdir(const char *pszPath);
char *red_getcwd(char *pszBuffer, uint32_t ulBufferSize);
#endif
char *red_getdirpath(int32_t iFildes, char *pszBuffer, uint32_t ulBufferSize, uint32_t ulFlags);
REDSTATUS *red_errnoptr(void);

#endif /* REDCONF_API_POSIX */


#ifdef __cplusplus
}
#endif

#endif
