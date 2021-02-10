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
    @brief Implements a Reliance Edge FUSE (File System in User Space) port
           for Linux.
*/
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#ifdef st_atime
  #define POSIX_2008_STAT

  /*  These undefs are preset to remove the definitions for the fields of the
      same name in the POSIX time and stat code.  If these are not present, a
      very cryptic error message will occur if this is compiled on a Linux (or
      other POSIX-based) system.
  */
  #undef st_atime
  #undef st_mtime
  #undef st_ctime
#endif

#include <redfs.h>

#if REDCONF_API_POSIX == 1

#include <redposix.h>
#include <redtoolcmn.h>
#include <redvolume.h>


typedef struct
{
    const char *pszVolSpec;
    const char *pszBDevSpec;
    const bool  fFormat;
    int         showHelp;
} REDOPTIONS;


static int fuse_red_getattr(const char *pszPath, struct stat *stbuf);
static int fuse_red_access(const char *pszPath, int mask);
static int fuse_red_create(const char *pszPath, mode_t mode, struct fuse_file_info *pFileInfo);
static int fuse_red_mkdir(const char *pszPath, mode_t mode);
static int fuse_red_unlink(const char *pszPath);
static int fuse_red_rmdir(const char *pszPath);
static int fuse_red_rename(const char *pszOldPath, const char *pszNewPath);
static int fuse_red_link(const char *pszOldPath, const char *pszNewPath);
static int fuse_red_chmod(const char *pszPath, mode_t mode);
static int fuse_red_truncate(const char *pszPath, off_t size);
static int fuse_red_open(const char *pszPath, struct fuse_file_info *pFileInfo);
static int fuse_red_read(const char *pszPath, char *pcBuf, size_t size, off_t offset, struct fuse_file_info *pFileInfo);
static int fuse_red_write(const char *pszPath, const char *pcBuf, size_t size, off_t offset, struct fuse_file_info *pFileInfo);
static int fuse_red_statfs(const char *pszPath, struct statvfs *stbuf);
static int fuse_red_release(const char *pszPath, struct fuse_file_info *pFileInfo);
static int fuse_red_fsync(const char *pszPath, int type, struct fuse_file_info *pFileInfo);
static int fuse_red_readdir(const char *pszPath, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *pFileInfo);
static int fuse_red_fsyncdir(const char *pszPath, int type, struct fuse_file_info *pFileInfo);
static void *fuse_red_init(struct fuse_conn_info *conn);
static void fuse_red_destroy(void *context);
static int fuse_red_ftruncate(const char *pszPath, off_t size, struct fuse_file_info *pFileInfo);
static int fuse_red_fgetattr(const char *pszPath, struct stat *stbuf, struct fuse_file_info *pFileInfo);
static int fgetattr_sub(struct stat *stbuf,  struct fuse_file_info *pFileInfo);
static int fuse_reltr_utimens(const char *pszPath, const struct timespec tv[2]);
static mode_t redmode_to_mode(uint16_t uRedMode);
static int rederrno_to_errno(int32_t rederrno);
static uint32_t flags_to_redflags(int flags);
static int32_t red_local_open(const char *pszPath, int flags);


static struct fuse_operations red_oper =
{
    .getattr = fuse_red_getattr,
    .access = fuse_red_access,
    .create = fuse_red_create,
    .mkdir = fuse_red_mkdir,
    .unlink = fuse_red_unlink,
    .rmdir = fuse_red_rmdir,
    .rename = fuse_red_rename,
    .link = fuse_red_link,
    .chmod = fuse_red_chmod,
    .truncate = fuse_red_truncate,
    .open = fuse_red_open,
    .read = fuse_red_read,
    .write = fuse_red_write,
    .statfs = fuse_red_statfs,
    .release = fuse_red_release,
    .fsync = fuse_red_fsync,
    .readdir = fuse_red_readdir,
    .fsyncdir = fuse_red_fsyncdir,
    .init = fuse_red_init,
    .destroy = fuse_red_destroy,
    .ftruncate = fuse_red_ftruncate,
    .fgetattr = fuse_red_fgetattr,
    .utimens = fuse_reltr_utimens
};


