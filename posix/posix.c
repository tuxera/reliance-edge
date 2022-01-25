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
    @brief Implementation of the Reliance Edge POSIX-like API.
*/
#include <redfs.h>

#if REDCONF_API_POSIX == 1

/** @defgroup red_group_posix The POSIX-like File System Interface
    @{
*/

#include <redvolume.h>
#include <redcoreapi.h>
#include <redposix.h>
#include <redpath.h>


/*-------------------------------------------------------------------
    File Descriptors
-------------------------------------------------------------------*/

#define FD_GEN_BITS 11U /* File descriptor bits for mount generation. */
#define FD_VOL_BITS 8U  /* File descriptor bits for volume number. */
#define FD_IDX_BITS 12U /* File descriptor bits for handle index. */

/*  31 bits available: file descriptors are int32_t, but the sign bit must
    always be zero.
*/
#if (FD_GEN_BITS + FD_VOL_BITS + FD_IDX_BITS) > 31U
  #error "Internal error: too many file descriptor bits!"
#endif

/*  Maximum values for file descriptor components.
*/
#define FD_GEN_MAX  ((1UL << FD_GEN_BITS) - 1U)
#define FD_VOL_MAX  ((1UL << FD_VOL_BITS) - 1U)
#define FD_IDX_MAX  ((1UL << FD_IDX_BITS) - 1U)

#if REDCONF_VOLUME_COUNT > FD_VOL_MAX
  #error "Error: Too many file system volumes!"
#endif
#if REDCONF_HANDLE_COUNT > (FD_IDX_MAX + 1U)
  #error "Error: Too many file system handles!"
#endif

/*  File descriptors must never be negative; and must never be zero, one, or
    two, to avoid confusion with STDIN, STDOUT, and STDERR.
*/
#define FD_MIN  (3)

/*-------------------------------------------------------------------
    Handles
-------------------------------------------------------------------*/

/*  Mask of all RED_O_* values.
*/
#define RED_O_MASK  (RED_O_RDONLY|RED_O_WRONLY|RED_O_RDWR|RED_O_APPEND|RED_O_CREAT|RED_O_EXCL|RED_O_TRUNC)

#define HFLAG_DIRECTORY 0x01U   /* Handle is for a directory. */
#define HFLAG_READABLE  0x02U   /* Handle is readable. */
#define HFLAG_WRITEABLE 0x04U   /* Handle is writeable. */
#define HFLAG_APPENDING 0x08U   /* Handle was opened in append mode. */

/*  @brief Handle structure, used to implement file descriptors and directory
           streams.
*/
typedef struct sREDHANDLE
{
    uint32_t        ulInode;    /**< Inode number; 0 if handle is available. */
    uint8_t         bVolNum;    /**< Volume containing the inode. */
    uint8_t         bFlags;     /**< Handle flags (type and mode). */
    union
    {
        uint64_t ullFileOffset;
      #if REDCONF_API_POSIX_READDIR == 1
        uint32_t ulDirPosition;
      #endif
    }               o;          /**< File or directory offset. */
  #if REDCONF_API_POSIX_READDIR == 1
    REDDIRENT       dirent;     /**< Dirent structure returned by red_readdir(). */
  #endif
} REDHANDLE;

/*-------------------------------------------------------------------
    Tasks
-------------------------------------------------------------------*/

#if REDCONF_API_POSIX_CWD == 1
/*  @brief Current working directory.
*/
typedef struct
{
    uint8_t     bVolNum;    /**< Volume containing the CWD. */
    uint32_t    ulInode;    /**< Inode number of CWD. */
} WORKDIR;
#endif

#if REDCONF_TASK_COUNT > 1U
/*  @brief Per-task information.
*/
typedef struct
{
    uint32_t    ulTaskId;   /**< ID of the task which owns this slot; 0 if free. */
    REDSTATUS   iErrno;     /**< Last error value. */
  #if REDCONF_API_POSIX_CWD == 1
    WORKDIR     cwd;        /**< Current working directory. */
  #endif
} TASKSLOT;
#endif

/*-------------------------------------------------------------------
    Local Prototypes
-------------------------------------------------------------------*/

#if (REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1))
static REDSTATUS UnlinkSub(const char *pszPath, FTYPE type);
#endif
static REDSTATUS PathStartingPoint(const char *pszPath, uint8_t *pbVolNum, uint32_t *pulCwdInode, const char **ppszLocalPath);
static REDSTATUS FildesOpen(const char *pszPath, uint32_t ulOpenMode, FTYPE type, int32_t *piFildes);
static REDSTATUS FildesClose(int32_t iFildes);
static REDSTATUS FildesToHandle(int32_t iFildes, FTYPE expectedType, REDHANDLE **ppHandle);
static int32_t FildesPack(uint16_t uHandleIdx, uint8_t bVolNum);
static void FildesUnpack(int32_t iFildes, uint16_t *puHandleIdx, uint8_t *pbVolNum, uint16_t *puGeneration);
#if REDCONF_API_POSIX_READDIR == 1
static bool DirStreamIsValid(const REDDIR *pDirStream);
#endif
static REDSTATUS PosixEnter(void);
static void PosixLeave(void);
static REDSTATUS ModeTypeCheck(uint16_t uMode, FTYPE expectedType);
#if (REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1) || ((REDCONF_API_POSIX_RENAME == 1) && (REDCONF_RENAME_ATOMIC == 1)))
static REDSTATUS InodeUnlinkCheck(uint32_t ulInode);
#endif
#if REDCONF_TASK_COUNT > 1U
static REDSTATUS TaskRegister(uint32_t *pulTaskIdx);
#endif
#if REDCONF_API_POSIX_CWD == 1
static WORKDIR *CwdGet(void);
static void CwdResetVol(uint8_t bVolNum);
static void CwdResetAll(void);
#endif
static int32_t PosixReturn(REDSTATUS iError);

/*-------------------------------------------------------------------
    Globals
-------------------------------------------------------------------*/

static bool gfPosixInited;                              /* Whether driver is initialized. */
static REDHANDLE gaHandle[REDCONF_HANDLE_COUNT];        /* Array of all handles. */
#if REDCONF_TASK_COUNT > 1U
static TASKSLOT gaTask[REDCONF_TASK_COUNT];             /* Array of task slots. */
#endif
#if (REDCONF_TASK_COUNT == 1U) && (REDCONF_API_POSIX_CWD == 1)
static WORKDIR gCwd;                                    /* Current working directory. */
#endif

/*  Array of volume mount "generations".  These are incremented for a volume
    each time that volume is mounted.  The generation number (along with the
    volume number) is incorporated into the file descriptors; a stale file
    descriptor from a previous mount can be detected since it will include a
    stale generation number.
*/
static uint16_t gauGeneration[REDCONF_VOLUME_COUNT];


/*-------------------------------------------------------------------
    Public API
-------------------------------------------------------------------*/

/** @brief Initialize the Reliance Edge file system driver.

    Prepares the Reliance Edge file system driver to be used.  Must be the first
    Reliance Edge function to be invoked: no volumes can be mounted or formatted
    until the driver has been initialized.

    If this function is called when the Reliance Edge driver is already
    initialized, it does nothing and returns success.

    This function is not thread safe: attempting to initialize from multiple
    threads could leave things in a bad state.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EINVAL: The volume path prefix configuration is invalid.
*/
int32_t red_init(void)
{
    REDSTATUS ret;

    if(gfPosixInited)
    {
        ret = 0;
    }
    else
    {
        ret = RedCoreInit();
        if(ret == 0)
        {
            RedMemSet(gaHandle, 0U, sizeof(gaHandle));

          #if REDCONF_TASK_COUNT > 1U
            RedMemSet(gaTask, 0U, sizeof(gaTask));
          #endif

          #if REDCONF_API_POSIX_CWD == 1
            CwdResetAll();
          #endif

            gfPosixInited = true;
        }
    }

    return PosixReturn(ret);
}


/** @brief Uninitialize the Reliance Edge file system driver.

    Tears down the Reliance Edge file system driver.  Cannot be used until all
    Reliance Edge volumes are unmounted.  A subsequent call to red_init() will
    initialize the driver again.

    If this function is called when the Reliance Edge driver is already
    uninitialized, it does nothing and returns success.

    This function is not thread safe: attempting to uninitialize from multiple
    threads could leave things in a bad state.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBUSY: At least one volume is still mounted.
*/
int32_t red_uninit(void)
{
    REDSTATUS ret;

    if(gfPosixInited)
    {
        ret = PosixEnter();

        if(ret == 0)
        {
            uint8_t bVolNum;

            for(bVolNum = 0U; bVolNum < REDCONF_VOLUME_COUNT; bVolNum++)
            {
                if(gaRedVolume[bVolNum].fMounted)
                {
                    ret = -RED_EBUSY;
                    break;
                }
            }

            if(ret == 0)
            {
                /*  All volumes are unmounted.  Mark the driver as
                    uninitialized before releasing the FS mutex, to avoid any
                    race condition where a volume could be mounted and then the
                    driver uninitialized with a mounted volume.
                */
                gfPosixInited = false;
            }

            /*  The FS mutex must be released before we uninitialize the core,
                since the FS mutex needs to be in the released state when it
                gets uninitialized.

                Don't use PosixLeave(), since it asserts gfPosixInited is true.
            */
          #if REDCONF_TASK_COUNT > 1U
            RedOsMutexRelease();
          #endif
        }

        if(ret == 0)
        {
            ret = RedCoreUninit();

            /*  Not good if the above fails, since things might be partly, but
                not entirely, torn down, and there might not be a way back to
                a valid driver state.
            */
            REDASSERT(ret == 0);
        }
    }
    else
    {
        ret = 0;
    }

    return PosixReturn(ret);
}


