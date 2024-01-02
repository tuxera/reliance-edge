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


#if REDCONF_POSIX_OWNER_PERM == 1

/*  The os/linux/services/osuidgid.c implementation of these functions isn't
    suitable for FUSE.  That file is stripped from the FUSE build and instead
    we reimplement the functions here.
*/
uint32_t RedOsUserId(void) { return fuse_get_context()->uid; }
uint32_t RedOsGroupId(void) { return fuse_get_context()->gid; }
bool RedOsIsGroupMember(uint32_t ulGid) { return RedOsGroupId() == ulGid; }
bool RedOsIsPrivileged(void)
{
    /*  User is always privileged.  This implicitly disables all permissions
        enforcement in the Reliance Edge POSIX-like API, which is what we want:
        redfuse is a developer tool, intended to allow the developer to view and
        modify a file system on removable media from an embedded target.  If
        we enforced permissions, that would just get in the way.  If permissions
        enforcement is really desired, then -o default_permissions (a FUSE mount
        option) can be used to enable enforcement in the kernel.
    */
    return true;
}

  /*  Reliance Edge uses the same permission bit values as Linux.  The code
      assumes that no translation needs to occur: make sure that this is the
      case.
  */
  #if (RED_S_IFREG != S_IFREG) || (RED_S_IFDIR != S_IFDIR) || (RED_S_ISUID != S_ISUID) || (RED_S_ISGID != S_ISGID) || \
      (RED_S_ISVTX != S_ISVTX) || (RED_S_IRWXU != S_IRWXU) || (RED_S_IRWXG != S_IRWXG) || (RED_S_IRWXO != S_IRWXO)
    #error "error: Reliance Edge permission bits don't match host OS permission bits!"
  #endif

#endif /* REDCONF_POSIX_OWNER_PERM == 1 */


typedef struct
{
    const char *pszVolSpec;
    const char *pszBDevSpec;
    int         fFormat;    /* Logically a boolean but type must be int */
    int         fShowHelp;  /* Same as above */
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
static int fuse_red_chown(const char *pszPath, uid_t uid, gid_t gid);
static int fuse_red_truncate(const char *pszPath, off_t size);
static int fuse_red_open(const char *pszPath, struct fuse_file_info *pFileInfo);
static int fuse_red_symlink(const char *pszPath, const char *pszSymlink);
static int fuse_red_readlink(const char *pszPath, char *pszBuffer, size_t nBufferSize);
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
static int fuse_red_utimens(const char *pszPath, const struct timespec tv[2]);
static void redstat_to_stat(const REDSTAT *pRedSB, struct stat *pSB);
static mode_t redmode_to_mode(uint16_t uRedMode);
static int rederrno_to_errno(int32_t rederrno);
static uint32_t flags_to_redflags(int flags);
static int32_t red_local_open(const char *pszPath, int flags, mode_t mode);
static int red_make_full_path(const char *pszPath, char *pszBuffer, size_t nBufferSize);
#if REDCONF_PATH_SEPARATOR != '/'
static char *string_copy_replace(const char *pszStr, char cFind, char cReplace);
static void string_replace(char *pszStr, size_t nMax, char cFind, char cReplace);
#endif


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
    .chown = fuse_red_chown,
    .truncate = fuse_red_truncate,
    .open = fuse_red_open,
    .symlink = fuse_red_symlink,
    .readlink = fuse_red_readlink,
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
    .utimens = fuse_red_utimens
};


static uint8_t gbVolume;
static const char *gpszVolume;

static pthread_mutex_t gFuseMutex = PTHREAD_MUTEX_INITIALIZER;
#define REDFS_LOCK() (void)pthread_mutex_lock(&gFuseMutex)
#define REDFS_UNLOCK() (void)pthread_mutex_unlock(&gFuseMutex)

static REDOPTIONS gOptions;

#define OPTION(t, p) { t, offsetof(REDOPTIONS, p), 1 }