static uint8_t gbVolume;
static const char *gpszVolume;

static pthread_mutex_t gFuseMutex = PTHREAD_MUTEX_INITIALIZER;
#define REDFS_LOCK() (void)pthread_mutex_lock(&gFuseMutex)
#define REDFS_UNLOCK() (void)pthread_mutex_unlock(&gFuseMutex)

static REDOPTIONS gOptions;

#define OPTION(t, p) { t, offsetof(REDOPTIONS, p), 1 }

static const struct fuse_opt gOptionSpec[] =
{
    OPTION("--vol=%s", pszVolSpec),
    OPTION("--dev=%s", pszBDevSpec),
    OPTION("-D %s", pszBDevSpec),
    OPTION("--format", fFormat),
    OPTION("-h", showHelp),
    OPTION("--help", showHelp),
    FUSE_OPT_END
};


static void show_help(
    const char *progname)
{
    fprintf(stderr, "usage: %s <mountpoint> [options]\n\n", progname);
    fprintf(stderr,
"Reliance Edge specific options:\n"
"    --vol=volumeID             A volume number (e.g., 2) or a volume path\n"
"                               prefix (e.g., VOL1: or /data) of the volume to\n"
"                               mount.  Mandatory if Reliance Edge is configured\n"
"                               with multiple volumes.\n"
"    --dev=devname, -D devname  Specifies the device name.  This can be the\n"
"                               path and name of a file disk (e.g., red.bin);\n"
"                               or an OS-specific reference to a device (on\n"
"                               Linux, a device file like /dev/sdb).\n"
"    --format                   Format the volume before mounting with fuse.\n"
"\n");
}


/** @brief Entry point for the Reliance Edge FUSE implementation.

    Reliance Edge can be installed as a FUSE driver (File System in User Space)
    on Linux.  This allows a user to mount a Reliance Edge volume within a
    folder so that it appears like a native Linux file system.  The contents of
    the volume can then be accessed with a file browser or any other Linux
    program.

    This function is a simple wrapper to parse Reliance Edge-specific options
    before calling fuse_main.
*/
int main(
    int                 argc,
    char               *argv[])
{
    int                 result;
    bool                fShowHelp = false;
    struct fuse_args    args = FUSE_ARGS_INIT(argc, argv);
    REDSTATUS           status;     /* For RedOsBDevConfig() return value */

    (void)argc;

    /*  Initialize immediately to ensure the output is printed.
    */
    if(red_init() != 0)
    {
        fprintf(stderr, "Unexpected error %d from red_init()\n", (int)red_errno);
        return 1;
    }

    /*  Parse options
    */
    if(fuse_opt_parse(&args, &gOptions, gOptionSpec, NULL) == -1)
    {
        return 1;
    }

    if(gOptions.showHelp)
    {
        fShowHelp = true;
        goto ShowHelp;
    }

    if(gOptions.pszBDevSpec == NULL)
    {
        fprintf(stderr, "You need to specify a file name (option --dev) for Reliance Edge\n\n");
        fShowHelp = true;
        goto ShowHelp;
    }

    if(gOptions.pszVolSpec == NULL)
    {
        /*  If there is only one volume, use it.  Otherwise require the user to
            specify.
        */
      #if REDCONF_VOLUME_COUNT == 1
        gOptions.pszVolSpec = "0";
      #else
        fprintf(stderr, "You need to specify a Reliance Edge volume name or number (option --vol)\n\n");
        fShowHelp = true;
        goto ShowHelp;
      #endif
    }

    assert(gOptions.pszVolSpec != NULL);
    gbVolume = RedFindVolumeNumber(gOptions.pszVolSpec);

    if(gbVolume >= REDCONF_VOLUME_COUNT)
    {
        fprintf(stderr, "Invalid volume specifier \"%s\"\n", gOptions.pszVolSpec);
        return 1;
    }

    gpszVolume = gaRedVolConf[gbVolume].pszPathPrefix;

    status = RedOsBDevConfig(gbVolume, gOptions.pszBDevSpec);
    if(status != 0)
    {
        fprintf(stderr, "Unexpected error %d from RedOsBDevConfig()\n", (int)status);
        return 1;
    }

    if(gOptions.fFormat)
    {
      #if (REDCONF_API_POSIX_FORMAT == 1)
        if(red_format(gpszVolume) != 0)
        {
            fprintf(stderr, "Error %d from red_format().\n"
"    Make sure you can access the device specified and that it is compatible\n"
"    with your Reliance Edge volume configuration.\n", (int)red_errno);
            return 1;
        }
      #else
        fprintf(stderr, "red_format() is not supported\n");
        return 1;
      #endif
    }

    if(red_mount(gpszVolume) != 0)
    {
        fprintf(stderr, "Error %d from red_mount().\n"
"    Make sure you can access the device specified and that it is compatible\n"
"    with your Reliance Edge volume configuration.\n", (int)red_errno);
        return 1;
    }

  ShowHelp:
    /*  When --help is specified, first print our own file-system
        specific help text, then signal fuse_main to show
        additional help (by adding `--help` to the options again)
        without usage: line (by setting argv[0] to the empty
        string)
    */
    if(fShowHelp)
    {
        show_help(argv[0]);
        result = fuse_opt_add_arg(&args, "--help");
        assert(result == 0);
        args.argv[0] = (char *)"";
    }

    return fuse_main(args.argc, args.argv, &red_oper, NULL);
}