#if REDCONF_READ_ONLY == 0
/** @brief Commits file system updates.

    Commits all changes on all file system volumes to permanent storage.  This
    function will not return until the operation is complete.

    If sync automatic transactions have been disabled for one or more volumes,
    this function does not commit changes to those volumes, but will still
    commit changes to any volumes for which automatic transactions are enabled.

    If sync automatic transactions have been disabled on all volumes, this
    function does nothing and returns success.

    @return On success, zero is returned.  On error, -1 is returned and
        #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EIO: I/O error during the transaction point.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_sync(void)
{
    REDSTATUS ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        uint8_t bVolNum;

        for(bVolNum = 0U; bVolNum < REDCONF_VOLUME_COUNT; bVolNum++)
        {
            if(gaRedVolume[bVolNum].fMounted && !gaRedVolume[bVolNum].fReadOnly)
            {
                REDSTATUS err;

              #if REDCONF_VOLUME_COUNT > 1U
                err = RedCoreVolSetCurrent(bVolNum);

                if(err == 0)
              #endif
                {
                    uint32_t ulTransMask;

                    err = RedCoreTransMaskGet(&ulTransMask);

                    if((err == 0) && ((ulTransMask & RED_TRANSACT_SYNC) != 0U))
                    {
                        err = RedCoreVolTransact();
                    }
                }

                if(err != 0)
                {
                    ret = err;
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif


/** @brief Mount a file system volume.

    Prepares the file system volume to be accessed.  Mount will fail if the
    volume has never been formatted, or if the on-disk format is inconsistent
    with the compile-time configuration.

    An error is returned if the volume is already mounted.

    @param pszVolume    A path prefix identifying the volume to mount.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBUSY: Volume is already mounted.
    - #RED_EINVAL: @p pszVolume is `NULL`; or the driver is uninitialized.
    - #RED_EIO: Volume not formatted, improperly formatted, or corrupt.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_mount(
    const char *pszVolume)
{
    return red_mount2(pszVolume, RED_MOUNT_DEFAULT);
}


/** @brief Mount a file system volume with flags.

    Prepares the file system volume to be accessed.  Mount will fail if the
    volume has never been formatted, or if the on-disk format is inconsistent
    with the compile-time configuration.

    An error is returned if the volume is already mounted.

    The following mount flags are available:

    - #RED_MOUNT_READONLY: If specified, the volume will be mounted read-only.
      All write operations with fail, setting #red_errno to #RED_EROFS.
    - #RED_MOUNT_DISCARD: If specified, and if the underlying block device
      supports discards, discards will be issued for blocks that become free.
      If the underlying block device does _not_ support discards, then this
      flag has no effect.

    The #RED_MOUNT_DEFAULT macro can be used to mount with the default mount
    flags, which is equivalent to mounting with red_mount().

    @param pszVolume    A path prefix identifying the volume to mount.
    @param ulFlags      A bitwise-OR'd mask of mount flags.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBUSY: Volume is already mounted.
    - #RED_EINVAL: @p pszVolume is `NULL`; or the driver is uninitialized; or
      @p ulFlags includes invalid mount flags.
    - #RED_EIO: Volume not formatted, improperly formatted, or corrupt.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_mount2(
    const char *pszVolume,
    uint32_t    ulFlags)
{
    REDSTATUS   ret;

    ret = PosixEnter();

    if(ret == 0)
    {
        ret = RedPathVolumeLookup(pszVolume, NULL);

        /*  The core will return success if the volume is already mounted, so
            check for that condition here to propagate the error.
        */
        if((ret == 0) && gpRedVolume->fMounted)
        {
            ret = -RED_EBUSY;
        }

        if(ret == 0)
        {
            ret = RedCoreVolMount(ulFlags);
        }

        if(ret == 0)
        {
            /*  Increment the mount generation, invalidating file descriptors
                from previous mounts.  Note that while the generation numbers
                are stored in 16-bit values, we have less than 16-bits to store
                generations in the file descriptors, so we must wrap-around
                manually.
            */
            gauGeneration[gbRedVolNum]++;
            if(gauGeneration[gbRedVolNum] > FD_GEN_MAX)
            {
                /*  Wrap-around to one, rather than zero.  The generation is
                    stored in the top bits of the file descriptor, and doing
                    this means that low numbers are never valid file
                    descriptors.  This implements the requirement that 0, 1,
                    and 2 are never valid file descriptors, thereby avoiding
                    confusion with STDIN, STDOUT, and STDERR.
                */
                gauGeneration[gbRedVolNum] = 1U;
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


/** @brief Unmount a file system volume.

    This function discards the in-memory state for the file system and marks it
    as unmounted.  Subsequent attempts to access the volume will fail until the
    volume is mounted again.

    If unmount automatic transaction points are enabled, this function will
    commit a transaction point prior to unmounting.  If unmount automatic
    transaction points are disabled, this function will unmount without
    transacting, effectively discarding the working state.

    Before unmounting, this function will wait for any active file system
    thread to complete by acquiring the FS mutex.  The volume will be marked as
    unmounted before the FS mutex is released, so subsequent FS threads will
    possibly block and then see an error when attempting to access a volume
    which is unmounting or unmounted.  If the volume has open handles, the
    unmount will fail.

    An error is returned if the volume is already unmounted.

    @param pszVolume    A path prefix identifying the volume to unmount.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBUSY: There are still open handles for this file system volume.
    - #RED_EINVAL: @p pszVolume is `NULL`; or the driver is uninitialized; or
      the volume is already unmounted.
    - #RED_EIO: I/O error during unmount automatic transaction point.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_umount(
    const char *pszVolume)
{
    return red_umount2(pszVolume, RED_UMOUNT_DEFAULT);
}


/** @brief Unmount a file system volume with flags.

    This function is the same as red_umount(), except that it accepts a flags
    parameter which can change the unmount behavior.

    The following unmount flags are available:

    - #RED_UMOUNT_FORCE: If specified, if the volume has open handles, the
      handles will be closed.  Without this flag, the behavior is to return an
      #RED_EBUSY error if the volume has open handles.

    The #RED_UMOUNT_DEFAULT macro can be used to unmount with the default
    unmount flags, which is equivalent to unmounting with red_umount().

    @param pszVolume    A path prefix identifying the volume to unmount.
    @param ulFlags      A bitwise-OR'd mask of unmount flags.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBUSY: There are still open handles for this file system volume and
      the #RED_UMOUNT_FORCE flag was _not_ specified.
    - #RED_EINVAL: @p pszVolume is `NULL`; or @p ulFlags includes invalid
      unmount flags; or the driver is uninitialized; or the volume is already
      unmounted.
    - #RED_EIO: I/O error during unmount automatic transaction point.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_umount2(
    const char *pszVolume,
    uint32_t    ulFlags)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        if(ulFlags != (ulFlags & RED_UMOUNT_MASK))
        {
            ret = -RED_EINVAL;
        }
        else
        {
            ret = RedPathVolumeLookup(pszVolume, NULL);
        }

        /*  The core will return success if the volume is already unmounted, so
            check for that condition here to propagate the error.
        */
        if((ret == 0) && !gpRedVolume->fMounted)
        {
            ret = -RED_EINVAL;
        }

        if(ret == 0)
        {
            uint16_t    uHandleIdx;

            /*  If the volume has open handles, return an error -- unless the
                force flag was specified, in which case all open handles are
                closed.
            */
            for(uHandleIdx = 0U; uHandleIdx < REDCONF_HANDLE_COUNT; uHandleIdx++)
            {
                REDHANDLE *pHandle = &gaHandle[uHandleIdx];

                if((pHandle->ulInode != INODE_INVALID) && (pHandle->bVolNum == gbRedVolNum))
                {
                    if((ulFlags & RED_UMOUNT_FORCE) != 0U)
                    {
                        pHandle->ulInode = INODE_INVALID;
                    }
                    else
                    {
                        ret = -RED_EBUSY;
                        break;
                    }
                }
            }
        }

        if(ret == 0)
        {
            ret = RedCoreVolUnmount();
        }

      #if REDCONF_API_POSIX_CWD == 1
        /*  Reset the CWD for any task whose CWD was on the unmounted volume.
        */
        if(ret == 0)
        {
            CwdResetVol(gbRedVolNum);
        }
      #endif

        PosixLeave();
    }

    return PosixReturn(ret);
}


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FORMAT == 1)
/** @brief Format a file system volume.

    Uses the statically defined volume configuration.  After calling this
    function, the volume needs to be mounted -- see red_mount().

    An error is returned if the volume is mounted.

    @param pszVolume    A path prefix identifying the volume to format.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBUSY: Volume is mounted.
    - #RED_EINVAL: @p pszVolume is `NULL`; or the driver is uninitialized.
    - #RED_EIO: I/O error formatting the volume.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_format(
    const char *pszVolume)
{
    return red_format2(pszVolume, NULL);
}


/** @brief Format a file system volume with options.

    This function is the same as red_format(), except that it accepts an options
    parameter which can change the on-disk layout version and which, in the
    future, may allow other aspects of the metadata to be specified at run-time.

    Since new members may be added to ::REDFMTOPT, applications should
    zero-initialize the structure to ensure forward compatibility.  For example:

    @code{.c}
    REDFMTOPT fmtopt = {0U};

    fmtopt.ulVersion = RED_DISK_LAYOUT_ORIGINAL;
    ret = red_format2("VOL0:", &fmtopt);
    @endcode

    @param pszVolume    A path prefix identifying the volume to format.
    @param pOptions     Format options.  May be `NULL`, in which case the default
                        values are used for the options, equivalent to
                        red_format().  If non-`NULL`, the caller should
                        zero-initialize the structure to ensure forward
                        compatibility in the event that additional members are
                        added to the ::REDFMTOPT structure.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBUSY: Volume is mounted.
    - #RED_EINVAL: @p pszVolume is `NULL`; or the driver is uninitialized.
    - #RED_EIO: I/O error formatting the volume.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_format2(
    const char         *pszVolume,
    const REDFMTOPT    *pOptions)
{
    REDSTATUS           ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        ret = RedPathVolumeLookup(pszVolume, NULL);

        if(ret == 0)
        {
            ret = RedCoreVolFormat(pOptions);
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FORMAT == 1) */


#if REDCONF_READ_ONLY == 0
/** @brief Commit a transaction point.

    Reliance Edge is a transactional file system.  All modifications, of both
    metadata and filedata, are initially working state.  A transaction point
    is a process whereby the working state atomically becomes the committed
    state, replacing the previous committed state.  Whenever Reliance Edge is
    mounted, including after power loss, the state of the file system after
    mount is the most recent committed state.  Nothing from the committed
    state is ever missing, and nothing from the working state is ever included.

    @param pszVolume    A path prefix identifying the volume to transact.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EINVAL: Volume is not mounted; or @p pszVolume is `NULL`.
    - #RED_EIO: I/O error during the transaction point.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EUSERS: Cannot become a file system user: too many users.
    - #RED_EROFS: The file system volume is read-only.
*/
int32_t red_transact(
    const char *pszVolume)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        ret = RedPathVolumeLookup(pszVolume, NULL);

        if(ret == 0)
        {
            ret = RedCoreVolTransact();
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


/** @brief Rollback to the previous transaction point.

    Reliance Edge is a transactional file system.  All modifications, of both
    metadata and filedata, are initially working state.  A transaction point is
    a process whereby the working state atomically becomes the committed state,
    replacing the previous committed state.  This call cancels all modifications
    in the working state and reverts to the last committed state.  In other
    words, calling this function will discard all changes made to the file
    system since the most recent transaction point.

    @param pszVolume    A path prefix identifying the volume to rollback.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBUSY: There are still open handles for this file system volume.
    - #RED_EINVAL: Volume is not mounted; or @p pszVolume is `NULL`.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EROFS: The file system volume is read-only.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_rollback(
    const char *pszVolume)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        uint8_t bVolNum;

        ret = RedPathVolumeLookup(pszVolume, &bVolNum);

        if(ret == 0)
        {
            uint16_t    uHandleIdx;

            /*  Do not rollback the volume if it still has open handles.
            */
            for(uHandleIdx = 0U; uHandleIdx < REDCONF_HANDLE_COUNT; uHandleIdx++)
            {
                const REDHANDLE *pHandle = &gaHandle[uHandleIdx];

                if((pHandle->ulInode != INODE_INVALID) && (pHandle->bVolNum == bVolNum))
                {
                    ret = -RED_EBUSY;
                    break;
                }
            }
        }

        if(ret == 0)
        {
            ret = RedCoreVolRollback();
        }

      #if REDCONF_API_POSIX_CWD == 1
        /*  After reverting to the committed state, it's possible that the
            working directories on this volume have ceased to exist.  To avoid
            unexpected behavior, reset the CWD for any task whose CWD was on the
            volume which was rolled back.
        */
        if(ret == 0)
        {
            CwdResetVol(bVolNum);
        }
      #endif

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif /* REDCONF_READ_ONLY == 0 */