static const struct fuse_opt gaOptionSpec[] =
{
    OPTION("--vol=%s", pszVolSpec),
    OPTION("--dev=%s", pszBDevSpec),
    OPTION("-D %s", pszBDevSpec),
    OPTION("--format", fFormat),
    OPTION("-h", fShowHelp),
    OPTION("--help", fShowHelp),
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
    if(fuse_opt_parse(&args, &gOptions, gaOptionSpec, NULL) == -1)
    {
        return 1;
    }

    if(gOptions.fShowHelp)
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
      #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FORMAT == 1)
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

    iFd = red_local_open(pszPath, O_RDONLY, 0);
    if(iFd < 0)
    {
        result = rederrno_to_errno(red_errno);
    }
    else
    {
        struct fuse_file_info fileInfo;

        fileInfo.fh = (uint64_t)iFd;

        result = fgetattr_sub(stbuf, &fileInfo);

        if((red_close(iFd) != 0) && (result == 0))
        {
            result = rederrno_to_errno(red_errno);
        }
    }

    REDFS_UNLOCK();

    return result;
}


/*  Note that this function is rarely called: an explicit access(2) call or a
    chdir(2) will invoke it, but other path-based operations do not.  As a
    result, implementing this function does _not_ mean that permissions are
    enforced by this FUSE driver.  To enforce permissions with FUSE, either:
    a) permission checks must be done internally for each operation, which we do
    _not_ do; or b) the file system must be mounted with -o default_permissions.
    In the latter case, the Linux kernel will enforce the permissions without
    assistance from the FUSE driver (this function will not be called).
*/
static int fuse_red_access(
    const char *pszPath,
    int         mask)
{
    struct stat st;
    int         result;

    result = fuse_red_getattr(pszPath, &st);

  #if REDCONF_POSIX_OWNER_PERM == 1
    if(result == 0)
    {
        const struct fuse_context  *pCtx = fuse_get_context();
        bool                        fUib = pCtx->uid == st.st_uid;
        bool                        fGib = pCtx->gid == st.st_gid;
        mode_t                      mode = st.st_mode;

        if(    (    ((mask & X_OK) != 0)
                 && (fUib
                      ? ((mode & S_IXUSR) == 0U)
                      : (fGib
                          ? ((mode & S_IXGRP) == 0U)
                          : ((mode & S_IXOTH) == 0U))))
            || (    ((mask & W_OK) != 0)
                 && (fUib
                      ? ((mode & S_IWUSR) == 0U)
                      : (fGib
                          ? ((mode & S_IWGRP) == 0U)
                          : ((mode & S_IWOTH) == 0U))))
            || (    ((mask & R_OK) != 0)
                 && (fUib
                      ? ((mode & S_IRUSR) == 0U)
                      : (fGib
                          ? ((mode & S_IRGRP) == 0U)
                          : ((mode & S_IROTH) == 0U)))))
        {
            result = -EACCES;
        }

        /*  "Oh, I'm sorry, Sir, go ahead, I didn't realize you were root."
        */
        if((result == -EACCES) && (pCtx->uid == RED_ROOT_USER))
        {
            result = 0;
        }
    }
  #else
    /*  In this configuration, Reliance Edge doesn't support permissions, so
        access is always OK as long as we can successfully open the file.
    */
    (void)mask;
  #endif

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

    iFd = red_local_open(pszPath, pFileInfo->flags | O_CREAT, mode);
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
    int         result;
    char        szRedPath[PATH_MAX];

    REDFS_LOCK();

    result = red_make_full_path(pszPath, szRedPath, sizeof(szRedPath));
    if(result == 0)
    {
        int32_t status;

      #if REDCONF_POSIX_OWNER_PERM == 1
        status = red_mkdir2(szRedPath, (uint16_t)mode & RED_S_IALLUGO);
      #else
        (void)mode;
        status = red_mkdir(szRedPath);
      #endif

        if(status != 0)
        {
            result = rederrno_to_errno(red_errno);
        }
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
    int         result;
    char        szRedPath[PATH_MAX];

    REDFS_LOCK();

    result = red_make_full_path(pszPath, szRedPath, sizeof(szRedPath));
    if(result == 0)
    {
        if(red_unlink(szRedPath) != 0)
        {
            result = rederrno_to_errno(red_errno);
        }
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
    int         result;
    char        szRedPath[PATH_MAX];

    REDFS_LOCK();

    result = red_make_full_path(pszPath, szRedPath, sizeof(szRedPath));
    if(result == 0)
    {
        if(red_rmdir(szRedPath) != 0)
        {
            result = rederrno_to_errno(red_errno);
        }
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
    int         result;
    char        szOldRedPath[PATH_MAX];
    char        szNewRedPath[PATH_MAX];

    REDFS_LOCK();

    result = red_make_full_path(pszOldPath, szOldRedPath, sizeof(szOldRedPath));

    if(result == 0)
    {
        result = red_make_full_path(pszNewPath, szNewRedPath, sizeof(szNewRedPath));
    }

    if(result == 0)
    {
        if(red_rename(szOldRedPath, szNewRedPath) != 0)
        {
            result = rederrno_to_errno(red_errno);
        }
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
    int         result;
    char        szOldRedPath[PATH_MAX];
    char        szNewRedPath[PATH_MAX];

    REDFS_LOCK();

    result = red_make_full_path(pszOldPath, szOldRedPath, sizeof(szOldRedPath));

    if(result == 0)
    {
        result = red_make_full_path(pszNewPath, szNewRedPath, sizeof(szNewRedPath));
    }

    if(result == 0)
    {
        if(red_link(szOldRedPath, szNewRedPath) != 0)
        {
            result = rederrno_to_errno(red_errno);
        }
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
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_POSIX_OWNER_PERM == 1)
    int         result;
    char        szRedPath[PATH_MAX];

    REDFS_LOCK();

    result = red_make_full_path(pszPath, szRedPath, sizeof(szRedPath));
    if(result == 0)
    {
        if(red_chmod(szRedPath, (uint16_t)mode & RED_S_IALLUGO) != 0)
        {
            result = rederrno_to_errno(red_errno);
        }
    }

    REDFS_UNLOCK();

    return result;
  #else
    /*  We don't support this, but FUSE whines if it's not implemented.
    */
    (void)pszPath;
    (void)mode;

    return -ENOSYS;
  #endif
}


static int fuse_red_chown(
    const char *pszPath,
    uid_t       uid,
    gid_t       gid)
{
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_POSIX_OWNER_PERM == 1)
    int         result;
    char        szRedPath[PATH_MAX];

    REDFS_LOCK();

    result = red_make_full_path(pszPath, szRedPath, sizeof(szRedPath));
    if(result == 0)
    {
        if(red_chown(szRedPath, (uint32_t)uid, (uint32_t)gid) != 0)
        {
            result = rederrno_to_errno(red_errno);
        }
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;
    (void)uid;
    (void)gid;

    return -ENOSYS;
  #endif
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

    iFd = red_local_open(pszPath, O_WRONLY, 0);
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

        if((red_close(iFd) != 0) && (result == 0))
        {
            result = rederrno_to_errno(red_errno);
        }
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

    /*  FUSE documentation says O_CREAT is never passed to this function.  If
        O_CREAT is passed to open():
        - If the file does not exist, FUSE calls ->create()
        - If the file exists, FUSE masks off O_CREAT and calls ->open()
    */
    assert((pFileInfo->flags & O_CREAT) == 0);

    REDFS_LOCK();

    iFd = red_local_open(pszPath, pFileInfo->flags, 0);
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


static int fuse_red_symlink(
    const char *pszPath,
    const char *pszSymlink)
{
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_SYMLINK == 1)
    int         result;
    char        szRedPath[PATH_MAX];

    REDFS_LOCK();

    result = red_make_full_path(pszSymlink, szRedPath, sizeof(szRedPath));

    if(result == 0)
    {
      #if REDCONF_PATH_SEPARATOR != '/'
        char *pszPathRedfs;

        pszPathRedfs = string_copy_replace(pszPath, '/', REDCONF_PATH_SEPARATOR);
        pszPath = pszPathRedfs;
        if(pszPathRedfs == NULL)
        {
            result = -ENOMEM;
        }
        else
      #endif
        {
            if(red_symlink(pszPath, szRedPath) != 0)
            {
                result = rederrno_to_errno(red_errno);
            }

          #if REDCONF_PATH_SEPARATOR != '/'
            free(pszPathRedfs);
          #endif
        }
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;
    (void)pszSymlink;

    return -ENOSYS;
  #endif
}


static int fuse_red_readlink(
    const char *pszPath,
    char       *pszBuffer,
    size_t      nBufferSize)
{
  #if REDCONF_API_POSIX_SYMLINK == 1
    int         result;
    char        szRedPath[PATH_MAX];

    REDFS_LOCK();

    result = red_make_full_path(pszPath, szRedPath, sizeof(szRedPath));

    if(result == 0)
    {
        if(red_readlink(szRedPath, pszBuffer, nBufferSize) != 0)
        {
            result = rederrno_to_errno(red_errno);
        }
      #if REDCONF_PATH_SEPARATOR != '/'
        else
        {
            string_replace(pszBuffer, nBufferSize, REDCONF_PATH_SEPARATOR, '/');
        }
      #endif
    }

    REDFS_UNLOCK();

    return result;
  #else
    (void)pszPath;
    (void)pszBuffer;
    (void)nBufferSize;

    return -ENOSYS;
  #endif
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
    result = (int)red_pread(iFd, pcBuf, size, (uint64_t)offset);
    if(result < 0)
    {
        result = rederrno_to_errno(red_errno);
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
    result = (int)red_pwrite(iFd, pcBuf, size, (uint32_t)offset);
    if(result < 0)
    {
        result = rederrno_to_errno(red_errno);
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
    int                     result;
    char                    szRedPath[PATH_MAX];

    (void)offset;
    (void)pFileInfo;

    REDFS_LOCK();

    result = red_make_full_path(pszPath, szRedPath, sizeof(szRedPath));

    if(result == 0)
    {
        pDir = red_opendir(szRedPath);
        if(pDir == NULL)
        {
            result = rederrno_to_errno(red_errno);
        }
    }

    if(result == 0)
    {
        while(true)
        {
            REDDIRENT  *pDirent;
            struct stat st;

            red_errno = 0;
            pDirent = red_readdir(pDir);
            if(pDirent == NULL)
            {
                if(red_errno != 0)
                {
                    result = rederrno_to_errno(red_errno);
                }

                break;
            }

            redstat_to_stat(&pDirent->d_stat, &st);

            /*  Supplying 0 as the "offset" of all dirents lets filler() know
                that it should read all directory entries at once.
            */
            if(filler(pBuf, pDirent->d_name, &st, 0))
            {
                break;
            }
        }

        if((red_closedir(pDir) != 0) && (result == 0))
        {
            result = rederrno_to_errno(red_errno);
        }
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
    if((gaRedVolume[gbVolume].ulTransMask & RED_TRANSACT_FSYNC) != 0U)
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
        REDFS_UNLOCK();
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
        REDFS_UNLOCK();
        fprintf(stderr, "Unexpected error %d from red_umount()\n", (int)red_errno);
        exit(rederrno_to_errno(red_errno));
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

    if(red_fstat((int32_t)pFileInfo->fh, &redstbuf) != 0)
    {
        result = rederrno_to_errno(red_errno);
    }
    else
    {
        redstat_to_stat(&redstbuf, stbuf);
    }

    return result;
}


static int fuse_red_utimens(
    const char             *pszPath,
    const struct timespec   tv[2])
{
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_INODE_TIMESTAMPS == 1)
    int                     result;
    char                    szRedPath[PATH_MAX];

    REDFS_LOCK();

    result = red_make_full_path(pszPath, szRedPath, sizeof(szRedPath));
    if(result == 0)
    {
        uint32_t    aulTimes[2U];
        uint32_t   *pulTimes;

        if(tv == NULL)
        {
            /*  For both utimens and red_utimes(), a NULL time pointer means to
                use the current time.
            */
            pulTimes = NULL;
        }
        else if((sizeof(tv[0U].tv_sec) > 4U) && ((tv[0U].tv_sec > (long)UINT32_MAX) || (tv[1U].tv_sec > (long)UINT32_MAX)))
        {
            /*  tv_sec is larger than the maximum value that Reliance Edge can
                store.
            */
            result = -ERANGE;
        }
        else
        {
            /*  Reliance Edge timestamps have a one-second resolution.  Truncate
                the provided timestamps.  Do _not_ round to the nearest second,
                since that could result in timestamps in the future.  Better to
                round down.
            */
            aulTimes[0U] = (uint32_t)tv[0U].tv_sec;
            aulTimes[1U] = (uint32_t)tv[1U].tv_sec;
            pulTimes = aulTimes;
        }

        if(result == 0)
        {
            if(red_utimes(szRedPath, pulTimes) != 0)
            {
                result = rederrno_to_errno(red_errno);
            }
        }
    }

    REDFS_UNLOCK();

    return result;
  #else
    /*  We don't support this, but FUSE whines if it's not implemented.
    */
    (void)pszPath;
    (void)tv;
    return -ENOSYS;
  #endif
}


/** @brief Translate a ::REDSTAT structure into a POSIX stat structure.

    @param pRedSB   The ::REDSTAT structure to translate.
    @param pbSB     The POSIX stat structure to populate.  Members of this
                    structure which don't exist in ::REDSTAT will be zeroed.
*/
static void redstat_to_stat(
    const REDSTAT  *pRedSB,
    struct stat    *pSB)
{
    memset(pSB, 0, sizeof(*pSB));
    pSB->st_dev = pRedSB->st_dev;
    pSB->st_ino = pRedSB->st_ino;
    pSB->st_mode = redmode_to_mode(pRedSB->st_mode);
    pSB->st_nlink = pRedSB->st_nlink;
  #if REDCONF_POSIX_OWNER_PERM == 1
    pSB->st_uid = (uid_t)pRedSB->st_uid;
    pSB->st_gid = (gid_t)pRedSB->st_gid;
  #endif
    pSB->st_size = pRedSB->st_size;
  #if REDCONF_INODE_TIMESTAMPS == 1
  #ifdef POSIX_2008_STAT
    pSB->st_atim.tv_sec = pRedSB->st_atime;
    pSB->st_ctim.tv_sec = pRedSB->st_ctime;
    pSB->st_mtim.tv_sec = pRedSB->st_mtime;
  #else
    pSB->st_atime = pRedSB->st_atime;
    pSB->st_ctime = pRedSB->st_ctime;
    pSB->st_mtime = pRedSB->st_mtime;
  #endif
  #endif
  #if REDCONF_INODE_BLOCKS == 1
    pSB->st_blocks = pRedSB->st_blocks;
  #endif
}


/** @brief Return the POSIX mode that should be used for a Reliance Edge mode.

    @param uRedMode The Reliance Edge mode.

    @return The POSIX mode that should be used for @p uRedMode.
*/
static mode_t redmode_to_mode(
    uint16_t    uRedMode)
{
    mode_t      linuxMode;

    /*  No need for translation: Reliance Edge mode bits have the same values as
        the Linux mode bits.
    */
    linuxMode = (mode_t)uRedMode;

    /*  One of the type bits should always be set.
    */
    assert(S_ISDIR(linuxMode) || S_ISREG(linuxMode) || S_ISLNK(linuxMode));

  #if REDCONF_POSIX_OWNER_PERM == 0
    /*  In this configuration, the Reliance Edge mode bits only store whether
        the file is a regular file or directory; the permission bits are unused.
        So we add hard-coded permissions here.
    */
    assert((uRedMode & RED_S_IALLUGO) == 0U);

    if(S_ISDIR(linuxMode))
    {
        linuxMode |= S_IXUSR | S_IXGRP | S_IXOTH;
    }

    /*  Always allow read access; allow write access if the file system is not
        readonly.
    */
    linuxMode |= S_IRUSR | S_IRGRP | S_IROTH;
  #if REDCONF_READ_ONLY == 0
    linuxMode |= S_IWUSR | S_IWGRP | S_IWOTH;
  #endif
  #endif /* REDCONF_POSIX_OWNER_PERM == 0 */

    return linuxMode;
}


/** @brief Translate a Reliance Edge errno into a POSIX errno.

    @param rederrno The Reliance Edge errno to translate.

    @return The POSIX errno equivalent of @p rederrno.
*/
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
        case RED_ELOOP:     return -ELOOP;
        case RED_ENODATA:   return -ENODATA;
        case RED_ENOLINK:   return -ENOLINK;
        case RED_EUSERS:    return -EUSERS;
        default:            return -EINVAL;    /* Not expected, but default to EINVAL */
    }
}


/** @brief Translate POSIX open flags to Reliance Edge open flags.

    POSIX open flags in @p flags which are not supported by Reliance Edge are
    ignored.

    @param flags    The POSIX open flags to translate.

    @return The Reliance Edge open flags equivalent to @p flags.
*/
static uint32_t flags_to_redflags(
    int         flags)
{
    uint32_t    ulRedFlags;

    if((flags & O_WRONLY) != 0)
    {
        ulRedFlags = RED_O_WRONLY;
    }
    else if((flags & O_RDWR) != 0)
    {
        ulRedFlags = RED_O_RDWR;
    }
    else
    {
        ulRedFlags = RED_O_RDONLY;
    }

    if((flags & O_CREAT) != 0)
    {
        ulRedFlags |= RED_O_CREAT;
    }

    if((flags & O_TRUNC) != 0)
    {
        ulRedFlags |= RED_O_TRUNC;
    }

    if((flags & O_EXCL) != 0)
    {
        ulRedFlags |= RED_O_EXCL;
    }

    if((flags & O_APPEND) != 0)
    {
        ulRedFlags |= RED_O_APPEND;
    }

    return ulRedFlags;
}


/** @brief Wrapper for red_open() or red_open2().

    @param pszPath  The path (provided by FUSE) to open.
    @param flags    The POSIX open flags.
    @param mode     The POSIX open mode (used if @p flags includes `O_CREAT` and
                    the file does not already exist).

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.
*/
static int32_t red_local_open(
    const char *pszPath,
    int         flags,
    mode_t      mode)
{
    char        szRedPath[PATH_MAX];
    int32_t     result;

    result = red_make_full_path(pszPath, szRedPath, sizeof(szRedPath));
    if(result != 0)
    {
        /*  This function mimics the POSIX-like API: returns 0 or -1 and sets
            red_errno on error.  ENAMETOOLONG and ENOMEM are the only error
            cases for red_make_full_path().
        */
        switch(result)
        {
            case -ENAMETOOLONG: red_errno = RED_ENAMETOOLONG; break;
            case -ENOMEM:       red_errno = RED_ENOMEM; break;
            default:            red_errno = RED_EINVAL; break;
        }

        result = -1;
    }
    else
    {
        /*  Open with RED_O_NOFOLLOW to provoke a RED_ELOOP error if the path
            names a symbolic link.
        */
        uint32_t ulOpenFlags = flags_to_redflags(flags) | RED_O_NOFOLLOW;

      #if REDCONF_POSIX_OWNER_PERM == 1
        result = red_open2(szRedPath, ulOpenFlags, (uint16_t)mode & RED_S_IALLUGO);
      #else
        (void)mode;
        result = red_open(szRedPath, ulOpenFlags);
      #endif

      #if REDCONF_API_POSIX_SYMLINK == 1
        /*  If the path names a symbolic link, this function needs to open a
            file descriptor for the symbolic link itself, not for what it points
            at.  This is required so that ->getattr() reports to FUSE that the
            path is a symbolic link.  Thus, if the RED_O_NOFOLLOW flag caused
            red_open()/red_open2() to fail with RED_ELOOP, then we need to use
            RED_O_SYMLINK to open the symlink itself.

            This might fail with RED_ELOOP again, if the error was caused by a
            symbolic link loop rather than RED_O_NOFOLLOW, but that's fine.
        */
        if((result == -1) && (red_errno == RED_ELOOP))
        {
            ulOpenFlags &= ~RED_O_NOFOLLOW;
            ulOpenFlags |= RED_O_SYMLINK;
            result = red_openat(RED_AT_FDNONE, szRedPath, ulOpenFlags, (uint16_t)mode & RED_S_IALLUGO);
        }
      #endif
    }

    return result;
}


/** @brief Make a full path by adding the Reliance Edge volume name.

    If #REDCONF_PATH_SEPARATOR is not '/', then '/' characters in @p pszPath are
    replaced with #REDCONF_PATH_SEPARATOR.

    @param pszPath      The path provided by FUSE.
    @param pszBuffer    The output buffer for the full path.
    @param nBufferSize  The size of @p pszBuffer in bytes.

    @return Returns 0 on success or a negative errno value on error.

    @retval -ENAMETOOLONG   @p pszBuffer is too small.
    @retval -ENOMEM         Memory allocation failure.
*/
static int red_make_full_path(
    const char *pszPath,
    char       *pszBuffer,
    size_t      nBufferSize)
{
    int         result = 0;
    const char  szPathSep[] = { REDCONF_PATH_SEPARATOR, '\0' };
    const char *pszPathSep = szPathSep;
  #if REDCONF_PATH_SEPARATOR != '/'
    char       *pszPathRedfs = string_copy_replace(pszPath, '/', REDCONF_PATH_SEPARATOR);

    if(pszPathRedfs == NULL)
    {
        return -ENOMEM;
    }

    pszPath = pszPathRedfs;
  #endif

    /*  Don't add a redundant path separator, for aesthetic reasons.
    */
    if(*pszPath == REDCONF_PATH_SEPARATOR)
    {
        pszPathSep = "";
    }

    if(snprintf(pszBuffer, nBufferSize, "%s%s%s", gpszVolume, pszPathSep, pszPath) >= nBufferSize)
    {
        fprintf(stderr, "Error: path too long (%s%s%s)\n", gpszVolume, pszPathSep, pszPath);
        result = -ENAMETOOLONG;
    }

  #if REDCONF_PATH_SEPARATOR != '/'
    free(pszPathRedfs);
  #endif

    return result;
}


#if REDCONF_PATH_SEPARATOR != '/'
/** @brief Copy a string and replace all occurrences of a character.

    @param pszStr   The string to copy.
    @param cFind    The character to be replaced by @p cReplace.
    @param cReplace The replacement character for @p cFind.

    @return Returns an allocated copy of @p pszStr, with all @p cFind characters
            replaced by @p cReplace characters.  If memory allocation fails,
            returns `NULL`.
*/
static char *string_copy_replace(
    const char *pszStr,
    char        cFind,
    char        cReplace)
{
    char       *pszStrNew = strdup(pszStr);

    if(pszStrNew != NULL)
    {
        string_replace(pszStrNew, SIZE_MAX, cFind, cReplace);
    }

    return pszStrNew;
}


/** @brief Replace all occurrences of a character with another in a string.

    @param pszStr   The string in which @p cFind is to be replaced.
    @param nMax     Maximum number of bytes from @p pszStr to process.  For
                    strings which are not guaranteed to be NUL-terminated.
    @param cFind    The character to be replaced by @p cReplace.
    @param cReplace The replacement character for @p cFind.
*/
static void string_replace(
    char   *pszStr,
    size_t  nMax,
    char    cFind,
    char    cReplace)
{
    size_t  nIdx = 0U;

    while((nIdx < nMax) && (pszStr[nIdx] != '\0'))
    {
        if(pszStr[nIdx] == cFind)
        {
            pszStr[nIdx] = cReplace;
        }

        nIdx++;
    }
}
#endif /* REDCONF_PATH_SEPARATOR != '/' */

#else /* REDCONF_API_POSIX */

int main(void)
{
    fprintf(stderr, "Reliance Edge FUSE port does not support the FSE API.\n");
    return 1;
}

#endif /* REDCONF_API_POSIX */