static int fuse_red_getattr(
    const char     *pszPath,
    struct stat    *stbuf)
{
    int             result;
    int32_t         iFd;

    REDFS_LOCK();

    iFd = red_local_open(pszPath, O_RDONLY);
    if(iFd < 0)
    {
        result = rederrno_to_errno(red_errno);
    }
    else
    {
        struct fuse_file_info fileInfo;

        fileInfo.fh = (uint64_t)iFd;

        result = fgetattr_sub(stbuf, &fileInfo);

        (void)red_close(iFd);
    }

    REDFS_UNLOCK();

    return result;
}


static int fuse_red_access(
    const char *pszPath,
    int         mask)
{
    struct stat st;
    int         result;

    result = fuse_red_getattr(pszPath, &st);

    /*  Reliance Edge doesn't support permissions, so access is always OK as
        long as we can successfully open the file.
    */
    (void)mask;

    return result;
}


static int fuse_red_create(
    const char             *pszPath,
    mode_t                  mode,
    struct fuse_file_info  *pFileInfo)
{
    int32_t                 iFd;
    int                     result = 0;

    assert(pFileInfo != NULL);

    REDFS_LOCK();

    (void)mode;

    iFd = red_local_open(pszPath, pFileInfo->flags | O_CREAT);

    if(iFd < 0)
    {
        result = rederrno_to_errno(red_errno);
    }
    else
    {
        pFileInfo->fh = (uint64_t)iFd;
    }

    REDFS_UNLOCK();

    return result;
}


static int fuse_red_mkdir(
    const char *pszPath,
    mode_t      mode)
{
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_MKDIR == 1)
    int         result = 0;
    char        acRedPath[PATH_MAX];

    REDFS_LOCK();

    if(snprintf(acRedPath, PATH_MAX, "%s%c%s", gpszVolume, REDCONF_PATH_SEPARATOR, pszPath) >= PATH_MAX)
    {
        fprintf(stderr, "Error: path too long (%s%c%s)\n", gpszVolume, REDCONF_PATH_SEPARATOR, pszPath);
        result = -ENAMETOOLONG;
    }
    else if(red_mkdir(acRedPath) != 0)
    {
        result = rederrno_to_errno(red_errno);
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;
    (void)mode;

    return -ENOSYS;
  #endif
}