#if REDCONF_READ_ONLY == 0
/** @brief Update the transaction mask.

    The following events are available:

    - #RED_TRANSACT_SYNC
    - #RED_TRANSACT_UMOUNT
    - #RED_TRANSACT_CREAT
    - #RED_TRANSACT_UNLINK
    - #RED_TRANSACT_MKDIR
    - #RED_TRANSACT_RENAME
    - #RED_TRANSACT_LINK
    - #RED_TRANSACT_CLOSE
    - #RED_TRANSACT_WRITE
    - #RED_TRANSACT_FSYNC
    - #RED_TRANSACT_TRUNCATE
    - #RED_TRANSACT_VOLFULL

    The #RED_TRANSACT_MANUAL macro (by itself) may be used to disable all
    automatic transaction events.  The #RED_TRANSACT_MASK macro is a bitmask
    of all transaction flags, excluding those representing excluded
    functionality.

    Attempting to enable events for excluded functionality will result in an
    error.

    @param pszVolume    The path prefix of the volume whose transaction mask is
                        being changed.
    @param ulEventMask  A bitwise-OR'd mask of automatic transaction events to
                        be set as the current transaction mode.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EINVAL: Volume is not mounted; or @p pszVolume is `NULL`; or
      @p ulEventMask contains invalid bits.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_settransmask(
    const char *pszVolume,
    uint32_t    ulEventMask)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        ret = RedPathVolumeLookup(pszVolume, NULL);

        if(ret == 0)
        {
            ret = RedCoreTransMaskSet(ulEventMask);
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif


/** @brief Read the transaction mask.

    If the volume is read-only, the returned event mask is always zero.

    @param pszVolume    The path prefix of the volume whose transaction mask is
                        being retrieved.
    @param pulEventMask Populated with a bitwise-OR'd mask of automatic
                        transaction events which represent the current
                        transaction mode for the volume.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EINVAL: Volume is not mounted; or @p pszVolume is `NULL`; or
      @p pulEventMask is `NULL`.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_gettransmask(
    const char *pszVolume,
    uint32_t   *pulEventMask)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        ret = RedPathVolumeLookup(pszVolume, NULL);

        if(ret == 0)
        {
            ret = RedCoreTransMaskGet(pulEventMask);
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


/** @brief Query file system status information.

    @p pszVolume should name a valid volume prefix or a valid root directory;
    this differs from POSIX statvfs, where any existing file or directory is a
    valid path.

    @param pszVolume    The path prefix of the volume to query.
    @param pStatvfs     The buffer to populate with volume information.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EINVAL: Volume is not mounted; or @p pszVolume is `NULL`; or
      @p pStatvfs is `NULL`.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_statvfs(
    const char *pszVolume,
    REDSTATFS  *pStatvfs)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        ret = RedPathVolumeLookup(pszVolume, NULL);

        if(ret == 0)
        {
            ret = RedCoreVolStat(pStatvfs);
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


/** @brief Open a file or directory.

    Exactly one file access mode must be specified:

    - #RED_O_RDONLY: Open for reading only.
    - #RED_O_WRONLY: Open for writing only.
    - #RED_O_RDWR: Open for reading and writing.

    Directories can only be opened with `RED_O_RDONLY`.

    The following flags may also be used:

    - #RED_O_APPEND: Set the file offset to the end-of-file prior to each
      write.
    - #RED_O_CREAT: Create the named file if it does not exist.
    - #RED_O_EXCL: In combination with `RED_O_CREAT`, return an error if the
      path already exists.
    - #RED_O_TRUNC: Truncate the opened file to size zero.  Only supported when
      #REDCONF_API_POSIX_FTRUNCATE is true.

    #RED_O_CREAT, #RED_O_EXCL, and #RED_O_TRUNC are invalid with #RED_O_RDONLY.
    #RED_O_EXCL is invalid without #RED_O_CREAT.

    If the volume is read-only, #RED_O_RDONLY is the only valid open flag; use
    of any other flag will result in an error.

    If #RED_O_TRUNC frees data which is in the committed state, it will not
    return to free space until after a transaction point.

    The returned file descriptor must later be closed with red_close().

    Unlike POSIX open, there is no optional third argument for the permissions
    (which Reliance Edge does not use) and other open flags (like `O_SYNC`) are
    not supported.

    @param pszPath      The path to the file or directory.
    @param ulOpenMode   The open flags (mask of `RED_O_` values).

    @return On success, a nonnegative file descriptor is returned.  On error,
            -1 is returned and #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EEXIST: Using #RED_O_CREAT and #RED_O_EXCL, and the indicated path
      already exists.
    - #RED_EINVAL: @p ulOpenMode is invalid; or @p pszPath is `NULL`; or the
      volume containing the path is not mounted; or #RED_O_CREAT is included in
      @p ulOpenMode, and the path ends with dot or dot-dot.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EISDIR: The path names a directory and @p ulOpenMode includes
      #RED_O_WRONLY or #RED_O_RDWR.
    - #RED_EMFILE: There are no available file descriptors.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENFILE: Attempting to create a file but the file system has used all
      available inode slots.
    - #RED_ENOENT: #RED_O_CREAT is not set and the named file does not exist; or
      #RED_O_CREAT is set and the parent directory does not exist; or the
      volume does not exist; or the @p pszPath argument points to an empty
      string (and there is no volume with an empty path prefix).
    - #RED_ENOSPC: The file does not exist and #RED_O_CREAT was specified, but
      there is insufficient free space to expand the directory or to create the
      new file.
    - #RED_ENOTDIR: A component of the prefix in @p pszPath does not name a
      directory.
    - #RED_EROFS: The path resides on a read-only file system and a write
      operation was requested.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_open(
    const char *pszPath,
    uint32_t    ulOpenMode)
{
    int32_t     iFildes = -1;   /* Init'd to quiet warnings. */
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
      #if REDCONF_READ_ONLY == 1
        if(ulOpenMode != RED_O_RDONLY)
        {
            ret = -RED_EROFS;
        }
      #else
        if(    (ulOpenMode != (ulOpenMode & RED_O_MASK))
            || ((ulOpenMode & (RED_O_RDONLY|RED_O_WRONLY|RED_O_RDWR)) == 0U)
            || (((ulOpenMode & RED_O_RDONLY) != 0U) && ((ulOpenMode & (RED_O_WRONLY|RED_O_RDWR)) != 0U))
            || (((ulOpenMode & RED_O_WRONLY) != 0U) && ((ulOpenMode & (RED_O_RDONLY|RED_O_RDWR)) != 0U))
            || (((ulOpenMode & RED_O_RDWR) != 0U) && ((ulOpenMode & (RED_O_RDONLY|RED_O_WRONLY)) != 0U))
            || (((ulOpenMode & (RED_O_TRUNC|RED_O_CREAT|RED_O_EXCL)) != 0U) && ((ulOpenMode & RED_O_RDONLY) != 0U))
            || (((ulOpenMode & RED_O_EXCL) != 0U) && ((ulOpenMode & RED_O_CREAT) == 0U)))
        {
            ret = -RED_EINVAL;
        }
      #if REDCONF_API_POSIX_FTRUNCATE == 0
        else if((ulOpenMode & RED_O_TRUNC) != 0U)
        {
            ret = -RED_EINVAL;
        }
      #endif
      #endif
        else
        {
            ret = FildesOpen(pszPath, ulOpenMode, FTYPE_EITHER, &iFildes);
        }

        PosixLeave();
    }

    if(ret != 0)
    {
        iFildes = PosixReturn(ret);
    }

    return iFildes;
}


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_UNLINK == 1)
/** @brief Delete a file or directory.

    The given name is deleted and the link count of the corresponding inode is
    decremented.  If the link count falls to zero (no remaining hard links),
    the inode will be deleted.

    Unlike POSIX unlink, deleting a file or directory with open handles (file
    descriptors or directory streams) will fail with an #RED_EBUSY error.  This
    only applies when deleting an inode with a link count of one; if a file has
    multiple names (hard links), all but the last name may be deleted even if
    the file is open.

    If the path names a directory which is not empty, the unlink will fail.

    If the deletion frees data in the committed state, it will not return to
    free space until after a transaction point.

    Unlike POSIX unlink, this function can fail when the disk is full.  To fix
    this, transact and try again: Reliance Edge guarantees that it is possible
    to delete at least one file or directory after a transaction point.  If disk
    full automatic transactions are enabled, this will happen automatically.

    @param pszPath  The path of the file or directory to delete.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBUSY: @p pszPath names the root directory; or @p pszPath points to
      an inode with open handles and a link count of one; or
      #REDCONF_API_POSIX_CWD is true and @p pszPath points to the CWD of a task.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or #REDCONF_API_POSIX_CWD is true and the path ends with dot
      or dot-dot.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: The path does not name an existing file; or the @p pszPath
      argument points to an empty string (and there is no volume with an empty
      path prefix).
    - #RED_ENOTDIR: A component of the path prefix is not a directory.
    - #RED_ENOTEMPTY: The path names a directory which is not empty.
    - #RED_ENOSPC: The file system does not have enough space to modify the
      parent directory to perform the deletion.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_unlink(
    const char *pszPath)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        ret = UnlinkSub(pszPath, FTYPE_EITHER);

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_MKDIR == 1)
/** @brief Create a new directory.

    Unlike POSIX mkdir, this function has no second argument for the
    permissions (which Reliance Edge does not use).

    @param pszPath  The name and location of the directory to create.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EEXIST: @p pszPath points to an existing file or directory.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or the path ends with dot or dot-dot.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of the path prefix does not name an existing
      directory; or the @p pszPath argument points to an empty string (and there
      is no volume with an empty path prefix).
    - #RED_ENOSPC: The file system does not have enough space for the new
      directory or to extend the parent directory of the new directory.
    - #RED_ENOTDIR: A component of the path prefix is not a directory.
    - #RED_EROFS: The parent directory resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_mkdir(
    const char *pszPath)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        uint32_t    ulCwdInode;
        const char *pszLocalPath;

        ret = PathStartingPoint(pszPath, NULL, &ulCwdInode, &pszLocalPath);
        if(ret == 0)
        {
            const char *pszName;
            uint32_t    ulPInode;

            ret = RedPathToName(ulCwdInode, pszLocalPath, -RED_EEXIST, &ulPInode, &pszName);
            if(ret == 0)
            {
                uint32_t ulInode;

                ret = RedCoreCreate(ulPInode, pszName, true, &ulInode);
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_RMDIR == 1)
/** @brief Delete a directory.

    The given directory name is deleted and the corresponding directory inode
    will be deleted.

    Unlike POSIX rmdir, deleting a directory with open handles (file
    descriptors or directory streams) will fail with an #RED_EBUSY error.

    If the path names a directory which is not empty, the deletion will fail.
    If the path names the root directory of a file system volume, the deletion
    will fail.

    If the path names a regular file, the deletion will fail.  This provides
    type checking and may be useful in cases where an application knows the
    path to be deleted should name a directory.

    If the deletion frees data in the committed state, it will not return to
    free space until after a transaction point.

    Unlike POSIX rmdir, this function can fail when the disk is full.  To fix
    this, transact and try again: Reliance Edge guarantees that it is possible
    to delete at least one file or directory after a transaction point.  If disk
    full automatic transactions are enabled, this will happen automatically.

    @param pszPath  The path of the directory to delete.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBUSY: @p pszPath names the root directory; or @p pszPath points to a
      directory with open handles; or #REDCONF_API_POSIX_CWD is true and
      @p pszPath points to the CWD of a task.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or #REDCONF_API_POSIX_CWD is true and the path ends with dot
      or dot-dot.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: The path does not name an existing directory; or the
      @p pszPath argument points to an empty string (and there is no volume with
      an empty path prefix).
    - #RED_ENOTDIR: A component of the path is not a directory.
    - #RED_ENOTEMPTY: The path names a directory which is not empty.
    - #RED_ENOSPC: The file system does not have enough space to modify the
      parent directory to perform the deletion.
    - #RED_EROFS: The directory to be removed resides on a read-only file
      system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_rmdir(
    const char *pszPath)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        ret = UnlinkSub(pszPath, FTYPE_DIR);

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_RENAME == 1)
/** @brief Rename a file or directory.

    Both paths must reside on the same file system volume.  Attempting to use
    this API to move a file to a different volume will result in an error.

    If @p pszNewPath names an existing file or directory, the behavior depends
    on the configuration.  If #REDCONF_RENAME_ATOMIC is false, and if the
    destination name exists, this function always fails and sets #red_errno to
    #RED_EEXIST.  This behavior is contrary to POSIX.

    If #REDCONF_RENAME_ATOMIC is true, and if the new name exists, then in one
    atomic operation, @p pszNewPath is unlinked and @p pszOldPath is renamed to
    @p pszNewPath.  Both @p pszNewPath and @p pszOldPath must be of the same
    type (both files or both directories).  As with red_unlink(), if
    @p pszNewPath is a directory, it must be empty.  The major exception to this
    behavior is that if both @p pszOldPath and @p pszNewPath are links to the
    same inode, then the rename does nothing and both names continue to exist.
    Unlike POSIX rename, if @p pszNewPath points to an inode with a link count
    of one and open handles (file descriptors or directory streams), the
    rename will fail with #RED_EBUSY.

    If the rename deletes the old destination, it may free data in the
    committed state, which will not return to free space until after a
    transaction point.  Similarly, if the deleted inode was part of the
    committed state, the inode slot will not be available until after a
    transaction point.

    @param pszOldPath   The path of the file or directory to rename.
    @param pszNewPath   The new name and location after the rename.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBUSY: @p pszOldPath or @p pszNewPath names the root directory; or
      #REDCONF_RENAME_ATOMIC is true and either a) @p pszNewPath
      points to an inode with open handles and a link count of one or b)
      #REDCONF_API_POSIX_CWD is true and the @p pszNewPath points to an inode
      which is the CWD of at least one task.
    - #RED_EEXIST: #REDCONF_RENAME_ATOMIC is false and @p pszNewPath exists.
    - #RED_EINVAL: @p pszOldPath is `NULL`; or @p pszNewPath is `NULL`; or the
      volume containing the path is not mounted; or #REDCONF_API_POSIX_CWD is
      true and either path ends with dot or dot-dot; or #REDCONF_API_POSIX_CWD
      is false and @p pszNewPath ends with dot or dot-dot; or the rename is
      cyclic.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EISDIR: The @p pszNewPath argument names a directory and the
      @p pszOldPath argument names a non-directory.
    - #RED_ENAMETOOLONG: The length of a component of either @p pszOldPath or
      @p pszNewPath is longer than #REDCONF_NAME_MAX.
    - #RED_ENOENT: The link named by @p pszOldPath does not name an existing
      entry; or either @p pszOldPath or @p pszNewPath point to an empty string
      (and there is no volume with an empty path prefix).
    - #RED_ENOTDIR: A component of either path prefix is not a directory; or
      @p pszOldPath names a directory and @p pszNewPath names a file.
    - #RED_ENOTEMPTY: The path named by @p pszNewPath is a directory which is
      not empty.
    - #RED_ENOSPC: The file system does not have enough space to extend the
      directory that would contain @p pszNewPath.
    - #RED_EROFS: The directory to be removed resides on a read-only file
      system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
    - #RED_EXDEV: @p pszOldPath and @p pszNewPath are on different file system
      volumes.
*/
int32_t red_rename(
    const char *pszOldPath,
    const char *pszNewPath)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        uint8_t     bOldVolNum;
        uint32_t    ulOldCwdInode;
        const char *pszOldLocalPath;

        ret = PathStartingPoint(pszOldPath, &bOldVolNum, &ulOldCwdInode, &pszOldLocalPath);
        if(ret == 0)
        {
            uint8_t     bNewVolNum;
            uint32_t    ulNewCwdInode;
            const char *pszNewLocalPath;

            ret = PathStartingPoint(pszNewPath, &bNewVolNum, &ulNewCwdInode, &pszNewLocalPath);

            if((ret == 0) && (bOldVolNum != bNewVolNum))
            {
                ret = -RED_EXDEV;
            }

            if(ret == 0)
            {
                const char *pszOldName;
                uint32_t    ulOldPInode;

                ret = RedPathToName(ulOldCwdInode, pszOldLocalPath, -RED_EBUSY, &ulOldPInode, &pszOldName);
                if(ret == 0)
                {
                    const char *pszNewName;
                    uint32_t    ulNewPInode;

                    ret = RedPathToName(ulNewCwdInode, pszNewLocalPath, -RED_EBUSY, &ulNewPInode, &pszNewName);

                  #if REDCONF_RENAME_ATOMIC == 1
                    if(ret == 0)
                    {
                        uint32_t ulDestInode;

                        ret = RedCoreLookup(ulNewPInode, pszNewName, &ulDestInode);
                        if(ret == 0)
                        {
                            ret = InodeUnlinkCheck(ulDestInode);
                        }
                        else if(ret == -RED_ENOENT)
                        {
                            ret = 0;
                        }
                        else
                        {
                            /*  Unexpected error, nothing to do.
                            */
                        }
                    }
                  #endif

                    if(ret == 0)
                    {
                        ret = RedCoreRename(ulOldPInode, pszOldName, ulNewPInode, pszNewName);
                    }
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_LINK == 1)
/** @brief Create a hard link.

    This creates an additional name (link) for the file named by @p pszPath.
    The new name refers to the same file with the same contents.  If a name is
    deleted, but the underlying file has other names, the file continues to
    exist.  The link count (accessible via red_fstat()) indicates the number of
    names that a file has.  All of a file's names are on equal footing: there
    is nothing special about the original name.

    If @p pszPath names a directory, the operation will fail.

    @param pszPath      The path indicating the inode for the new link.
    @param pszHardLink  The name and location for the new link.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EEXIST: @p pszHardLink resolves to an existing file.
    - #RED_EINVAL: @p pszPath or @p pszHardLink is `NULL`; or the volume
      containing the paths is not mounted; or #REDCONF_API_POSIX_CWD is true and
      @p pszHardLink ends with dot or dot-dot.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EMLINK: Creating the link would exceed the maximum link count of the
      inode named by @p pszPath.
    - #RED_ENAMETOOLONG: The length of a component of either @p pszPath or
      @p pszHardLink is longer than #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of either path prefix does not exist; or the file
      named by @p pszPath does not exist; or either @p pszPath or @p pszHardLink
      point to an empty string (and there is no volume with an empty path
      prefix).
    - #RED_ENOSPC: There is insufficient free space to expand the directory that
      would contain the link.
    - #RED_ENOTDIR: A component of either path prefix is not a directory.
    - #RED_EPERM: The @p pszPath argument names a directory.
    - #RED_EROFS: The requested link requires writing in a directory on a
      read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
    - #RED_EXDEV: @p pszPath and @p pszHardLink are on different file system
      volumes.
*/
int32_t red_link(
    const char *pszPath,
    const char *pszHardLink)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        uint8_t     bVolNum;
        uint32_t    ulCwdInode;
        const char *pszLocalPath;

        ret = PathStartingPoint(pszPath, &bVolNum, &ulCwdInode, &pszLocalPath);
        if(ret == 0)
        {
            uint8_t     bLinkVolNum;
            uint32_t    ulLinkCwdInode;
            const char *pszLinkLocalPath;

            ret = PathStartingPoint(pszHardLink, &bLinkVolNum, &ulLinkCwdInode, &pszLinkLocalPath);

            if((ret == 0) && (bVolNum != bLinkVolNum))
            {
                ret = -RED_EXDEV;
            }

            if(ret == 0)
            {
                uint32_t    ulInode;

                ret = RedPathLookup(ulCwdInode, pszLocalPath, &ulInode);
                if(ret == 0)
                {
                    const char *pszLinkName;
                    uint32_t    ulLinkPInode;

                    ret = RedPathToName(ulLinkCwdInode, pszLinkLocalPath, -RED_EEXIST, &ulLinkPInode, &pszLinkName);
                    if(ret == 0)
                    {
                        ret = RedCoreLink(ulLinkPInode, pszLinkName, ulInode);
                    }
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif


/** @brief Close a file descriptor.

    @param iFildes  The file descriptor to close.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: @p iFildes is not a valid file descriptor.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_close(
    int32_t     iFildes)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        ret = FildesClose(iFildes);

        PosixLeave();
    }

    return PosixReturn(ret);
}


/** @brief Read from an open file.

    The read takes place at the file offset associated with @p iFildes and
    advances the file offset by the number of bytes actually read.

    Data which has not yet been written, but which is before the end-of-file
    (sparse data), will read as zeroes.  A short read -- where the number of
    bytes read is less than requested -- indicates that the requested read was
    partially or, if zero bytes were read, entirely beyond the end-of-file.

    @param iFildes  The file descriptor from which to read.
    @param pBuffer  The buffer to populate with data read.  Must be at least
                    @p ulLength bytes in size.
    @param ulLength Number of bytes to attempt to read.

    @return On success, returns a nonnegative value indicating the number of
            bytes actually read.  On error, -1 is returned and #red_errno is
            set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor open
      for reading.
    - #RED_EINVAL: @p pBuffer is `NULL`; or @p ulLength exceeds INT32_MAX and
      cannot be returned properly.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EISDIR: The @p iFildes is a file descriptor for a directory.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_read(
    int32_t     iFildes,
    void       *pBuffer,
    uint32_t    ulLength)
{
    uint32_t    ulLenRead = 0U;
    REDSTATUS   ret;
    int32_t     iReturn;

    if(ulLength > (uint32_t)INT32_MAX)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        ret = PosixEnter();
    }

    if(ret == 0)
    {
        REDHANDLE  *pHandle;

        ret = FildesToHandle(iFildes, FTYPE_FILE, &pHandle);

        if((ret == 0) && ((pHandle->bFlags & HFLAG_READABLE) == 0U))
        {
            ret = -RED_EBADF;
        }

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->bVolNum);
        }
      #endif

        if(ret == 0)
        {
            ulLenRead = ulLength;
            ret = RedCoreFileRead(pHandle->ulInode, pHandle->o.ullFileOffset, &ulLenRead, pBuffer);
        }

        if(ret == 0)
        {
            REDASSERT(ulLenRead <= ulLength);

            pHandle->o.ullFileOffset += ulLenRead;
        }

        PosixLeave();
    }

    if(ret == 0)
    {
        iReturn = (int32_t)ulLenRead;
    }
    else
    {
        iReturn = PosixReturn(ret);
    }

    return iReturn;
}


#if REDCONF_READ_ONLY == 0
/** @brief Write to an open file.

    The write takes place at the file offset associated with @p iFildes and
    advances the file offset by the number of bytes actually written.
    Alternatively, if @p iFildes was opened with #RED_O_APPEND, the file offset
    is set to the end-of-file before the write begins, and likewise advances by
    the number of bytes actually written.

    A short write -- where the number of bytes written is less than requested
    -- indicates either that the file system ran out of space but was still
    able to write some of the request; or that the request would have caused
    the file to exceed the maximum file size, but some of the data could be
    written prior to the file size limit.

    If an error is returned (-1), either none of the data was written or a
    critical error occurred (like an I/O error) and the file system volume will
    be read-only.

    @param iFildes  The file descriptor to write to.
    @param pBuffer  The buffer containing the data to be written.  Must be at
                    least @p ulLength bytes in size.
    @param ulLength Number of bytes to attempt to write.

    @return On success, returns a nonnegative value indicating the number of
            bytes actually written.  On error, -1 is returned and #red_errno is
            set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor open
      for writing.  This includes the case where the file descriptor is for a
      directory.
    - #RED_EFBIG: No data can be written to the current file offset since the
      resulting file size would exceed the maximum file size.
    - #RED_EINVAL: @p pBuffer is `NULL`; or @p ulLength exceeds INT32_MAX and
      cannot be returned properly.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ENOSPC: No data can be written because there is insufficient free
      space.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_write(
    int32_t     iFildes,
    const void *pBuffer,
    uint32_t    ulLength)
{
    uint32_t    ulLenWrote = 0U;
    REDSTATUS   ret;
    int32_t     iReturn;

    if(ulLength > (uint32_t)INT32_MAX)
    {
        ret = -RED_EINVAL;
    }
    else
    {
        ret = PosixEnter();
    }

    if(ret == 0)
    {
        REDHANDLE *pHandle;

        ret = FildesToHandle(iFildes, FTYPE_FILE, &pHandle);
        if(ret == -RED_EISDIR)
        {
            /*  POSIX says that if a file descriptor is not writable, the
                errno should be -RED_EBADF.  Directory file descriptors are
                never writable, and unlike for read(), the spec does not
                list -RED_EISDIR as an allowed errno.  Therefore -RED_EBADF
                takes precedence.
            */
            ret = -RED_EBADF;
        }

        if((ret == 0) && ((pHandle->bFlags & HFLAG_WRITEABLE) == 0U))
        {
            ret = -RED_EBADF;
        }

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->bVolNum);
        }
      #endif

        if((ret == 0) && ((pHandle->bFlags & HFLAG_APPENDING) != 0U))
        {
            REDSTAT s;

            ret = RedCoreStat(pHandle->ulInode, &s);
            if(ret == 0)
            {
                pHandle->o.ullFileOffset = s.st_size;
            }
        }

        if(ret == 0)
        {
            ulLenWrote = ulLength;
            ret = RedCoreFileWrite(pHandle->ulInode, pHandle->o.ullFileOffset, &ulLenWrote, pBuffer);
        }

        if(ret == 0)
        {
            REDASSERT(ulLenWrote <= ulLength);

            pHandle->o.ullFileOffset += ulLenWrote;
        }

        PosixLeave();
    }

    if(ret == 0)
    {
        iReturn = (int32_t)ulLenWrote;
    }
    else
    {
        iReturn = PosixReturn(ret);
    }

    return iReturn;
}
#endif