static int fuse_red_unlink(
    const char *pszPath)
{
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_UNLINK == 1)
    int         result = 0;
    char        acRedPath[PATH_MAX];

    REDFS_LOCK();

    if(snprintf(acRedPath, PATH_MAX, "%s%c%s", gpszVolume, REDCONF_PATH_SEPARATOR, pszPath) >= PATH_MAX)
    {
        fprintf(stderr, "Error: path too long (%s%c%s)\n", gpszVolume, REDCONF_PATH_SEPARATOR, pszPath);
        result = -ENAMETOOLONG;
    }
    else if(red_unlink(acRedPath) != 0)
    {
        result = rederrno_to_errno(red_errno);
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;

    return -ENOSYS;
  #endif
}


static int fuse_red_rmdir(
    const char *pszPath)
{
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_RMDIR == 1)
    int         result = 0;
    char        acRedPath[PATH_MAX];

    REDFS_LOCK();

    if(snprintf(acRedPath, PATH_MAX, "%s%c%s", gpszVolume, REDCONF_PATH_SEPARATOR, pszPath) >= PATH_MAX)
    {
        fprintf(stderr, "Error: path too long (%s%c%s)\n", gpszVolume, REDCONF_PATH_SEPARATOR, pszPath);
        result = -ENAMETOOLONG;
    }
    else if(red_rmdir(acRedPath) != 0)
    {
        result = rederrno_to_errno(red_errno);
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;

    return -ENOSYS;
  #endif
}


static int fuse_red_rename(
    const char *pszOldPath,
    const char *pszNewPath)
{
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_RENAME == 1)
    int         result = 0;
    char        acOldRedPath[PATH_MAX];
    char        acNewRedPath[PATH_MAX];

    REDFS_LOCK();

    if(snprintf(acOldRedPath, PATH_MAX, "%s%c%s", gpszVolume, REDCONF_PATH_SEPARATOR, pszOldPath) >= PATH_MAX)
    {
        fprintf(stderr, "Error: path too long (%s%c%s)\n", gpszVolume, REDCONF_PATH_SEPARATOR, pszOldPath);
        result = -ENAMETOOLONG;
    }
    else if(snprintf(acNewRedPath, PATH_MAX, "%s%c%s", gpszVolume, REDCONF_PATH_SEPARATOR, pszNewPath) >= PATH_MAX)
    {
        fprintf(stderr, "Error: path too long (%s%c%s)\n", gpszVolume, REDCONF_PATH_SEPARATOR, pszNewPath);
        result = -ENAMETOOLONG;
    }
    else if(red_rename(acOldRedPath, acNewRedPath) != 0)
    {
        result = rederrno_to_errno(red_errno);
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszOldPath;
    (void)pszNewPath;

    return -ENOSYS;
  #endif
}


static int fuse_red_link(
    const char *pszOldPath,
    const char *pszNewPath)
{
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_LINK == 1)
    int         result = 0;
    char        acOldRedPath[PATH_MAX];
    char        acNewRedPath[PATH_MAX];

    REDFS_LOCK();

    if(snprintf(acOldRedPath, PATH_MAX, "%s%c%s", gpszVolume, REDCONF_PATH_SEPARATOR, pszOldPath) >= PATH_MAX)
    {
        fprintf(stderr, "Error: path too long (%s%c%s)\n", gpszVolume, REDCONF_PATH_SEPARATOR, pszOldPath);
        result = -ENAMETOOLONG;
    }
    else if(snprintf(acNewRedPath, PATH_MAX, "%s%c%s", gpszVolume, REDCONF_PATH_SEPARATOR, pszNewPath) >= PATH_MAX)
    {
        fprintf(stderr, "Error: path too long (%s%c%s)\n", gpszVolume, REDCONF_PATH_SEPARATOR, pszNewPath);
        result = -ENAMETOOLONG;
    }
    else if(red_link(acOldRedPath, acNewRedPath) != 0)
    {
        result = rederrno_to_errno(red_errno);
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszOldPath;
    (void)pszNewPath;

    return -ENOSYS;
  #endif
}