#if REDCONF_READ_ONLY == 0
/** @brief Synchronizes changes to a file.

    Commits all changes associated with a file or directory (including file
    data, directory contents, and metadata) to permanent storage.  This
    function will not return until the operation is complete.

    In the current implementation, this function has global effect.  All dirty
    buffers are flushed and a transaction point is committed.  Fsyncing one
    file effectively fsyncs all files.

    If fsync automatic transactions have been disabled, this function does
    nothing and returns success.  In the current implementation, this is the
    only real difference between this function and red_transact(): this
    function can be configured to do nothing, whereas red_transact() is
    unconditional.

    Applications written for portability should avoid assuming red_fsync()
    effects all files, and use red_fsync() on each file that needs to be
    synchronized.

    Passing read-only file descriptors to this function is permitted.

    @param iFildes  The file descriptor to synchronize.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_fsync(
    int32_t     iFildes)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        REDHANDLE *pHandle;

        ret = FildesToHandle(iFildes, FTYPE_EITHER, &pHandle);

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->bVolNum);
        }
      #endif

        /*  No core event for fsync, so this transaction flag needs to be
            implemented here.
        */
        if(ret == 0)
        {
            uint32_t    ulTransMask;

            ret = RedCoreTransMaskGet(&ulTransMask);

            if((ret == 0) && ((ulTransMask & RED_TRANSACT_FSYNC) != 0U))
            {
                ret = RedCoreVolTransact();
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif


/** @brief Move the read/write file offset.

    The file offset of the @p iFildes file descriptor is set to @p llOffset,
    relative to some starting position.  The available positions are:

    - ::RED_SEEK_SET Seek from the start of the file.  In other words,
      @p llOffset becomes the new file offset.
    - ::RED_SEEK_CUR Seek from the current file offset.  In other words,
      @p llOffset is added to the current file offset.
    - ::RED_SEEK_END Seek from the end-of-file.  In other words, the new file
      offset is the file size plus @p llOffset.

    Since @p llOffset is signed (can be negative), it is possible to seek
    backward with ::RED_SEEK_CUR or ::RED_SEEK_END.

    It is permitted to seek beyond the end-of-file; this does not increase the
    file size (a subsequent red_write() call would).

    Unlike POSIX lseek, this function cannot be used with directory file
    descriptors.

    @param iFildes  The file descriptor whose offset is to be updated.
    @param llOffset The new file offset, relative to @p whence.
    @param whence   The location from which @p llOffset should be applied.

    @return On success, returns the new file position, measured in bytes from
            the beginning of the file.  On error, -1 is returned and #red_errno
            is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not an open file descriptor.
    - #RED_EINVAL: @p whence is not a valid `RED_SEEK_` value; or the resulting
      file offset would be negative or beyond the maximum file size.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EISDIR: The @p iFildes argument is a file descriptor for a directory.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int64_t red_lseek(
    int32_t     iFildes,
    int64_t     llOffset,
    REDWHENCE   whence)
{
    REDSTATUS   ret;
    int64_t     llReturn = -1;  /* Init'd to quiet warnings. */

    ret = PosixEnter();
    if(ret == 0)
    {
        int64_t     llFrom = 0; /* Init'd to quiet warnings. */
        REDHANDLE  *pHandle;

        /*  Unlike POSIX, we disallow lseek() on directory handles.
        */
        ret = FildesToHandle(iFildes, FTYPE_FILE, &pHandle);

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->bVolNum);
        }
      #endif

        if(ret == 0)
        {
            switch(whence)
            {
                /*  Seek from the beginning of the file.
                */
                case RED_SEEK_SET:
                    llFrom = 0;
                    break;

                /*  Seek from the current file offset.
                */
                case RED_SEEK_CUR:
                    REDASSERT(pHandle->o.ullFileOffset <= (uint64_t)INT64_MAX);
                    llFrom = (int64_t)pHandle->o.ullFileOffset;
                    break;

                /*  Seek from the end of the file.
                */
                case RED_SEEK_END:
                {
                    REDSTAT s;

                    ret = RedCoreStat(pHandle->ulInode, &s);
                    if(ret == 0)
                    {
                        REDASSERT(s.st_size <= (uint64_t)INT64_MAX);
                        llFrom = (int64_t)s.st_size;
                    }

                    break;
                }

                default:
                    ret = -RED_EINVAL;
                    break;
            }
        }

        if(ret == 0)
        {
            REDASSERT(llFrom >= 0);

            /*  Avoid signed integer overflow from llFrom + llOffset with large
                values of llOffset and nonzero llFrom values.  Underflow isn't
                possible since llFrom is nonnegative.
            */
            if((llOffset > 0) && (((uint64_t)llFrom + (uint64_t)llOffset) > (uint64_t)INT64_MAX))
            {
                ret = -RED_EINVAL;
            }
            else
            {
                int64_t llNewOffset = llFrom + llOffset;

                if((llNewOffset < 0) || ((uint64_t)llNewOffset > gpRedVolume->ullMaxInodeSize))
                {
                    /*  Invalid file offset.
                    */
                    ret = -RED_EINVAL;
                }
                else
                {
                    pHandle->o.ullFileOffset = (uint64_t)llNewOffset;
                    llReturn = llNewOffset;
                }
            }
        }

        PosixLeave();
    }

    if(ret != 0)
    {
        llReturn = PosixReturn(ret);
    }

    return llReturn;
}


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FTRUNCATE == 1)
/** @brief Truncate a file to a specified length.

    Allows the file size to be increased, decreased, or to remain the same.  If
    the file size is increased, the new area is sparse (will read as zeroes).
    If the file size is decreased, the data beyond the new end-of-file will
    return to free space once it is no longer part of the committed state
    (either immediately or after the next transaction point).

    The value of the file offset is not modified by this function.

    Unlike POSIX ftruncate, this function can fail when the disk is full if
    @p ullSize is non-zero.  If decreasing the file size, this can be fixed by
    transacting and trying again: Reliance Edge guarantees that it is possible
    to perform a truncate of at least one file that decreases the file size
    after a transaction point.  If disk full transactions are enabled, this will
    happen automatically.

    @param iFildes  The file descriptor of the file to truncate.
    @param ullSize  The new size of the file.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor open
      for writing.  This includes the case where the file descriptor is for a
      directory.
    - #RED_EFBIG: @p ullSize exceeds the maximum file size.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ENOSPC: Insufficient free space to perform the truncate.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_ftruncate(
    int32_t     iFildes,
    uint64_t    ullSize)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        REDHANDLE *pHandle;

        ret = FildesToHandle(iFildes, FTYPE_FILE, &pHandle);
        if(ret == -RED_EISDIR)
        {
            /*  Similar to red_write() (see comment there), the RED_EBADF error
                for a non-writable file descriptor takes precedence.
            */
            ret = -RED_EBADF;
        }

        if((ret == 0) && ((pHandle->bFlags & HFLAG_WRITEABLE) == 0U))
        {
            ret = -RED_EBADF;
        }

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->bVolNum);
        }
      #endif

        if(ret == 0)
        {
            ret = RedCoreFileTruncate(pHandle->ulInode, ullSize);
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif


/** @brief Get the status of a file or directory.

    See the ::REDSTAT type for the details of the information returned.

    @param iFildes  An open file descriptor for the file whose information is
                    to be retrieved.
    @param pStat    Pointer to a ::REDSTAT buffer to populate.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor.
    - #RED_EINVAL: @p pStat is `NULL`.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_fstat(
    int32_t     iFildes,
    REDSTAT    *pStat)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        REDHANDLE *pHandle;

        ret = FildesToHandle(iFildes, FTYPE_EITHER, &pHandle);

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->bVolNum);
        }
      #endif

        if(ret == 0)
        {
            ret = RedCoreStat(pHandle->ulInode, pStat);
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


#if REDCONF_API_POSIX_READDIR == 1
/** @brief Open a directory stream for reading.

    @param pszPath  The path of the directory to open.

    @return On success, returns a pointer to a ::REDDIR object that can be used
            with red_readdir() and red_closedir().  On error, returns `NULL`
            and #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ENOENT: A component of @p pszPath does not exist; or the @p pszPath
      argument points to an empty string (and there is no volume with an empty
      path prefix).
    - #RED_ENOTDIR: A component of @p pszPath is a not a directory.
    - #RED_EMFILE: There are no available file descriptors.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
REDDIR *red_opendir(
    const char *pszPath)
{
    int32_t     iFildes;
    REDSTATUS   ret;
    REDDIR     *pDir = NULL;

    ret = PosixEnter();
    if(ret == 0)
    {
        ret = FildesOpen(pszPath, RED_O_RDONLY, FTYPE_DIR, &iFildes);
        if(ret == 0)
        {
            uint16_t uHandleIdx;

            FildesUnpack(iFildes, &uHandleIdx, NULL, NULL);
            pDir = &gaHandle[uHandleIdx];
        }

        PosixLeave();
    }

    REDASSERT((pDir == NULL) == (ret != 0));

    if(pDir == NULL)
    {
        red_errno = -ret;
    }

    return pDir;
}


/** @brief Read from a directory stream.

    The ::REDDIRENT pointer returned by this function will be overwritten by
    subsequent calls on the same @p pDir.  Calls with other ::REDDIR objects
    will *not* modify the returned ::REDDIRENT.

    If files are added to the directory after it is opened, the new files may
    or may not be returned by this function.  If files are deleted, the deleted
    files will not be returned.

    This function (like its POSIX equivalent) returns `NULL` in two cases: on
    error and when the end of the directory is reached.  To distinguish between
    these two cases, the application should set #red_errno to zero before
    calling this function, and if `NULL` is returned, check if #red_errno is
    still zero.  If it is, the end of the directory was reached; otherwise,
    there was an error.

    @param pDirStream   The directory stream to read from.

    @return On success, returns a pointer to a ::REDDIRENT object which is
            populated with directory entry information read from the directory.
            On error, returns `NULL` and #red_errno is set appropriately.  If at
            the end of the directory, returns `NULL` but #red_errno is not
            modified.

    <b>Errno values</b>
    - #RED_EBADF: @p pDirStream is not an open directory stream.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
REDDIRENT *red_readdir(
    REDDIR     *pDirStream)
{
    REDSTATUS   ret;
    REDDIRENT  *pDirEnt = NULL;

    ret = PosixEnter();
    if(ret == 0)
    {
        if(!DirStreamIsValid(pDirStream))
        {
            ret = -RED_EBADF;
        }
      #if REDCONF_VOLUME_COUNT > 1U
        else
        {
            ret = RedCoreVolSetCurrent(pDirStream->bVolNum);
        }
      #endif

        if(ret == 0)
        {
            ret = RedCoreDirRead(pDirStream->ulInode, &pDirStream->o.ulDirPosition, pDirStream->dirent.d_name, &pDirStream->dirent.d_ino);
            if(ret == 0)
            {
                /*  POSIX extension: return stat information with the dirent.
                */
                ret = RedCoreStat(pDirStream->dirent.d_ino, &pDirStream->dirent.d_stat);
                if(ret == 0)
                {
                    pDirEnt = &pDirStream->dirent;
                }
            }
            else if(ret == -RED_ENOENT)
            {
                /*  Reached the end of the directory; return NULL but do not set
                    errno.
                */
                ret = 0;
            }
            else
            {
                /*  Miscellaneous error; return NULL and set errno (done below).
                */
            }
        }

        PosixLeave();
    }

    if(ret != 0)
    {
        REDASSERT(pDirEnt == NULL);

        red_errno = -ret;
    }

    return pDirEnt;
}


/** @brief Rewind a directory stream to read it from the beginning.

    Similar to closing the directory object and opening it again, but without
    the need for the path.

    Since this function (like its POSIX equivalent) cannot return an error,
    it takes no action in error conditions, such as when @p pDirStream is
    invalid.

    @param pDirStream   The directory stream to rewind.
*/
void red_rewinddir(
    REDDIR *pDirStream)
{
    if(PosixEnter() == 0)
    {
        if(DirStreamIsValid(pDirStream))
        {
            pDirStream->o.ulDirPosition = 0U;
        }

        PosixLeave();
    }
}


/** @brief Close a directory stream.

    After calling this function, @p pDirStream should no longer be used.

    @param pDirStream   The directory stream to close.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: @p pDirStream is not an open directory stream.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_closedir(
    REDDIR     *pDirStream)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        if(DirStreamIsValid(pDirStream))
        {
            /*  Mark this handle as unused.
            */
            pDirStream->ulInode = INODE_INVALID;
        }
        else
        {
            ret = -RED_EBADF;
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif /* REDCONF_API_POSIX_READDIR */


#if REDCONF_API_POSIX_CWD == 1
/** @brief Change the current working directory (CWD).

    The default CWD, if it has never been set since the file system was
    initialized, is the root directory of volume zero.  If the CWD is on a
    volume that is unmounted, it resets to the root directory of that volume.

    @param pszPath  The path to the directory which will become the current
                    working directory.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of @p pszPath does not name an existing
      directory; or the volume does not exist; or the @p pszPath argument
      points to an empty string (and there is no volume with an empty path
      prefix).
    - #RED_ENOTDIR: A component of @p pszPath does not name a directory.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_chdir(
    const char *pszPath)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        uint8_t     bVolNum;
        uint32_t    ulCwdInode;
        const char *pszLocalPath;

        ret = PathStartingPoint(pszPath, &bVolNum, &ulCwdInode, &pszLocalPath);
        if(ret == 0)
        {
            uint32_t ulInode;

            /*  Resolve the new CWD.
            */
            ret = RedPathLookup(ulCwdInode, pszLocalPath, &ulInode);
            if(ret == 0)
            {
                /*  The CWD must be a directory.
                */
                if(ulInode != INODE_ROOTDIR)
                {
                    REDSTAT sb;

                    ret = RedCoreStat(ulInode, &sb);
                    if((ret == 0) && !RED_S_ISDIR(sb.st_mode))
                    {
                        ret = -RED_ENOTDIR;
                    }
                }

                /*  Update the CWD.
                */
                if(ret == 0)
                {
                    WORKDIR *pCwd = CwdGet();

                    if(pCwd == NULL)
                    {
                        /*  This code should be unreachable because PosixEnter()
                            never returns zero unless the task is registered,
                            and every registered task has a CWD.
                        */
                        REDERROR();
                        ret = -RED_EFUBAR;
                    }
                    else
                    {
                        pCwd->bVolNum = bVolNum;
                        pCwd->ulInode = ulInode;
                    }
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


/** @brief Get the path of the current working directory (CWD).

    The default CWD, if it has never been set since the file system was
    initialized, is the root directory of volume zero.  If the CWD is on a
    volume that is unmounted, it resets to the root directory of that volume.

    @note Reliance Edge does not have a maximum path length; paths, including
          the CWD path, can be arbitrarily long.  Thus, no buffer is guaranteed
          to be large enough to store the CWD.  If it is important that calls to
          this function succeed, you need to analyze your application to
          determine the maximum length of the CWD path.  Alternatively, if
          dynamic memory allocation is used, this function can be called in a
          loop, with the buffer size increasing if the function fails with a
          RED_ERANGE error; repeat until the call succeeds.

    @param pszBuffer    The buffer to populate with the CWD.
    @param ulBufferSize The size in bytes of @p pszBuffer.

    @return On success, @p pszBuffer is returned.  On error, `NULL` is returned
            and #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EINVAL: @p pszBuffer is `NULL`; or @p ulBufferSize is zero.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ERANGE: @p ulBufferSize is greater than zero but too small for the
      CWD path string.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
char *red_getcwd(
    char       *pszBuffer,
    uint32_t    ulBufferSize)
{
    REDSTATUS   ret;
    char       *pszReturn;

    if((pszBuffer == NULL) || (ulBufferSize == 0U))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        ret = PosixEnter();
        if(ret == 0)
        {
            const WORKDIR *pCwd = CwdGet();

            if(pCwd == NULL)
            {
                /*  This code should be unreachable because PosixEnter() never
                    returns zero unless the task is registered, and every
                    registered task has a CWD.
                */
                REDERROR();
                ret = -RED_EFUBAR;
            }
            else
            {
                /*  Implementation notes...  We store the CWD as an inode/volume
                    rather than as a string, which has several advantages: it
                    saves memory, avoids the need to impose a maximum path
                    length, makes relative path operations faster since the CWD
                    does not need to be resolved every time, and makes it easy
                    to allow renaming and disallow deleting the CWD.  The
                    disadvantage is that getcwd() (this function) is more
                    complicated, because the CWD buffer must be constructed.
                    This construction is possible since each directory inode
                    stores the inode number of its parent directory (only one
                    parent: no hard links allowed for directories), so for the
                    CWD inode we can step up to its parent, then scan that
                    parent directory for the name which corresponds to the
                    inode. Iteratively we can repeat this process to construct
                    the CWD in reverse, starting with the deepest subdirectory
                    and working up toward the root directory.  This is
                    potentially a slow operation if the directories are large
                    and thus slow to scan.
                */

              #if REDCONF_VOLUME_COUNT > 1U
                ret = RedCoreVolSetCurrent(pCwd->bVolNum);
                if(ret == 0)
              #endif
                {
                    uint32_t ulInode = pCwd->ulInode;
                    uint32_t ulPInode;
                    uint32_t ulCwdLen; /* Length includes terminating NUL */

                    pszBuffer[0U] = '\0';
                    ulCwdLen = 1U;

                    /*  The CWD for an unmounted volume is always the root
                        directory -- so in that case, the loop below is not
                        entered, and we end up populating the buffer with just
                        the volume path prefix and a path separator, which is
                        exactly as it should be.
                    */
                    REDASSERT(gpRedVolume->fMounted || (ulInode == INODE_ROOTDIR));

                    /*  Work our way up the path, converting the inode numbers
                        to names, building the CWD in reverse, until we reach
                        the root directory.
                    */
                    while((ret == 0) && (ulInode != INODE_ROOTDIR))
                    {
                        /*  The name buffer is static in case REDCONF_NAME_MAX
                            is too big to fit on the stack; we're single-
                            threaded so this is safe.  The variable name
                            includes "GetCwd" since that might be preserved in
                            the linker's map file, making it easier to determine
                            who is allocating this memory.
                        */
                        static char szGetCwdName[REDCONF_NAME_MAX + 1U];
                        uint32_t    ulDirPos = 0U;

                        /*  Scan the parent directory to convert this inode into
                            a name.  Hard linking is prohibited for directories
                            so the inode will have only one parent inode and one
                            name.
                        */
                        ret = RedCoreDirParent(ulInode, &ulPInode);
                        while(ret == 0)
                        {
                            uint32_t ulThisInode;

                            ret = RedCoreDirRead(ulPInode, &ulDirPos, szGetCwdName, &ulThisInode);
                            if((ret == 0) && (ulThisInode == ulInode))
                            {
                                /*  Found the matching name.
                                */
                                break;
                            }

                            /*  If we get to the end of the parent directory
                                without finding the inode of the child
                                directory, something is wrong -- probably file
                                system corruption.
                            */
                            if(ret == -RED_ENOENT)
                            {
                                REDERROR();
                                ret = -RED_EFUBAR;
                            }
                        }

                        /*  Shift the contents of pszBuffer to the right and
                            copy in the next name.  For example, if the CWD is
                            "a/b/c", the contents of pszBuffer will be "", then
                            "c", then "b/c", then "a/b/c".
                        */
                        if(ret == 0)
                        {
                            /*  Skip the path separator for the first name so
                                that we end up with "a/b/c" instead of "a/b/c/".
                            */
                            bool fPathSeparator = (ulInode != pCwd->ulInode);
                            uint32_t ulNameLen = RedNameLen(szGetCwdName);
                            uint32_t ulNewLen = ulNameLen;

                            if(fPathSeparator)
                            {
                                ulNewLen++; /* For path separator */
                            }

                            if((ulCwdLen + ulNewLen) > ulBufferSize)
                            {
                                /*  The CWD buffer provided by the caller is too
                                    small.
                                */
                                ret = -RED_ERANGE;
                            }
                            else
                            {
                                RedMemMove(&pszBuffer[ulNewLen], pszBuffer, ulCwdLen);
                                RedMemCpy(pszBuffer, szGetCwdName, ulNameLen);
                                if(fPathSeparator)
                                {
                                    pszBuffer[ulNameLen] = REDCONF_PATH_SEPARATOR;
                                }

                                ulCwdLen += ulNewLen;
                            }
                        }

                        /*  Move up the path to the parent directory.
                        */
                        if(ret == 0)
                        {
                            ulInode = ulPInode;
                        }
                    }

                    /*  Copy in the volume path prefix, followed by a leading
                        slash for the root directory.
                    */
                    if(ret == 0)
                    {
                        uint32_t ulVolPrefixLen = RedStrLen(gpRedVolConf->pszPathPrefix);

                        if((ulCwdLen + ulVolPrefixLen + 1U) > ulBufferSize)
                        {
                            /*  The CWD buffer provided by the caller is too
                                small.
                            */
                            ret = -RED_ERANGE;
                        }
                        else
                        {
                            RedMemMove(&pszBuffer[ulVolPrefixLen + 1U], pszBuffer, ulCwdLen);
                            RedMemCpy(pszBuffer, gpRedVolConf->pszPathPrefix, ulVolPrefixLen);
                            pszBuffer[ulVolPrefixLen] = REDCONF_PATH_SEPARATOR;
                        }
                    }
                }
            }

            PosixLeave();
        }
    }

    if(ret == 0)
    {
        pszReturn = pszBuffer;
    }
    else
    {
        pszReturn = NULL;
        red_errno = -ret;
    }

    return pszReturn;
}
#endif /* REDCONF_API_POSIX_CWD */