static int fuse_red_chmod(
    const char *pszPath,
    mode_t      mode)
{
    /*  We don't support this, but FUSE whines if it's not implemented.
    */
    (void)pszPath;
    (void)mode;

    return -ENOSYS;
}


static int fuse_red_truncate(
    const char *pszPath,
    off_t       size)
{
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FTRUNCATE == 1)
    int32_t     iFd;
    int         result = 0;

    if(size < 0)
    {
        return -EINVAL;
    }

    REDFS_LOCK();

    iFd = red_local_open(pszPath, O_WRONLY);

    if(iFd < 0)
    {
        result = rederrno_to_errno(red_errno);
    }
    else
    {
        if(red_ftruncate(iFd, (uint64_t)size) != 0)
        {
            result = rederrno_to_errno(red_errno);
        }

        (void)red_close(iFd);
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;
    (void)size;

    return -ENOSYS;
  #endif
}


static int fuse_red_open(
    const char             *pszPath,
    struct fuse_file_info  *pFileInfo)
{
    int32_t                 iFd;
    int                     result = 0;

    assert(pFileInfo != NULL);

    REDFS_LOCK();

    iFd = red_local_open(pszPath, pFileInfo->flags);

    if(iFd < 0)
    {
        result = rederrno_to_errno(red_errno);
    }
    else
    {
        pFileInfo->fh = (uint64_t)iFd;
    }

    REDFS_UNLOCK();

    return result;
}


static int fuse_red_read(
    const char             *pszPath,
    char                   *pcBuf,
    size_t                  size,
    off_t                   offset,
    struct fuse_file_info  *pFileInfo)
{
    int32_t                 iFd;
    int                     result = 0;

    (void)pszPath;

    assert(pFileInfo != NULL);
    if(offset < 0 || size > INT_MAX)
    {
        return -EINVAL;
    }

    REDFS_LOCK();

    iFd = (int32_t)pFileInfo->fh;

    if(red_lseek(iFd, offset, RED_SEEK_SET) == -1)
    {
        result = rederrno_to_errno(red_errno);
    }
    else
    {
        result = (int)red_read(iFd, pcBuf, size);

        if(result < 0)
        {
            result = rederrno_to_errno(red_errno);
        }
    }

    REDFS_UNLOCK();

    return result;
}


static int fuse_red_write(
    const char             *pszPath,
    const char             *pcBuf,
    size_t                  size,
    off_t                   offset,
    struct fuse_file_info  *pFileInfo)
{
  #if REDCONF_READ_ONLY == 0
    int32_t                 iFd;
    int                     result = 0;

    (void)pszPath;

    assert(pFileInfo != NULL);
    if(offset < 0 || size > INT_MAX)
    {
        return -EINVAL;
    }

    REDFS_LOCK();

    iFd = (int32_t)pFileInfo->fh;

    if(red_lseek(iFd, offset, RED_SEEK_SET) == -1)
    {
        result = rederrno_to_errno(red_errno);
    }
    else
    {
        result = (int)red_write(iFd, pcBuf, size);

        if(result < 0)
        {
            result = rederrno_to_errno(red_errno);
        }
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;
    (void)pcBuf;
    (void)size;
    (void)offset;
    (void)pFileInfo;

    return -ENOSYS;
  #endif
}


static int fuse_red_statfs(
    const char     *pszPath,
    struct statvfs *stbuf)
{
    int             result = 0;
    REDSTATFS       redstbuf;

    (void)pszPath;

    REDFS_LOCK();

    result = red_statvfs(gpszVolume, &redstbuf);
    if(result == -1)
    {
        result = rederrno_to_errno(red_errno);
    }
    else
    {
        stbuf->f_bsize = redstbuf.f_bsize;
        stbuf->f_frsize = redstbuf.f_frsize;
        stbuf->f_blocks = redstbuf.f_blocks;
        stbuf->f_bfree = redstbuf.f_bfree;
        stbuf->f_bavail = redstbuf.f_bavail;
        stbuf->f_files = redstbuf.f_files;
        stbuf->f_ffree = redstbuf.f_ffree;
        stbuf->f_favail = redstbuf.f_favail;
        stbuf->f_fsid = redstbuf.f_fsid;
        stbuf->f_flag = redstbuf.f_flag;
        stbuf->f_namemax = redstbuf.f_namemax;
    }

    REDFS_UNLOCK();

    return result;
}


static int fuse_red_release(
    const char             *pszPath,
    struct fuse_file_info  *pFileInfo)
{
    int                     result = 0;

    assert(pFileInfo != NULL);

    (void)pszPath;

    REDFS_LOCK();

    if(red_close((int32_t)pFileInfo->fh) != 0)
    {
        result = rederrno_to_errno(red_errno);
    }

    REDFS_UNLOCK();

    return result;
}


static int fuse_red_fsync(
    const char             *pszPath,
    int                     type,
    struct fuse_file_info  *pFileInfo)
{
  #if REDCONF_READ_ONLY == 0
    int                     result = 0;

    assert(pFileInfo != NULL);

    (void)pszPath;
    (void)type;

    REDFS_LOCK();

    if(red_fsync((int32_t)pFileInfo->fh) != 0)
    {
        result = rederrno_to_errno(red_errno);
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;
    (void)type;
    (void)pFileInfo;

    return -ENOSYS;
  #endif
}


static int fuse_red_readdir(
    const char             *pszPath,
    void                   *pBuf,
    fuse_fill_dir_t         filler,
    off_t                   offset,
    struct fuse_file_info  *pFileInfo)
{
  #if REDCONF_API_POSIX_READDIR == 1
    REDDIR                 *pDir;
    REDDIRENT              *pDirent;
    int                     result = 0;
    char                    acRedPath[PATH_MAX];

    (void)offset;
    (void)pFileInfo;

    REDFS_LOCK();

    if(snprintf(acRedPath, PATH_MAX, "%s%c%s", gpszVolume, REDCONF_PATH_SEPARATOR, pszPath) >= PATH_MAX)
    {
        fprintf(stderr, "Error: path too long (%s%c%s)\n", gpszVolume, REDCONF_PATH_SEPARATOR, pszPath);
        result = -ENAMETOOLONG;
    }
    else
    {
        pDir = red_opendir(acRedPath);
        if(pDir == NULL)
        {
            result = rederrno_to_errno(red_errno);
        }
    }

    if(result == 0)
    {
        while((pDirent = red_readdir(pDir)) != NULL)
        {
            struct stat st;

            memset(&st, 0, sizeof(st));
            st.st_ino = pDirent->d_ino;
            st.st_mode = redmode_to_mode(pDirent->d_stat.st_mode);

            /*  Supplying 0 as the "offset" of all dirents lets filler() know
                that it should read all directory entries at once.
            */
            if(filler(pBuf, pDirent->d_name, &st, 0))
            {
                break;
            }
        }

        (void)red_closedir(pDir);
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;
    (void)filler;
    (void)offset;
    (void)pFileInfo;

    return -ENOSYS;
  #endif
}


static int fuse_red_fsyncdir(
    const char             *pszPath,
    int                     type,
    struct fuse_file_info  *pFileInfo)
{
  #if REDCONF_READ_ONLY == 0
    int                     result = 0;

    (void)pszPath;
    (void)type;
    (void)pFileInfo;

    REDFS_LOCK();

    /*  Current implementation: transact if RED_TRANSACT_FSYNC is enabled,
        ignoring the file path given, since this is what red_fsync does
        internally.  This may need to change if the behavior of red_fsync
        changes in the future.
    */
    if(gaRedVolume[gbVolume].ulTransMask & RED_TRANSACT_FSYNC)
    {
        if(red_transact(gpszVolume) != 0)
        {
            result = rederrno_to_errno(red_errno);
        }
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;
    (void)type;
    (void)pFileInfo;

    return -ENOSYS;
  #endif
}


static void *fuse_red_init(
    struct fuse_conn_info  *conn)
{
    (void)conn;