/** @brief Pointer to where the last file system error (errno) is stored.

    This function is intended to be used via the #red_errno macro, or a similar
    user-defined macro, that can be used both as an lvalue (writable) and an
    rvalue (readable).

    Under normal circumstances, the errno for each task is stored in a
    different location.  Applications do not need to worry about one task
    obliterating an error value that another task needed to read.  This task
    errno for is initially zero.  When one of the POSIX-like APIs returns an
    indication of error, the location for the calling task will be populated
    with the error value.

    In some circumstances, this function will return a pointer to a global
    errno location which is shared by multiple tasks.  If the calling task is
    not registered as a file system user and all of the task slots are full,
    there can be no task-specific errno, so the global pointer is returned.
    Likewise, if the file system driver is uninitialized, there are no
    registered file system users and this function always returns the pointer
    to the global errno.  Under these circumstances, multiple tasks
    manipulating errno could be problematic.

    This function never returns `NULL` under any circumstances.  The #red_errno
    macro unconditionally dereferences the return value from this function, so
    returning `NULL` could result in a fault.

    @return Pointer to where the errno value is stored for this task.
*/
REDSTATUS *red_errnoptr(void)
{
    /*  The global errno value, used in single-task configurations and when the
        caller is not (and cannot become) a file system user (which includes
        when the driver is uninitialized).
    */
    static REDSTATUS iGlobalErrno = 0;

  #if REDCONF_TASK_COUNT == 1U

    return &iGlobalErrno;

  #else

    REDSTATUS *piErrno;

    if(gfPosixInited)
    {
        uint32_t ulTaskId = RedOsTaskId();
        uint32_t ulIdx;

        REDASSERT(ulTaskId != 0U);

        /*  If this task has used the file system before, it will already have
            a task slot, which includes the task-specific errno.
        */
        RedOsMutexAcquire();

        for(ulIdx = 0U; ulIdx < REDCONF_TASK_COUNT; ulIdx++)
        {
            if(gaTask[ulIdx].ulTaskId == ulTaskId)
            {
                break;
            }
        }

        RedOsMutexRelease();

        if(ulIdx == REDCONF_TASK_COUNT)
        {
            REDSTATUS ret;

            /*  This task is not a file system user, so try to register it as
                one.  This FS mutex must be held in order to register.
            */
            RedOsMutexAcquire();

            ret = TaskRegister(&ulIdx);

            RedOsMutexRelease();

            if(ret == 0)
            {
                REDASSERT(gaTask[ulIdx].ulTaskId == RedOsTaskId());
                REDASSERT(gaTask[ulIdx].iErrno == 0);

                piErrno = &gaTask[ulIdx].iErrno;
            }
            else
            {
                /*  Unable to register; use the global errno.
                */
                piErrno = &iGlobalErrno;
            }
        }
        else
        {
            piErrno = &gaTask[ulIdx].iErrno;
        }
    }
    else
    {
        /*  There are no registered file system tasks when the driver is
            uninitialized, so use the global errno.
        */
        piErrno = &iGlobalErrno;
    }

    /*  This function is not allowed to return NULL.
    */
    REDASSERT(piErrno != NULL);
    return piErrno;

  #endif
}
/** @} */

/*-------------------------------------------------------------------
    Helper Functions
-------------------------------------------------------------------*/

#if (REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1))

/** @brief Remove a link to a file or directory.

    If the link count becomes zero, the file or directory is deleted.

    @param pszPath  Path of the link to remove.
    @param type     The expected type of the path: file, directory, or either.
                    An error is returned if the expected type is file or
                    directory and does not match the path.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval -RED_EBUSY          @p pszPath names the root directory; or
                                @p pszPath points to an inode with open handles
                                and a link count of one; or
                                #REDCONF_API_POSIX_CWD is true and @p pszPath
                                points to an inode which is the CWD of at least
                                one task.
    @retval -RED_EINVAL         @p pszPath is `NULL`; or the volume containing
                                the path is not mounted; or
                                #REDCONF_API_POSIX_CWD is true and the path ends
                                with dot or dot-dot.
    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_EISDIR         @p type is ::FTYPE_FILE and the path names a
                                directory.
    @retval -RED_ENAMETOOLONG   @p pszName is too long.
    @retval -RED_ENOENT         The path does not name an existing file; or
                                @p pszPath, after removing the volume prefix (if
                                present), points to an empty string.
    @retval -RED_ENOTDIR        @p type is ::FTYPE_DIR and the path does not
                                name a directory.
    @retval -RED_ENOTEMPTY      @p pszPath is a directory which is not empty.
    @retval -RED_ENOSPC         The file system does not have enough space to
                                modify the parent directory to perform the
                                deletion.
*/
static REDSTATUS UnlinkSub(
    const char *pszPath,
    FTYPE       type)
{
    uint32_t    ulCwdInode;
    const char *pszLocalPath;
    REDSTATUS   ret;

    ret = PathStartingPoint(pszPath, NULL, &ulCwdInode, &pszLocalPath);
    if(ret == 0)
    {
        const char *pszName;
        uint32_t    ulPInode;

        ret = RedPathToName(ulCwdInode, pszLocalPath, -RED_EBUSY, &ulPInode, &pszName);
        if(ret == 0)
        {
            uint32_t ulInode;

            ret = RedCoreLookup(ulPInode, pszName, &ulInode);

            /*  ModeTypeCheck() always passes when the type is FTYPE_EITHER, so
                skip stat'ing the inode in that case.
            */
            if((ret == 0) && (type != FTYPE_EITHER))
            {
                REDSTAT InodeStat;

                ret = RedCoreStat(ulInode, &InodeStat);
                if(ret == 0)
                {
                    ret = ModeTypeCheck(InodeStat.st_mode, type);
                }
            }

            if(ret == 0)
            {
                ret = InodeUnlinkCheck(ulInode);
            }

            if(ret == 0)
            {
                ret = RedCoreUnlink(ulPInode, pszName);
            }
        }
    }

    return ret;
}
#endif /* (REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1) */


/** @brief Find the starting point for a path.

    In other words, find the volume number and directory inode from which the
    parsing of this path should start.

    The volume number will be set as the current volume.

    @param pszPath          The path to examine.
    @param pbVolNum         On successful return, if non-NULL, populated with
                            the volume number for the path.  This volume number
                            is set as the current volume.
    @param pulCwdInode      On successful return, populated with the directory
                            inode that the local path starts in: this is either
                            the root directory or the CWD.
    @param ppszLocalPath    On successful return, populated with the path
                            stripped of volume path prefixing, if there was any.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p pszPath or @p pulCwdInode or @p ppszLocalPath is
                        `NULL`.
    @retval -RED_ENOENT @p pszPath could not be matched to any volume and there
                        is no CWD.
*/
static REDSTATUS PathStartingPoint(
    const char     *pszPath,
    uint8_t        *pbVolNum,
    uint32_t       *pulCwdInode,
    const char    **ppszLocalPath)
{
    REDSTATUS       ret;

    if((pszPath == NULL) || (pulCwdInode == NULL) || (ppszLocalPath == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        uint8_t bVolNum;

        ret = RedPathVolumePrefixLookup(pszPath, &bVolNum);
        if(ret == 0)
        {
            *pulCwdInode = INODE_ROOTDIR;
            *ppszLocalPath = &pszPath[RedStrLen(gpRedVolConf->pszPathPrefix)];
        }

      #if REDCONF_API_POSIX_CWD == 1
        /*  If the path was _not_ an absolute path, use the CWD.  We consider
            the path to be absolute if it exactly matched a non-zero length
            volume path prefix; or if it started with a path separator.

            Don't use the CWD if the path was an empty string -- POSIX considers
            empty paths to be an error.
        */
        if(    ((ret == -RED_ENOENT) || ((ret == 0) && (gpRedVolConf->pszPathPrefix[0U] == '\0')))
            && (pszPath[0U] != REDCONF_PATH_SEPARATOR)
            && (pszPath[0U] != '\0'))
        {
            const WORKDIR *pCwd = CwdGet();

            if(pCwd == NULL)
            {
                /*  This should be unreachable unless there is a coding error
                    and this function is being called without first calling
                    PosixEnter().
                */
                REDERROR();
                ret = -RED_EFUBAR;
            }
            else
            {
                bVolNum = pCwd->bVolNum;
                *pulCwdInode = pCwd->ulInode;
                *ppszLocalPath = pszPath;

              #if REDCONF_VOLUME_COUNT > 1U
                ret = RedCoreVolSetCurrent(bVolNum);
              #else
                ret = 0;
              #endif
            }
        }
      #endif

        if((ret == 0) && (pbVolNum != NULL))
        {
            *pbVolNum = bVolNum;
        }
    }

    return ret;
}


/** @brief Get a file descriptor for a path.

    @param pszPath      Path to a file to open.
    @param ulOpenMode   The RED_O_* flags the descriptor is opened with.
    @param type         Indicates the expected descriptor type: file, directory,
                        or either.
    @param piFildes     On successful return, populated with the file
                        descriptor.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0                   Operation was successful.
    @retval -RED_EINVAL         @p piFildes is `NULL`; or @p pszPath is `NULL`;
                                or the volume is not mounted.
    @retval -RED_EMFILE         There are no available handles.
    @retval -RED_EEXIST         Using #RED_O_CREAT and #RED_O_EXCL, and the
                                indicated path already exists.
    @retval -RED_EISDIR         The path names a directory and @p ulOpenMode
                                includes #RED_O_WRONLY or #RED_O_RDWR.
    @retval -RED_ENOENT         #RED_O_CREAT is not set and the named file does
                                not exist; or #RED_O_CREAT is set and the parent
                                directory does not exist; or the volume does not
                                exist; or the @p pszPath argument, after
                                removing the volume prefix, points to an empty
                                string.
    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_ENAMETOOLONG   The length of a component of @p pszPath is
                                longer than #REDCONF_NAME_MAX.
    @retval -RED_ENFILE         Attempting to create a file but the file system
                                has used all available inode slots.
    @retval -RED_ENOSPC         The file does not exist and #RED_O_CREAT was
                                specified, but there is insufficient free space
                                to expand the directory or to create the new
                                file.
    @retval -RED_ENOTDIR        A component of the prefix in @p pszPath does not
                                name a directory.
    @retval -RED_EROFS          The path resides on a read-only file system and
                                a write operation was requested.
*/
static REDSTATUS FildesOpen(
    const char *pszPath,
    uint32_t    ulOpenMode,
    FTYPE       type,
    int32_t    *piFildes)
{
    uint8_t     bVolNum;
    uint32_t    ulCwdInode;
    const char *pszLocalPath;
    REDSTATUS   ret;

    ret = PathStartingPoint(pszPath, &bVolNum, &ulCwdInode, &pszLocalPath);
    if(ret == 0)
    {
        if(piFildes == NULL)
        {
            ret = -RED_EINVAL;
        }
      #if REDCONF_READ_ONLY == 0
        else if(gpRedVolume->fReadOnly && (ulOpenMode != RED_O_RDONLY))
        {
            ret = -RED_EROFS;
        }
      #endif
        else
        {
            uint16_t    uHandleIdx;
            REDHANDLE  *pHandle = NULL;

            /*  Search for an unused handle.
            */
            for(uHandleIdx = 0U; uHandleIdx < REDCONF_HANDLE_COUNT; uHandleIdx++)
            {
                if(gaHandle[uHandleIdx].ulInode == INODE_INVALID)
                {
                    pHandle = &gaHandle[uHandleIdx];
                    break;
                }
            }

            /*  Error if all the handles are in use.
            */
            if(pHandle == NULL)
            {
                ret = -RED_EMFILE;
            }
            else
            {
                bool        fCreated = false;
                uint16_t    uMode = 0U;
                uint32_t    ulInode = 0U;       /* Init'd to quiet warnings. */

              #if REDCONF_READ_ONLY == 0
                if((ulOpenMode & RED_O_CREAT) != 0U)
                {
                    uint32_t    ulPInode;
                    const char *pszName;

                    ret = RedPathToName(ulCwdInode, pszLocalPath, -RED_EISDIR, &ulPInode, &pszName);
                    if(ret == 0)
                    {
                        ret = RedCoreCreate(ulPInode, pszName, false, &ulInode);
                        if(ret == 0)
                        {
                            fCreated = true;
                        }
                        else if((ret == -RED_EEXIST) && ((ulOpenMode & RED_O_EXCL) == 0U))
                        {
                            /*  If the path already exists and that's OK,
                                lookup its inode number.
                            */
                            ret = RedCoreLookup(ulPInode, pszName, &ulInode);
                        }
                        else
                        {
                            /*  No action, just propagate the error.
                            */
                        }
                    }
                }
                else
              #endif
                {
                    ret = RedPathLookup(ulCwdInode, pszLocalPath, &ulInode);
                }

                /*  If we created the inode, none of the below stuff is
                    necessary.  This is important from an error handling
                    perspective -- we do not need code to delete the created
                    inode on error.
                */
                if(!fCreated)
                {
                    if(ret == 0)
                    {
                        REDSTAT s;

                        ret = RedCoreStat(ulInode, &s);
                        if(ret == 0)
                        {
                            uMode = s.st_mode;
                        }
                    }

                    /*  Error if the inode is not of the expected type.
                    */
                    if(ret == 0)
                    {
                        ret = ModeTypeCheck(uMode, type);
                    }

                    /*  Directories must always be opened with O_RDONLY.
                    */
                    if((ret == 0) && RED_S_ISDIR(uMode) && ((ulOpenMode & RED_O_RDONLY) == 0U))
                    {
                        ret = -RED_EISDIR;
                    }

                  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FTRUNCATE == 1)
                    if((ret == 0) && ((ulOpenMode & RED_O_TRUNC) != 0U))
                    {
                        ret = RedCoreFileTruncate(ulInode, UINT64_SUFFIX(0));
                    }
                  #endif
                }

                if(ret == 0)
                {
                    int32_t iFildes;

                    RedMemSet(pHandle, 0U, sizeof(*pHandle));

                    /*  Populate this handle, marking it as in use.
                    */
                    pHandle->ulInode = ulInode;
                    pHandle->bVolNum = bVolNum;

                    if(RED_S_ISDIR(uMode))
                    {
                        pHandle->bFlags |= HFLAG_DIRECTORY;
                    }

                    if(((ulOpenMode & RED_O_RDONLY) != 0U) || ((ulOpenMode & RED_O_RDWR) != 0U))
                    {
                        pHandle->bFlags |= HFLAG_READABLE;
                    }

                  #if REDCONF_READ_ONLY == 0
                    if(((ulOpenMode & RED_O_WRONLY) != 0U) || ((ulOpenMode & RED_O_RDWR) != 0U))
                    {
                        pHandle->bFlags |= HFLAG_WRITEABLE;
                    }

                    if((ulOpenMode & RED_O_APPEND) != 0U)
                    {
                        pHandle->bFlags |= HFLAG_APPENDING;
                    }
                  #endif

                    iFildes = FildesPack(uHandleIdx, bVolNum);
                    if(iFildes == -1)
                    {
                        /*  It should be impossible to get here, unless there
                            is memory corruption.
                        */
                        REDERROR();
                        ret = -RED_EFUBAR;
                    }
                    else
                    {
                        *piFildes = iFildes;
                    }
                }
            }
        }
    }

    return ret;
}


/** @brief Close a file descriptor.

    @param iFildes  The file descriptor to close.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EBADF  @p iFildes is not a valid file descriptor.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS FildesClose(
    int32_t     iFildes)
{
    REDHANDLE  *pHandle;
    REDSTATUS   ret;

    ret = FildesToHandle(iFildes, FTYPE_EITHER, &pHandle);

  #if REDCONF_READ_ONLY == 0
  #if REDCONF_VOLUME_COUNT > 1U
    if(ret == 0)
    {
        ret = RedCoreVolSetCurrent(pHandle->bVolNum);
    }
  #endif

    /*  No core event for close, so this transaction flag needs to be
        implemented here.

        If the volume is read-only, skip the close transaction.  This avoids
        -RED_EROFS errors when closing files on a read-only volume.
    */
    if((ret == 0) && !gpRedVolume->fReadOnly)
    {
        uint32_t    ulTransMask;

        ret = RedCoreTransMaskGet(&ulTransMask);

        if((ret == 0) && ((ulTransMask & RED_TRANSACT_CLOSE) != 0U))
        {
            ret = RedCoreVolTransact();
        }
    }
  #endif

    if(ret == 0)
    {
        /*  Mark this handle as unused.
        */
        pHandle->ulInode = INODE_INVALID;
    }

    return ret;
}


/** @brief Convert a file descriptor into a handle pointer.

    Also validates the file descriptor.

    @param iFildes  The file descriptor for which to get a handle.
    @param expectedType The expected type of the file descriptor: ::FTYPE_DIR,
                        ::FTYPE_FILE, or ::FTYPE_EITHER.
    @param ppHandle     On successful return, populated with a pointer to the
                        handle associated with @p iFildes.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0               Operation was successful.
    @retval -RED_EBADF      @p iFildes is not a valid file descriptor.
    @retval -RED_EINVAL     @p ppHandle is `NULL`.
    @retval -RED_EISDIR     Expected a file, but the file descriptor is for a
                            directory.
    @retval -RED_ENOTDIR    Expected a directory, but the file descriptor is for
                            a file.
*/
static REDSTATUS FildesToHandle(
    int32_t     iFildes,
    FTYPE       expectedType,
    REDHANDLE **ppHandle)
{
    REDSTATUS   ret;

    if(ppHandle == NULL)
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else if(iFildes < FD_MIN)
    {
        ret = -RED_EBADF;
    }
    else
    {
        uint16_t uHandleIdx;
        uint8_t  bVolNum;
        uint16_t uGeneration;

        FildesUnpack(iFildes, &uHandleIdx, &bVolNum, &uGeneration);

        if(    (uHandleIdx >= REDCONF_HANDLE_COUNT)
            || (bVolNum >= REDCONF_VOLUME_COUNT)
            || (gaHandle[uHandleIdx].ulInode == INODE_INVALID)
            || (gaHandle[uHandleIdx].bVolNum != bVolNum)
            || (gauGeneration[bVolNum] != uGeneration))
        {
            ret = -RED_EBADF;
        }
        else if((expectedType == FTYPE_FILE) && ((gaHandle[uHandleIdx].bFlags & HFLAG_DIRECTORY) != 0U))
        {
            ret = -RED_EISDIR;
        }
        else if((expectedType == FTYPE_DIR) && ((gaHandle[uHandleIdx].bFlags & HFLAG_DIRECTORY) == 0U))
        {
            ret = -RED_ENOTDIR;
        }
        else
        {
            *ppHandle = &gaHandle[uHandleIdx];
            ret = 0;
        }
    }

    return ret;
}