    REDFS_LOCK();

    /*  We already called red_mount() in main(); call it again just in case
        fuse_red_destroy() has been called and we are re-mounting.  But ignore
        RED_EBUSY errors because the volume may already be mounted.
    */
    if((red_mount(gpszVolume) != 0) && (red_errno != RED_EBUSY))
    {
        fprintf(stderr, "Unexpected error %d from red_mount()\n", (int)red_errno);
        exit(rederrno_to_errno(red_errno));
    }

    REDFS_UNLOCK();

    return NULL;
}


static void fuse_red_destroy(
    void *context)
{
    (void)context;

    REDFS_LOCK();

    if(red_umount(gpszVolume) != 0)
    {
        fprintf(stderr, "red_umount() failed, errno %d\n", red_errno);
        exit(1);
    }

    /*  Note: don't uninit just in case fuse_red_init() is called again.
        There is nothing particularly bad about leaving the filesystem
        initialized until the task is aborted.
    */

    REDFS_UNLOCK();
}


static int fuse_red_ftruncate(
    const char             *pszPath,
    off_t                   size,
    struct fuse_file_info  *pFileInfo)
{
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FTRUNCATE == 1)
    int                     result = 0;

    (void)pszPath;
    assert(pFileInfo != NULL);

    REDFS_LOCK();

    if(red_ftruncate((int32_t)pFileInfo->fh, (uint64_t)size) != 0)
    {
        result = rederrno_to_errno(red_errno);
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;
    (void)size;
    (void)pFileInfo;

    return -ENOSYS;
  #endif
}


static int fuse_red_fgetattr(
    const char             *pszPath,
    struct stat            *stbuf,
    struct fuse_file_info  *pFileInfo)
{
    int                     result;

    (void)pszPath;
    assert(pFileInfo != NULL);

    REDFS_LOCK();

    result = fgetattr_sub(stbuf, pFileInfo);

    REDFS_UNLOCK();

    return result;
}


static int fgetattr_sub(
    struct stat            *stbuf,
    struct fuse_file_info  *pFileInfo)
{
    int                     result = 0;
    REDSTAT                 redstbuf;

    assert(pFileInfo != NULL);

    memset(stbuf, 0, sizeof(*stbuf));

    if(red_fstat((int32_t)pFileInfo->fh, &redstbuf) != 0)
    {
        result = rederrno_to_errno(red_errno);
    }
    else
    {
        /*  Translate the REDSTAT to Unix stat
        */
        stbuf->st_dev = redstbuf.st_dev;
        stbuf->st_ino = redstbuf.st_ino;
        stbuf->st_mode = redmode_to_mode(redstbuf.st_mode);
        stbuf->st_nlink = redstbuf.st_nlink;
        stbuf->st_size = redstbuf.st_size;
      #if REDCONF_INODE_TIMESTAMPS == 1
      #ifdef POSIX_2008_STAT
        stbuf->st_atim.tv_sec = redstbuf.st_atime;
        stbuf->st_ctim.tv_sec = redstbuf.st_ctime;
        stbuf->st_mtim.tv_sec = redstbuf.st_mtime;
      #else
        stbuf->st_atime = redstbuf.st_atime;
        stbuf->st_ctime = redstbuf.st_ctime;
        stbuf->st_mtime = redstbuf.st_mtime;
      #endif
      #endif
      #if REDCONF_INODE_BLOCKS == 1
        stbuf->st_blocks = redstbuf.st_blocks;
      #endif
    }