/** @brief Pack a file descriptor.

    @param uHandleIdx   The index of the file handle that will be associated
                        with this file descriptor.
    @param bVolNum      The volume which contains the file or directory this
                        file descriptor was opened against.

    @return The packed file descriptor.
*/
static int32_t FildesPack(
    uint16_t    uHandleIdx,
    uint8_t     bVolNum)
{
    int32_t     iFildes;

    if((uHandleIdx >= REDCONF_HANDLE_COUNT) || (bVolNum >= REDCONF_VOLUME_COUNT))
    {
        REDERROR();
        iFildes = -1;
    }
    else
    {
        uint32_t    ulFdBits;

        REDASSERT(gauGeneration[bVolNum] <= FD_GEN_MAX);
        REDASSERT(gauGeneration[bVolNum] != 0U);

        ulFdBits = gauGeneration[bVolNum];
        ulFdBits <<= FD_VOL_BITS;
        ulFdBits |= bVolNum;
        ulFdBits <<= FD_IDX_BITS;
        ulFdBits |= uHandleIdx;

        iFildes = (int32_t)ulFdBits;

        if(iFildes < FD_MIN)
        {
            REDERROR();
            iFildes = -1;
        }
    }

    return iFildes;
}


/** @brief Unpack a file descriptor.

    @param iFildes      The file descriptor to unpack.
    @param puHandleIdx  If non-NULL, populated with the handle index extracted
                        from the file descriptor.
    @param pbVolNum     If non-NULL, populated with the volume number extracted
                        from the file descriptor.
    @param puGeneration If non-NULL, populated with the generation number
                        extracted from the file descriptor.
*/
static void FildesUnpack(
    int32_t     iFildes,
    uint16_t   *puHandleIdx,
    uint8_t    *pbVolNum,
    uint16_t   *puGeneration)
{
    uint32_t ulFdBits = (uint32_t)iFildes;

    REDASSERT(iFildes >= FD_MIN);

    if(puHandleIdx != NULL)
    {
        *puHandleIdx = (uint16_t)(ulFdBits & FD_IDX_MAX);
    }

    ulFdBits >>= FD_IDX_BITS;

    if(pbVolNum != NULL)
    {
        *pbVolNum = (uint8_t)(ulFdBits & FD_VOL_MAX);
    }

    ulFdBits >>= FD_VOL_BITS;

    if(puGeneration != NULL)
    {
        *puGeneration = (uint16_t)(ulFdBits & FD_GEN_MAX);
    }
}


#if REDCONF_API_POSIX_READDIR == 1
/** @brief Validate a directory stream object.

    @param pDirStream   The directory stream to validate.

    @return Whether the directory stream is valid.

    @retval true    The directory stream object appears valid.
    @retval false   The directory stream object is invalid.
*/
static bool DirStreamIsValid(
    const REDDIR   *pDirStream)
{
    bool            fRet;

    if(!PTR_IS_ARRAY_ELEMENT(pDirStream, gaHandle, ARRAY_SIZE(gaHandle), sizeof(gaHandle[0U])))
    {
        /*  pDirStream is not a pointer to one of our handles.
        */
        fRet = false;
    }
    else if(    (pDirStream->ulInode == INODE_INVALID)
             || (pDirStream->bVolNum >= REDCONF_VOLUME_COUNT)
             || ((pDirStream->bFlags & HFLAG_DIRECTORY) == 0U))
    {
        /*  The handle must be in use, have a valid volume number, and be a
            directory handle.
        */
        fRet = false;
    }
    else
    {
        fRet = true;
    }

    return fRet;
}
#endif


/** @brief Enter the file system driver.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL The file system driver is uninitialized.
    @retval -RED_EUSERS Cannot become a file system user: too many users.
*/
static REDSTATUS PosixEnter(void)
{
    REDSTATUS ret;

    if(gfPosixInited)
    {
      #if REDCONF_TASK_COUNT > 1U
        RedOsMutexAcquire();

        ret = TaskRegister(NULL);
        if(ret != 0)
        {
            RedOsMutexRelease();
        }
      #else
        ret = 0;
      #endif
    }
    else
    {
        ret = -RED_EINVAL;
    }

    return ret;
}


/** @brief Leave the file system driver.
*/
static void PosixLeave(void)
{
    /*  If the driver was uninitialized, PosixEnter() should have failed and we
        should not be calling PosixLeave().
    */
    REDASSERT(gfPosixInited);

  #if REDCONF_TASK_COUNT > 1U
    RedOsMutexRelease();
  #endif
}


/** @brief Check that a mode is consistent with the given expected type.

    @param uMode        An inode mode, indicating whether the inode is a file
                        or a directory.
    @param expectedType The expected type: ::FTYPE_FILE, ::FTYPE_DIR, or
                        ::FTYPE_EITHER.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0               Operation was successful.
    @retval -RED_EISDIR     Expected type is file, actual type is directory.
    @retval -RED_ENOTDIR    Expected type is directory, actual type is file.
*/
static REDSTATUS ModeTypeCheck(
    uint16_t    uMode,
    FTYPE       expectedType)
{
    REDSTATUS   ret;

    if((expectedType == FTYPE_FILE) && RED_S_ISDIR(uMode))
    {
        /*  Expected file, found directory.
        */
        ret = -RED_EISDIR;
    }
    else if((expectedType == FTYPE_DIR) && RED_S_ISREG(uMode))
    {
        /*  Expected directory, found file.
        */
        ret = -RED_ENOTDIR;
    }
    else
    {
        /*  No expected type or found what we expected.
        */
        ret = 0;
    }

    return ret;
}


#if (REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1) || ((REDCONF_API_POSIX_RENAME == 1) && (REDCONF_RENAME_ATOMIC == 1)))
/** @brief Check whether an inode can be unlinked.

    If an inode has a link count of 1 (meaning unlinking another name would
    result in the deletion of the inode) and open handles, it cannot be deleted
    since this would break open handles.

    If an inode is the current working directory, it cannot be deleted since
    this would break the CWD.

    @param ulInode  The inode whose name is to be unlinked.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EBADF  @p ulInode is not a valid inode.
    @retval -RED_EBUSY  The inode has a link count of one and open handles; or
                        #REDCONF_API_POSIX_CWD is true and the inode is the CWD
                        of at least one task.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS InodeUnlinkCheck(
    uint32_t    ulInode)
{
    REDSTATUS   ret;

  #if REDCONF_API_POSIX_LINK == 0
    ret = 0;
  #else
    REDSTAT     InodeStat;

    ret = RedCoreStat(ulInode, &InodeStat);

    /*  We only need to check for open handles if the inode is down to its last
        link.  If it has multiple links, the inode will continue to exist, so
        deleting the name will not break the open handles.
    */
    if((ret == 0) && (InodeStat.st_nlink == 1U))
  #endif
    {
        uint16_t uHandleIdx;

        for(uHandleIdx = 0U; uHandleIdx < REDCONF_HANDLE_COUNT; uHandleIdx++)
        {
            if((gaHandle[uHandleIdx].ulInode == ulInode) && (gaHandle[uHandleIdx].bVolNum == gbRedVolNum))
            {
                ret = -RED_EBUSY;
                break;
            }
        }

      #if REDCONF_API_POSIX_CWD == 1
        /*  The CWD for any task is considered referenced, and cannot be
            deleted.
        */
        if(ret == 0)
        {
          #if REDCONF_TASK_COUNT > 1U
            uint32_t ulTaskIdx;

            for(ulTaskIdx = 0U; ulTaskIdx < REDCONF_TASK_COUNT; ulTaskIdx++)
            {
                if((gaTask[ulTaskIdx].cwd.ulInode == ulInode) && (gaTask[ulTaskIdx].cwd.bVolNum == gbRedVolNum))
                {
                    ret = -RED_EBUSY;
                    break;
                }
            }
          #else
            if((gCwd.ulInode == ulInode) && (gCwd.bVolNum == gbRedVolNum))
            {
                ret = -RED_EBUSY;
            }
          #endif
        }
      #endif
    }

    return ret;
}
#endif


#if REDCONF_TASK_COUNT > 1U
/** @brief Register a task as a file system user, if it is not already
           registered as one.

    The caller must hold the FS mutex.

    @param pulTaskIdx   On successful return, if non-NULL, populated with the
                        index of the task slot assigned to the calling task.
                        This is populated whether or not the task had already
                        been registered.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EUSERS Cannot become a file system user: too many users.
*/
static REDSTATUS TaskRegister(
    uint32_t   *pulTaskIdx)
{
    uint32_t    ulTaskId = RedOsTaskId();
    uint32_t    ulFirstFreeIdx = REDCONF_TASK_COUNT;
    uint32_t    ulIdx;
    REDSTATUS   ret;

    REDASSERT(ulTaskId != 0U);

    /*  Scan the task slots to determine if the task is registered as a file
        system task.
    */
    for(ulIdx = 0U; ulIdx < REDCONF_TASK_COUNT; ulIdx++)
    {
        if(gaTask[ulIdx].ulTaskId == ulTaskId)
        {
            break;
        }

        if((ulFirstFreeIdx == REDCONF_TASK_COUNT) && (gaTask[ulIdx].ulTaskId == 0U))
        {
            ulFirstFreeIdx = ulIdx;
        }
    }

    if(ulIdx == REDCONF_TASK_COUNT)
    {
        /*  Task not already registered.
        */
        if(ulFirstFreeIdx == REDCONF_TASK_COUNT)
        {
            /*  Cannot register task, no more slots.
            */
            ret = -RED_EUSERS;
        }
        else
        {
            /*  Registering task.
            */
            ulIdx = ulFirstFreeIdx;
            gaTask[ulIdx].ulTaskId = ulTaskId;
            ret = 0;
        }
    }
    else
    {
        /*  Task already registered.
        */
        ret = 0;
    }

    if((ret == 0) && (pulTaskIdx != NULL))
    {
        *pulTaskIdx = ulIdx;
    }

    return ret;
}
#endif /* REDCONF_TASK_COUNT > 1U */


#if REDCONF_API_POSIX_CWD == 1
/** @brief Get the current working directory (CWD) for the calling task.

    @return The pointer to the current working directory for the calling task.
*/
static WORKDIR *CwdGet(void)
{
  #if REDCONF_TASK_COUNT == 1U
    /*  Return the one and only CWD.
    */
    return &gCwd;
  #else
    uint32_t    ulIdx;
    uint32_t    ulTaskId = RedOsTaskId();
    WORKDIR    *pCwd = NULL;

    REDASSERT(ulTaskId != 0U);

    for(ulIdx = 0U; ulIdx < REDCONF_TASK_COUNT; ulIdx++)
    {
        if(gaTask[ulIdx].ulTaskId == ulTaskId)
        {
            pCwd = &gaTask[ulIdx].cwd;
            break;
        }
    }

    /*  The task should be registered when this function is called, so its CWD
        should be found.
    */
    REDASSERT(pCwd != NULL);

    return pCwd;
  #endif
}


/** @brief Reset all current working directories (CWD) on the given volume to
           the root directory.

    @param bVolNum  Reset CWDs residing on this volume.
*/
static void CwdResetVol(
    uint8_t     bVolNum)
{
    REDASSERT(bVolNum < REDCONF_VOLUME_COUNT);

  #if REDCONF_TASK_COUNT == 1U
    if(bVolNum == gCwd.bVolNum)
    {
        gCwd.ulInode = INODE_ROOTDIR;
    }
  #else
    {
        uint32_t    ulIdx;

        for(ulIdx = 0U; ulIdx < REDCONF_TASK_COUNT; ulIdx++)
        {
            if(bVolNum == gaTask[ulIdx].cwd.bVolNum)
            {
                gaTask[ulIdx].cwd.ulInode = INODE_ROOTDIR;
            }
        }
    }
  #endif
}


/** @brief Reset all current working directories (CWD) to the default.
*/
static void CwdResetAll(void)
{
  #if REDCONF_TASK_COUNT == 1U
    gCwd.bVolNum = 0U;
    gCwd.ulInode = INODE_ROOTDIR;
  #else
    {
        uint32_t    ulIdx;

        for(ulIdx = 0U; ulIdx < REDCONF_TASK_COUNT; ulIdx++)
        {
            gaTask[ulIdx].cwd.bVolNum = 0U;
            gaTask[ulIdx].cwd.ulInode = INODE_ROOTDIR;
        }
    }
  #endif
}
#endif /* REDCONF_API_POSIX_CWD == 1 */


/** @brief Convert an error value into a simple 0 or -1 return.

    This function is simple, but what it does is needed in many places.  It
    returns zero if @p iError is zero (meaning success) or it returns -1 if
    @p iError is nonzero (meaning error).  Also, if @p iError is nonzero, it
    is saved in red_errno.

    @param  iError  The error value.

    @return Returns 0 if @p iError is 0; otherwise, returns -1.
*/
static int32_t PosixReturn(
    REDSTATUS   iError)
{
    int32_t     iReturn;

    if(iError == 0)
    {
        iReturn = 0;
    }
    else
    {
        iReturn = -1;

        /*  The errors should be negative, and errno positive.
        */
        REDASSERT(iError < 0);
        red_errno = -iError;
    }

    return iReturn;
}


#endif /* REDCONF_API_POSIX == 1 */