    return result;
}


static int fuse_reltr_utimens(
    const char             *pszPath,
    const struct timespec   tv[2])
{
    /*  We don't support this, but FUSE whines if it's not implemented.
    */
    (void)pszPath;
    (void)tv;
    return -ENOSYS;
}


static mode_t redmode_to_mode(
    uint16_t    uRedMode)
{
    mode_t      linuxMode;

    /*  The Reliance Edge mode bits only store whether the file is a regular
        file or directory; Reliance Edge does not use permissions.  So we
        add hard-coded permissions here.
    */

    if(RED_S_ISDIR(uRedMode))
    {
        linuxMode = S_IFDIR;
        linuxMode |= S_IXUSR | S_IXGRP | S_IXOTH;
    }
    else
    {
        linuxMode = S_IFREG;
    }

    /*  Always allow read access; allow write access if the file system is not
        readonly.
    */
    linuxMode |= S_IRUSR | S_IRGRP | S_IROTH;

  #if REDCONF_READ_ONLY == 0
    linuxMode |= S_IWUSR | S_IWGRP | S_IWOTH;
  #endif

    return linuxMode;
}


static int rederrno_to_errno(
    int32_t rederrno)
{
    switch(rederrno)
    {
        case 0:             return 0;
        case RED_EPERM:     return -EPERM;
        case RED_ENOENT:    return -ENOENT;
        case RED_EIO:       return -EIO;
        case RED_EBADF:     return -EBADF;
        case RED_ENOMEM:    return -ENOMEM;
        case RED_EBUSY:     return -EBUSY;
        case RED_EEXIST:    return -EEXIST;
        case RED_EXDEV:     return -EXDEV;
        case RED_ENOTDIR:   return -ENOTDIR;
        case RED_EISDIR:    return -EISDIR;
        case RED_EINVAL:    return -EINVAL;
        case RED_ENFILE:    return -ENFILE;
        case RED_EMFILE:    return -EMFILE;
        case RED_EFBIG:     return -EFBIG;
        case RED_ENOSPC:    return -ENOSPC;
        case RED_EROFS:     return -EROFS;
        case RED_EMLINK:    return -EMLINK;
        case RED_ERANGE:    return -ERANGE;
        case RED_ENAMETOOLONG: return -ENAMETOOLONG;
        case RED_ENOSYS:    return -ENOSYS;
        case RED_ENOTEMPTY: return -ENOTEMPTY;
        case RED_ENODATA:   return -ENODATA;
        case RED_EUSERS:    return -EUSERS;
        default:            return -EINVAL;    /* Not expected, but default to EINVAL */
    }
}


static uint32_t flags_to_redflags(
    int         flags)
{
    uint32_t    ulRedFlags = RED_O_RDONLY;

    if(flags & O_WRONLY)
    {
        ulRedFlags = RED_O_WRONLY;
    }
    else if(flags & O_RDWR)
    {
        ulRedFlags = RED_O_RDWR;
    }

    if(flags & O_CREAT)
    {
        ulRedFlags |= RED_O_CREAT;
    }

    if(flags & O_TRUNC)
    {
        ulRedFlags |= RED_O_TRUNC;
    }

    if(flags & O_EXCL)
    {
        ulRedFlags |= RED_O_EXCL;
    }

    if(flags & O_APPEND)
    {
        ulRedFlags |= RED_O_APPEND;
    }

    return ulRedFlags;
}


static int32_t red_local_open(
    const char *pszPath,
    int         flags)
{
    char        acRedPath[PATH_MAX];
    int32_t     result;

    if(snprintf(acRedPath, PATH_MAX, "%s%c%s", gpszVolume, REDCONF_PATH_SEPARATOR, pszPath) >= PATH_MAX)
    {
        fprintf(stderr, "Error: path too long (%s%c%s)\n", gpszVolume, REDCONF_PATH_SEPARATOR, pszPath);

        /*  The desired error is -ENAMETOOLONG, so we set red_errno
            appropriately.
        */
        red_errno = RED_ENAMETOOLONG;
        result = -1;
    }
    else
    {
        result = red_open(acRedPath, flags_to_redflags(flags));
    }

    return result;
}

#else /* REDCONF_API_POSIX */

int main(void)
{
    fprintf(stderr, "Reliance Edge FUSE port does not support the FSE API.\n");
    return 1;
}

#endif /* REDCONF_API_POSIX */

