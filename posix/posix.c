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

#if REDCONF_API_POSIX_SYMLINK == 1
  #define RED_O_SYMLINK_IF_ENABLED RED_O_SYMLINK
#else
  #define RED_O_SYMLINK_IF_ENABLED 0U
#endif

/*  Mask of all RED_O_* values.
*/
#define RED_O_MASK \
    (RED_O_RDONLY|RED_O_WRONLY|RED_O_RDWR|RED_O_APPEND|RED_O_CREAT|RED_O_EXCL|RED_O_TRUNC|RED_O_NOFOLLOW|RED_O_SYMLINK_IF_ENABLED)

/*  Mask of all RED_O_* values for a read-only configuration.
*/
#define RED_O_MASK_RDONLY (RED_O_RDONLY|RED_O_NOFOLLOW|RED_O_SYMLINK_IF_ENABLED)

#define HFLAG_DIRECTORY 0x01U   /* Handle is for a directory. */
#define HFLAG_READABLE  0x02U   /* Handle is readable. */
#define HFLAG_WRITEABLE 0x04U   /* Handle is writeable. */
#define HFLAG_APPENDING 0x08U   /* Handle was opened in append mode. */
#define HFLAG_SYMLINK   0x10U   /* Handle is for a symbolic link. */

#define HANDLE_PTR_IS_VALID(h) PTR_IS_ARRAY_ELEMENT((h), gaHandle, ARRAY_SIZE(gaHandle), sizeof(*(h)))

/*  @brief Number of OPENINODE structures needed.
*/
#if REDCONF_API_POSIX_CWD == 1
#define OPEN_INODE_COUNT (REDCONF_HANDLE_COUNT + REDCONF_TASK_COUNT)
#else
#define OPEN_INODE_COUNT REDCONF_HANDLE_COUNT
#endif

#define OIFLAG_ORPHAN   0x01U   /* The link count of the inode is 0. */
#define OIFLAG_RESERVED 0x02U   /* Space has been reserved for writing to the inode. */

#define OI_PTR_IS_VALID(oi) PTR_IS_ARRAY_ELEMENT((oi), gaOpenInos, ARRAY_SIZE(gaOpenInos), sizeof(*(oi)))

/*  @brief Inode information structure, used to store information common to all
           handles for the inode.
*/
typedef struct
{
    uint32_t    ulInode;    /**< Inode number; 0 if slot is available. */
    uint8_t     bVolNum;    /**< Volume containing the inode. */
    uint8_t     bFlags;     /**< Open inode flags. */
    uint16_t    uRefs;      /**< Number of handles open for this inode. */
  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FRESERVE == 1)
    uint64_t    ullResOff;  /**< The offset where reserved inode space starts. */
  #endif
} OPENINODE;

/*  @brief Handle structure, used to implement file descriptors and directory
           streams.
*/
typedef struct sREDHANDLE
{
    OPENINODE  *pOpenIno;   /**< Pointer to the ::OPENINODE structure.  Handle is free if this is NULL. */
    uint8_t     bFlags;     /**< Handle flags (type and mode). */
    union
    {
        uint64_t ullFileOffset;
      #if REDCONF_API_POSIX_READDIR == 1
        uint32_t ulDirPosition;
      #endif
    }           o;          /**< File or directory offset. */
  #if REDCONF_API_POSIX_READDIR == 1
    REDDIRENT   dirent;     /**< Dirent structure returned by red_readdir(). */
  #endif
} REDHANDLE;

/*-------------------------------------------------------------------
    Tasks
-------------------------------------------------------------------*/

/*  @brief Per-task information.
*/
typedef struct
{
  #if REDCONF_TASK_COUNT > 1U
    uint32_t    ulTaskId;   /**< ID of the task which owns this slot; 0 if free. */
  #endif
    REDSTATUS   iErrno;     /**< Last error value. */
  #if REDCONF_API_POSIX_CWD == 1
    OPENINODE  *pCwd;       /**< Current working directory. */
  #endif
} TASKSLOT;

/*-------------------------------------------------------------------
    Local Prototypes
-------------------------------------------------------------------*/

static int32_t ReadSub(int32_t iFildes, void *pBuffer, uint32_t ulLength, bool fIsPread, uint64_t ullOffset);
#if REDCONF_READ_ONLY == 0
static int32_t WriteSub(int32_t iFildes, const void *pBuffer, uint32_t ulLength, bool fIsPwrite, uint64_t ullOffset);
#endif
static REDSTATUS PathStartingPoint(int32_t iDirFildes, const char *pszPath, uint8_t *pbVolNum, uint32_t *pulDirInode, const char **ppszLocalPath);
static REDSTATUS FildesOpen(int32_t iDirFildes, const char *pszPath, uint32_t ulOpenMode, FTYPE type, uint16_t uMode, int32_t *piFildes);
static REDSTATUS FildesClose(int32_t iFildes);
static REDSTATUS FildesToHandle(int32_t iFildes, FTYPE expectedType, REDHANDLE **ppHandle);
static int32_t FildesPack(uint16_t uHandleIdx, uint8_t bVolNum);
static void FildesUnpack(int32_t iFildes, uint16_t *puHandleIdx, uint8_t *pbVolNum, uint16_t *puGeneration);
#if REDCONF_API_POSIX_READDIR == 1
static bool DirStreamIsValid(const REDDIR *pDirStream);
#endif
static REDHANDLE *HandleFindFree(void);
static REDSTATUS HandleOpen(REDHANDLE *pHandle, uint32_t ulInode);
static REDSTATUS HandleClose(REDHANDLE *pHandle, uint32_t ulTransFlag);
static REDSTATUS OpenInoDeref(OPENINODE *pOpenIno, bool fFreeIfOrphaned, bool fPropagateOrphanError);
static OPENINODE *OpenInoFind(uint8_t bVolNum, uint32_t ulInode, bool fAlloc);
static REDSTATUS PosixEnter(void);
static void PosixLeave(void);
#if DELETE_SUPPORTED
static REDSTATUS InodeUnlinkCheck(uint32_t ulInode);
#endif
#if DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1)
static void InodeOrphaned(uint32_t ulInode);
#endif
static REDSTATUS DirInodeToPath(uint32_t ulDirInode, char *pszBuffer, uint32_t ulBufferSize, uint32_t ulFlags);
#if (REDCONF_API_POSIX_CWD == 1) || (REDCONF_TASK_COUNT > 1U)
static TASKSLOT *TaskFind(void);
#endif
#if REDCONF_TASK_COUNT > 1U
static TASKSLOT *TaskRegister(void);
#endif
#if REDCONF_API_POSIX_CWD == 1
static REDSTATUS CwdCloseVol(bool fReset);
static void CwdResetAll(void);
static REDSTATUS CwdClose(TASKSLOT *pTask, bool fClearVol, bool fReset);
#endif
static int32_t PosixReturn(REDSTATUS iError);

/*-------------------------------------------------------------------
    Globals
-------------------------------------------------------------------*/

static bool gfPosixInited;                          /* Whether driver is initialized. */
static OPENINODE gaOpenInos[OPEN_INODE_COUNT];      /* Array of all open inodes. */
static REDHANDLE gaHandle[REDCONF_HANDLE_COUNT];    /* Array of all handles. */
static TASKSLOT gaTask[REDCONF_TASK_COUNT];         /* Array of task slots. */

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
            RedMemSet(gaOpenInos, 0U, sizeof(gaOpenInos));
            RedMemSet(gaTask, 0U, sizeof(gaTask));

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
    REDSTATUS ret = 0;

    if(gfPosixInited)
    {
        uint8_t bVolNum;

      #if REDCONF_TASK_COUNT > 1U
        /*  Not using PosixEnter() to acquire the mutex, since we don't want to
            try and register the calling task as a file system user.
        */
        RedOsMutexAcquire();
      #endif

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
            /*  All volumes are unmounted.  Mark the driver as uninitialized
                before releasing the FS mutex, to avoid any race condition where
                a volume could be mounted and then the driver uninitialized with
                a mounted volume.
            */
            gfPosixInited = false;
        }

      #if REDCONF_TASK_COUNT > 1U
        /*  The FS mutex must be released before we uninitialize the core, since
            the FS mutex needs to be in the released state when it gets
            uninitialized.
        */
        RedOsMutexRelease();
      #endif

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
    - #RED_MOUNT_SKIP_DELETE: If specified, do not clean up orphaned inodes
      before returning from mount.  The orphaned inodes can be reclaimed later,
      either as part of #RED_TRANSACT_VOLFULL transaction points, or via
      red_freeorphans().

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
            uint16_t uHandleIdx;

            /*  If the volume has open handles, return an error -- unless the
                force flag was specified, in which case all open handles are
                closed.
            */
            for(uHandleIdx = 0U; uHandleIdx < REDCONF_HANDLE_COUNT; uHandleIdx++)
            {
                REDHANDLE *pHandle = &gaHandle[uHandleIdx];

                if((pHandle->pOpenIno != NULL) && (pHandle->pOpenIno->bVolNum == gbRedVolNum))
                {
                    if((ulFlags & RED_UMOUNT_FORCE) != 0U)
                    {
                        ret = HandleClose(pHandle, 0U);
                    }
                    else
                    {
                        ret = -RED_EBUSY;
                    }

                    if(ret != 0)
                    {
                        break;
                    }
                }
            }
        }

      #if REDCONF_API_POSIX_CWD == 1
        /*  Close the CWD for any task whose CWD is on the to-be-unmounted
            volume.
        */
        if(ret == 0)
        {
            ret = CwdCloseVol(false);
        }
      #endif

        if(ret == 0)
        {
            ret = RedCoreVolUnmount();
        }

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
    parameter which can change the on-disk layout version and inode count.  In
    the future, it may allow additional aspects of the metadata to be specified
    at run-time.

    Since new members may be added to ::REDFMTOPT, applications should
    zero-initialize the structure to ensure forward compatibility.  For example:

    @code{.c}
    REDFMTOPT fmtopt = {0U};

    fmtopt.ulVersion = RED_DISK_LAYOUT_ORIGINAL;
    fmtopt.ulInodeCount = RED_FORMAT_INODE_COUNT_AUTO;
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
        ret = RedPathVolumeLookup(pszVolume, NULL);

        if(ret == 0)
        {
            uint16_t uHandleIdx;

            /*  Do not rollback the volume if it still has open handles.
            */
            for(uHandleIdx = 0U; uHandleIdx < REDCONF_HANDLE_COUNT; uHandleIdx++)
            {
                const REDHANDLE *pHandle = &gaHandle[uHandleIdx];

                if((pHandle->pOpenIno != NULL) && (pHandle->pOpenIno->bVolNum == gbRedVolNum))
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
            (void)CwdCloseVol(true);
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


#if DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1)
/** @brief Free inodes orphaned before the most recent mount.

    When the last directory entry referring to an inode is unlinked, but there
    are one or more open handles to that inode, the inode becomes orphaned.
    Reliance Edge keeps a list of these orphaned inodes.  When the last open
    handle for an orphaned inode is closed, the orphaned inode is freed.
    However, if the volume is not cleanly unmounted and transacted, either
    because unmount transactions are disabled, or red_umount2() is called with
    #RED_UMOUNT_FORCE, or due to power interruption or other system error, it is
    possible for the list of orphaned inodes to be non-empty when the volume is
    next mounted.  Reliance Edge will by default free all orphaned inodes at
    mount time.  However, doing so could make mount take much longer than
    normal.  Thus, red_mount2() accepts a #RED_MOUNT_SKIP_DELETE flag which will
    cause the orphaned inodes list to be moved to a special "defunct orphaned
    inodes" list, which contains only inodes which were orphaned before the most
    recent mount.  If the defunct orphaned inodes list is not empty, the two
    lists are concatenated, such that immediately after mounting, all orphans
    are in the defunct orphaned inodes list and the orphaned inodes list is
    empty.  This allows the file system to function as expected without freeing
    the orphaned inodes at mount time.

    When there are inodes in the defunct orphan list, and #RED_TRANSACT_VOLFULL
    is enabled, they will be freed automatically if the file system runs out of
    free inodes or free blocks, to reclaim space.

    This API provides a method to free defunct orphaned inodes at a convenient
    time, rather than paying the penalty during mount or write operations.

    @param pszVolume    The path prefix of the volume.
    @param ulCount      The maximum number of defunct orphans to free.  If there
                        are fewer than @p ulCount defunct orphans, all defunct
                        orphans will be freed.

    @return On success, zero is returned when all defunct orphans have been
            freed, and 1 is returned when defunct orphans remain.  On error, -1
            is returned and #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EINVAL: Volume is not mounted; or @p pszVolume is `NULL`; or
      @p ulCount is zero.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ENOENT: @p pszVolume is not a valid volume path prefix.
    - #RED_EROFS: The file system volume is read-only.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_freeorphans(
    const char *pszVolume,
    uint32_t    ulCount)
{
    REDSTATUS   err;
    int32_t     ret;

    err = PosixEnter();
    if(err == 0)
    {
        err = RedPathVolumeLookup(pszVolume, NULL);

        if(err == 0)
        {
            err = RedCoreVolFreeOrphans(ulCount);
            if(err == 0)
            {
                err = 1; /* Success, but defunct orphans remain. */
            }
            else if(err == -RED_ENOENT)
            {
                err = 0; /* No more defunct orphans. */
            }
            else
            {
                /*  Other error: do nothing and propagate it.
                */
            }
        }

        PosixLeave();
    }

    if(err < 0)
    {
        ret = PosixReturn(err);
    }
    else
    {
        ret = err;
    }

    return ret;
}
#endif


/** @brief Open a file or directory.

    Exactly one file access mode must be specified:

    - #RED_O_RDONLY: Open for reading only.
    - #RED_O_WRONLY: Open for writing only.
    - #RED_O_RDWR: Open for reading and writing.

    Directories can only be opened with #RED_O_RDONLY.

    The following flags may also be used:

    - #RED_O_APPEND: Set the file offset to the end-of-file prior to each write.
    - #RED_O_CREAT: Create the named file if it does not exist.
    - #RED_O_EXCL: In combination with `RED_O_CREAT`, return an error if the
      path already exists.
    - #RED_O_TRUNC: Truncate the opened file to size zero.  Only supported when
      #REDCONF_API_POSIX_FTRUNCATE is true.
    - #RED_O_NOFOLLOW: If the final path component is a symbolic link, return a
      #RED_ELOOP error rather than following it.  This flag has no effect except
      when #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are both
      enabled.
    - #RED_O_SYMLINK: Expect the final path component to be a symbolic link and
      fail with a #RED_ENOLINK error if it is not a symbolic link.  This flag
      can be used to open a file descriptor for a symbolic link which can then
      be accessed as if it were a file descriptor for a regular file.  With
      #RED_O_CREAT, this flag can be used to create a symbolic link.  This flag
      is only defined when #REDCONF_API_POSIX_SYMLINK is enabled.

    #RED_O_TRUNC is invalid with #RED_O_RDONLY.  #RED_O_EXCL is invalid without
    #RED_O_CREAT.  #RED_O_NOFOLLOW is invalid with #RED_O_SYMLINK.

    If the volume is read-only, #RED_O_RDONLY is the only valid open flag; use
    of any other flag will result in an error.

    If #RED_O_TRUNC frees data which is in the committed state, it will not
    return to free space until after a transaction point.

    The returned file descriptor must later be closed with red_close().

    Unlike POSIX open, other open flags (like `O_SYNC`) are not supported and
    the third argument for the permissions is not supported.  If RED_O_CREAT is
    specified in @p ulOpenFlags and the file does not exist, the permissions
    default to #RED_S_IREG_DEFAULT.  To create a file with specified
    permissions, see red_open2().

    @param pszPath      The path to the file or directory.
    @param ulOpenFlags  The open flags (mask of `RED_O_` values).

    @return On success, a nonnegative file descriptor is returned.  On error, -1
            is returned and #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: POSIX permissions prohibit the current user from performing
      the operation: no search permission for a component of the prefix in
      @p pszPath; or the indicated path exists but @p ulOpenFlags requires read
      or write permissions which are absent; or the indicated path does not
      exist, #RED_O_CREAT was specified, and no write permission for the parent
      directory where the file would be created.
    - #RED_EEXIST: Using #RED_O_CREAT and #RED_O_EXCL, and the indicated path
      already exists.
    - #RED_EINVAL: @p ulOpenFlags is invalid; or @p pszPath is `NULL`; or the
      volume containing the path is not mounted; or #RED_O_CREAT is included in
      @p ulOpenFlags, and the path ends with dot or dot-dot.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EISDIR: The path names a directory and @p ulOpenFlags includes
      #RED_O_WRONLY or #RED_O_RDWR.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled, and either: a) @p ulOpenFlags includes #RED_O_NOFOLLOW and
      @p pszPath names a symbolic link; or b) @p pszPath cannot be resolved
      because it either contains a symbolic link loop or nested symbolic links
      which exceed the nesting limit.
    - #RED_EMFILE: There are no available file descriptors.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENFILE: Attempting to create a file but the file system has used all
      available inode slots.
    - #RED_ENOENT: #RED_O_CREAT is not set and the named file does not exist; or
      #RED_O_CREAT is set and the parent directory does not exist; or the volume
      does not exist; or the @p pszPath argument points to an empty string (and
      there is no volume with an empty path prefix); or
      #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are both enabled,
      and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link; or, @p ulOpenFlags includes #RED_O_SYMLINK and
      @p pszPath does not name a symbolic link.
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
    uint32_t    ulOpenFlags)
{
    return red_openat(RED_AT_FDNONE, pszPath, ulOpenFlags, RED_S_IREG_DEFAULT);
}


#if (REDCONF_READ_ONLY == 0) && (REDCONF_POSIX_OWNER_PERM == 1)
/** @brief Open a file or directory.

    This function is similar to red_open(), except that it has a third argument
    for specifying the mode bits to use when creating a new file.

    See red_open() for details on the @p ulOpenFlags parameter.

    Unlike POSIX open, other open flags (like `O_SYNC`) are not supported and
    the third argument for the permissions is not optional.

    @param pszPath      The path to the file or directory.
    @param ulOpenFlags  The open flags (mask of `RED_O_` values).
    @param uMode        The mode bits to use in case #RED_O_CREAT is specified
                        in @p ulOpenFlags and the file does not exist.  The
                        supported mode bits are defined in #RED_S_IALLUGO.

    @return On success, a nonnegative file descriptor is returned.  On error, -1
            is returned and #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszPath; or the indicated
      path exists but @p ulOpenFlags requires read or write permissions which
      are absent; or the indicated path does not exist, #RED_O_CREAT was
      specified, and no write permission for the parent directory where the file
      would be created.
    - #RED_EEXIST: Using #RED_O_CREAT and #RED_O_EXCL, and the indicated path
      already exists.
    - #RED_EINVAL: @p ulOpenFlags is invalid; or @p pszPath is `NULL`; or the
      volume containing the path is not mounted; or #RED_O_CREAT is included in
      @p ulOpenFlags, and either the path ends with dot or dot-dot or @p uMode
      includes bits other than #RED_S_IALLUGO.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EISDIR: The path names a directory and @p ulOpenFlags includes
      #RED_O_WRONLY or #RED_O_RDWR.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled, and either: a) @p ulOpenFlags includes #RED_O_NOFOLLOW and
      @p pszPath names a symbolic link; or b) @p pszPath cannot be resolved
      because it either contains a symbolic link loop or nested symbolic links
      which exceed the nesting limit.
    - #RED_EMFILE: There are no available file descriptors.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENFILE: Attempting to create a file but the file system has used all
      available inode slots.
    - #RED_ENOENT: #RED_O_CREAT is not set and the named file does not exist; or
      #RED_O_CREAT is set and the parent directory does not exist; or the volume
      does not exist; or the @p pszPath argument points to an empty string (and
      there is no volume with an empty path prefix); or
      #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are both enabled,
      and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link; or, @p ulOpenFlags includes #RED_O_SYMLINK and
      @p pszPath does not name a symbolic link.
    - #RED_ENOSPC: The file does not exist and #RED_O_CREAT was specified, but
      there is insufficient free space to expand the directory or to create the
      new file.
    - #RED_ENOTDIR: A component of the prefix in @p pszPath does not name a
      directory.
    - #RED_EROFS: The path resides on a read-only file system and a write
      operation was requested.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_open2(
    const char *pszPath,
    uint32_t    ulOpenFlags,
    uint16_t    uMode)
{
    return red_openat(RED_AT_FDNONE, pszPath, ulOpenFlags, uMode);
}
#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_POSIX_OWNER_PERM == 1) */


/** @brief Open a file or directory, optionally via a path which is relative to
           a given directory.

    This function is similar to red_open() or red_open2(), except that it
    optionally supports parsing a relative path starting from a directory
    specified via file descriptor.

    See red_open() for details on the @p ulOpenFlags parameter.

    @param iDirFildes   File descriptor for the directory from which @p pszPath,
                        if it is a relative path, should be parsed.  May also be
                        one of the pseudo file descriptors: #RED_AT_FDCWD,
                        #RED_AT_FDABS, or #RED_AT_FDNONE; see the documentation
                        of those macros for details.
    @param pszPath      The path to the file or directory.  This may be an
                        absolute path, in which case @p iDirFildes is ignored;
                        or it may be a relative path, in which case it is parsed
                        with @p iDirFildes as the starting point.
    @param ulOpenFlags  The open flags (mask of `RED_O_` values).
    @param uMode        The mode bits to use in case #RED_O_CREAT is specified
                        in @p ulOpenFlags and the file does not exist.  The
                        supported mode bits are defined in #RED_S_IALLUGO.  This
                        parameter has no effect if #REDCONF_POSIX_OWNER_PERM is
                        false.

    @return On success, a nonnegative file descriptor is returned.  On error, -1
            is returned and #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for @p pDirFildes; or no search permission for a component of
      the prefix in @p pszPath; or the indicated path exists but @p ulOpenFlags
      requires read or write permissions which are absent; or the indicated path
      does not exist, #RED_O_CREAT was specified, and no write permission for
      the parent directory where the file would be created.
    - #RED_EBADF: @p pszPath does not specify an absolute path and @p iDirFildes
      is neither a valid pseudo file descriptor nor a valid file descriptor open
      for reading.
    - #RED_EEXIST: Using #RED_O_CREAT and #RED_O_EXCL, and the indicated path
      already exists.
    - #RED_EINVAL: @p ulOpenFlags is invalid; or @p pszPath is `NULL`; or the
      volume containing the path is not mounted; or #RED_O_CREAT is included in
      @p ulOpenFlags, and the path ends with dot or dot-dot; or #RED_O_CREAT is
      included in @p ulOpenFlags, and #REDCONF_POSIX_OWNER_PERM is true, and
      @p uMode includes bits other than #RED_S_IALLUGO.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EISDIR: The path names a directory and @p ulOpenFlags includes
      #RED_O_WRONLY or #RED_O_RDWR.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled, and either: a) @p ulOpenFlags includes #RED_O_NOFOLLOW and
      @p pszPath names a symbolic link; or b) @p pszPath cannot be resolved
      because it either contains a symbolic link loop or nested symbolic links
      which exceed the nesting limit.
    - #RED_EMFILE: There are no available file descriptors.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENFILE: Attempting to create a file but the file system has used all
      available inode slots.
    - #RED_ENOENT: #RED_O_CREAT is not set and the named file does not exist; or
      #RED_O_CREAT is set and the parent directory does not exist; or the volume
      does not exist; or the @p pszPath argument points to an empty string (and
      there is no volume with an empty path prefix); or
      #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are both enabled,
      and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link; or, @p ulOpenFlags includes #RED_O_SYMLINK and
      @p pszPath does not name a symbolic link.
    - #RED_ENOSPC: The file does not exist and #RED_O_CREAT was specified, but
      there is insufficient free space to expand the directory or to create the
      new file.
    - #RED_ENOTDIR: A component of the prefix in @p pszPath does not name a
      directory; or @p pszPath does not specify an absolute path and
      @p iDirFildes is a valid file descriptor for a non-directory.
    - #RED_EROFS: The path resides on a read-only file system and a write
      operation was requested.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_openat(
    int32_t     iDirFildes,
    const char *pszPath,
    uint32_t    ulOpenFlags,
    uint16_t    uMode)
{
    int32_t     iFildes = -1;   /* Init'd to quiet warnings. */
    REDSTATUS   ret;

    ret = PosixEnter();

    if(ret == 0)
    {
      #if REDCONF_READ_ONLY == 1
        if((ulOpenFlags & RED_O_MASK_RDONLY) != ulOpenFlags)
        {
            ret = -RED_EROFS;
        }
      #else
        if(    (ulOpenFlags != (ulOpenFlags & RED_O_MASK))
            || ((ulOpenFlags & (RED_O_RDONLY|RED_O_WRONLY|RED_O_RDWR)) == 0U)
            || (((ulOpenFlags & RED_O_RDONLY) != 0U) && ((ulOpenFlags & (RED_O_WRONLY|RED_O_RDWR)) != 0U))
            || (((ulOpenFlags & RED_O_WRONLY) != 0U) && ((ulOpenFlags & (RED_O_RDONLY|RED_O_RDWR)) != 0U))
            || (((ulOpenFlags & RED_O_RDWR) != 0U) && ((ulOpenFlags & (RED_O_RDONLY|RED_O_WRONLY)) != 0U))
            || (((ulOpenFlags & RED_O_TRUNC) != 0U) && ((ulOpenFlags & RED_O_RDONLY) != 0U))
            || (((ulOpenFlags & RED_O_EXCL) != 0U) && ((ulOpenFlags & RED_O_CREAT) == 0U))
            || (((ulOpenFlags & RED_O_CREAT) != 0U) && ((uMode & ~RED_S_IALLUGO) != 0U)))
        {
            ret = -RED_EINVAL;
        }
      #if REDCONF_API_POSIX_FTRUNCATE == 0
        else if((ulOpenFlags & RED_O_TRUNC) != 0U)
        {
            ret = -RED_EINVAL;
        }
      #endif
      #endif
      #if REDCONF_API_POSIX_SYMLINK == 1
        else if(((ulOpenFlags & RED_O_NOFOLLOW) != 0U) && ((ulOpenFlags & RED_O_SYMLINK) != 0U))
        {
            ret = -RED_EINVAL;
        }
      #endif
        else
        {
            uint16_t    uOpenMode;
            FTYPE       expectedType;

          #if REDCONF_POSIX_OWNER_PERM == 1
            uOpenMode = uMode;
          #else
            /*  If uMode were passed into FildesOpen(), there would be an error
                if it included unsupported bits.  Since it is documented to have
                "no effect" in this configuration, don't use uMode at all.
            */
            uOpenMode = RED_S_IREG_DEFAULT;
            (void)uMode;
          #endif

          #if REDCONF_API_POSIX_SYMLINK == 1
            if((ulOpenFlags & RED_O_SYMLINK) != 0U)
            {
                uOpenMode |= RED_S_IFLNK;
                expectedType = FTYPE_SYMLINK;
            }
            else
          #endif
            {
                uOpenMode |= RED_S_IFREG;
                expectedType = FTYPE_FILE | FTYPE_DIR;
            }

            ret = FildesOpen(iDirFildes, pszPath, ulOpenFlags, expectedType, uOpenMode, &iFildes);
        }

        PosixLeave();
    }

    if(ret != 0)
    {
        iFildes = PosixReturn(ret);
    }

    return iFildes;
}


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_SYMLINK == 1)
/** @brief Create a symbolic link.

    @param pszPath      The target for the symbolic link; i.e., the path that
                        the symbolic link will point at.  This path will be
                        stored verbatim; it will not be parsed in any way.
    @param pszSymlink   The path to the symbolic link to create.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszSymlink; or no write
      permission for the parent directory where the symlink would be created.
    - #RED_EEXIST: @p pszSymlink points to an existing file or directory.
    - #RED_EINVAL: @p pszPath is `NULL`; or @p pszSymlink is `NULL`; or the
      volume containing the @p pszSymlink path is not mounted; or @p pszSymlink
      ends with dot or dot-dot.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ENAMETOOLONG: The length of a component of @p pszSymlink is longer
      than #REDCONF_NAME_MAX.
    - #RED_ELOOP: #REDOSCONF_SYMLINK_FOLLOW is enabled and @p pszSymlink cannot
      be resolved because it either contains a symbolic link loop or nested
      symbolic links which exceed the nesting limit.
    - #RED_ENFILE: No available inodes to create the symbolic link.
    - #RED_ENOENT: A component of the @p pszSymlink path prefix does not exist;
      or @p pszSymlink is an empty string; or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDOSCONF_SYMLINK_FOLLOW is disabled and resolving
      @p pszSymlink requires following a symbolic link.
    - #RED_ENOSPC: There is insufficient free space to expand the directory or
      to create the new symbolic link.
    - #RED_ENOTDIR: A component of the prefix in @p pszSymlink does not name a
      directory.
    - #RED_EROFS: @p pszSymlink resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_symlink(
    const char *pszPath,
    const char *pszSymlink)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        if(pszPath == NULL)
        {
            ret = -RED_EINVAL;
        }
        else
        {
            uint32_t    ulDirInode;
            const char *pszLocalPath;

            ret = PathStartingPoint(RED_AT_FDNONE, pszSymlink, NULL, &ulDirInode, &pszLocalPath);
            if(ret == 0)
            {
                uint32_t    ulPInode;
                const char *pszName;

                ret = RedPathToName(ulDirInode, pszLocalPath, -RED_EISDIR, &ulPInode, &pszName);
                if(ret == 0)
                {
                    uint32_t ulInode;

                    ret = RedCoreCreate(ulPInode, pszName, RED_S_IFLNK | (RED_S_IRWXUGO & RED_S_IFVALID), &ulInode);
                    if(ret == 0)
                    {
                        uint32_t ulPathLen = RedStrLen(pszPath) + 1U;
                        uint32_t ulLenWrote = ulPathLen;

                        ret = RedCoreFileWrite(ulInode, 0U, &ulLenWrote, pszPath);
                        if((ret == 0) && (ulLenWrote != ulPathLen))
                        {
                            ret = -RED_ENOSPC;
                        }

                        /*  If the write failed, delete the empty symbolic link.
                        */
                        if(ret != 0)
                        {
                            REDSTATUS ret2;

                            ret2 = RedCoreUnlink(ulPInode, pszName, false);
                            if(ret2 != 0)
                            {
                                /*  Some write errors are expected (like ENOSPC)
                                    but all unlink errors are catastrophic, so
                                    given precedence to the unlink error.
                                */
                                ret = ret2;
                            }
                        }
                    }
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_SYMLINK == 1) */


#if REDCONF_API_POSIX_SYMLINK == 1
/** @brief Read the contents of a symbolic link.

    @note On success, @p pszBuffer will be null-terminated by this function if
          @p ulBufferSize exceeded the length of the target path stored for
          @p pszSymlink.  This differs from the readlink() implementations in
          most POSIX-like systems (such as Linux and the *BSDs), which _never_
          include the null-terminator.  If @p ulBufferSize is less than or equal
          to the length of the target path, the @p pszBuffer will _not_ be
          null-terminated by this function.

    The caller must handle the case where the symbolic link target was too large
    to fit into the buffer.  This can be done by looking for a return value
    which is equal to @p ulBufferSize.  For example:

    @code{.c}
    char szBuffer[256];
    int32_t len;

    len = red_readlink(pszSymlink, szBuffer, sizeof(szBuffer));
    if(len == (int32_t)sizeof(szBuffer))
    {
        // Optionally return an error
        err = RED_ENAMETOOLONG;
        // Optionally null-terminate the buffer to use the truncated target path
        szBuffer[sizeof(szBuffer) - 1U] = '\0';
    }
    @endcode

    @param pszSymlink   The path to the symbolic link to read.
    @param pszBuffer    The buffer to populate with the target of the the
                        symbolic link.
    @param ulBufferSize The size of @p pszBuffer, in bytes.  If the length of
                        the symbolic link target is greater than or equal to the
                        value, then the target string is truncated and no null
                        terminator is written to @p pszBuffer.

    @return On success, returns the length of the symlink target (not including
            any null-terminator) or @p ulBufferSize, whichever is smaller.  On
            error, -1 is returned and #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszSymlink.
    - #RED_EINVAL: @p pszSymlink is `NULL`; or @p pszBuffer is `NULL`; or the
      volume containing the @p pszSymlink path is not mounted; or @p pszSymlink
      exists but is not a symbolic link; or @p ulBufferSize is larger than
      `INT32_MAX`.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDOSCONF_SYMLINK_FOLLOW is enabled and @p pszSymlink cannot
      be resolved because it either contains a symbolic link loop or nested
      symbolic links which exceed the nesting limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszSymlink is longer
      than #REDCONF_NAME_MAX.
    - #RED_ENOENT: @p pszSymlink does not exist or is an empty path string; or
      #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are both enabled,
      and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDOSCONF_SYMLINK_FOLLOW is disabled and resolving
      @p pszSymlink requires following a symbolic link.
    - #RED_ENOTDIR: A component of the prefix in @p pszSymlink does not name a
      directory.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_readlink(
    const char *pszSymlink,
    char       *pszBuffer,
    uint32_t    ulBufferSize)
{
    uint32_t    ulLenRead = 0U;
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        if(ulBufferSize > (uint32_t)INT32_MAX)
        {
            ret = -RED_EINVAL;
        }
        else
        {
            uint32_t    ulDirInode;
            const char *pszLocalPath;

            ret = PathStartingPoint(RED_AT_FDNONE, pszSymlink, NULL, &ulDirInode, &pszLocalPath);
            if(ret == 0)
            {
                uint32_t ulInode;

                ret = RedPathLookup(ulDirInode, pszLocalPath, RED_AT_SYMLINK_NOFOLLOW, &ulInode);

                if(ret == 0)
                {
                    REDSTAT sb;

                    ret = RedCoreStat(ulInode, &sb);
                    if(ret == 0)
                    {
                        ret = RedModeTypeCheck(sb.st_mode, FTYPE_SYMLINK);
                        if(ret == -RED_ENOLINK)
                        {
                            ret = -RED_EINVAL;
                        }
                    }
                }

                if(ret == 0)
                {
                    ulLenRead = ulBufferSize;
                    ret = RedCoreFileRead(ulInode, 0U, &ulLenRead, pszBuffer);
                }

                /*  The POSIX readlink() specification is somewhat vague, but
                    most implementations (including Linux and the *BSDs) do
                    the following: read the contents of the symlink (_not_
                    including the NUL terminator) into the buffer and return
                    the number of bytes copied into the buffer.  The buffer
                    is *never* NUL terminated, even if there is room for a NUL.
                    This is a poor API, since in most cases the callers will
                    need to NUL-terminate the string to use it, and failure to
                    do so could lead to subtle bugs.

                    red_symlink() (unlike most implementations) will write
                    the NUL terminator to disk as part of the file data for
                    the symlink.  However, we can't assume that the symlink
                    is NUL terminated.  Reliance Edge has the RED_O_SYMLINK
                    extension which allows symlinks to be opened as file
                    descriptors and to have arbitrary contents written into
                    them.  This means that the symlinks might not end with a
                    NUL, or it might have a NUL character before the EOF.

                    So -- as a compromise between POSIX compliance, convenience,
                    and Reliance Edge's extensions -- we do the following:
                */
                if(ret == 0)
                {
                    uint32_t i;

                    /*  Add a NUL terminator if there's room for it and it's not
                        already there.  In most cases, it'll already be there in
                        symlink file data, but we can't assume that, due to
                        RED_O_SYMLINK.

                        The typical readlink() implementation never writes the
                        NUL, whether or not there's room for it, but we are
                        deliberately deviating from that.
                    */
                    if(    (ulLenRead < ulBufferSize)
                        && ((ulLenRead == 0U) || (pszBuffer[ulLenRead - 1U] != '\0')))
                    {
                        pszBuffer[ulLenRead] = '\0';
                    }

                    /*  If the symlink contains a NUL terminator _before_ the
                        EOF, the length is reduced so that the NUL terminator is
                        treated as the "end" of the symlink.  This means that
                        bytes after &pszBuffer[<return value>] are potentially
                        modified by this function.  This is allowed by POSIX,
                        which says: "If the number of bytes in the symbolic link
                        is less than bufsize, the contents of the remainder of
                        buf are unspecified."

                        As a side effect, the NUL terminator (whether or not
                        it exists on disk) is _not_ included in the returned
                        length.  This makes the red_readlink() return value
                        compatible with the return value of other readlink()
                        implementations.
                    */
                    for(i = 0U; i < ulLenRead; i++)
                    {
                        if(pszBuffer[i] == '\0')
                        {
                            ulLenRead = i;
                            break;
                        }
                    }
                }
            }
        }

        PosixLeave();
    }

    if(ret == 0)
    {
        ret = (int32_t)ulLenRead;
    }
    else
    {
        ret = PosixReturn(ret);
    }

    return ret;
}
#endif /* REDCONF_API_POSIX_SYMLINK == 1 */


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_UNLINK == 1)
/** @brief Delete a file or directory.

    The given name is deleted and the link count of the corresponding inode is
    decremented.  If the link count falls to zero (no remaining hard links), the
    inode will be deleted.

    If #REDCONF_DELETE_OPEN is true, then deleting a file or directory with open
    handles (file descriptors or directory streams) works as in POSIX unlink.
    If #REDCONF_DELETE_OPEN is false, then unlike POSIX unlink, deleting a file
    or directory with open handles will fail with an #RED_EBUSY error.  This
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
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszPath; or no write
      permission for the parent directory where the name would be removed.
    - #RED_EBUSY: @p pszPath names the root directory; or #REDCONF_DELETE_OPEN
      is false and either: a) @p pszPath points to an inode with open handles
      and a link count of one, or b) #REDCONF_API_POSIX_CWD is true and
      @p pszPath points to the CWD of a task.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or #REDCONF_API_POSIX_CWD is true and the path ends with dot
      or dot-dot.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: The path does not name an existing file; or the @p pszPath
      argument points to an empty string (and there is no volume with an empty
      path prefix); or #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW
      are both enabled, and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOTDIR: A component of the path prefix is not a directory.
    - #RED_ENOTEMPTY: The path names a directory which is not empty.
    - #RED_ENOSPC: The file system does not have enough space to modify the
      parent directory to perform the deletion.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_unlink(
    const char *pszPath)
{
    return red_unlinkat(RED_AT_FDNONE, pszPath, 0U);
}
#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_UNLINK == 1) */


#if (REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1))
/** @brief Delete a file or directory, optionally via a path which is relative
           to a given directory.

    This function is similar to red_unlink() or red_rmdir(), except that it
    optionally supports parsing a relative path starting from a directory
    specified via file descriptor.

    See red_unlink() and red_rmdir() for further details on unlinking files and
    directories which also apply to this function.

    @param iDirFildes   File descriptor for the directory from which @p pszPath,
                        if it is a relative path, should be parsed.  May also be
                        one of the pseudo file descriptors: #RED_AT_FDCWD,
                        #RED_AT_FDABS, or #RED_AT_FDNONE; see the documentation
                        of those macros for details.
    @param pszPath      The path to the file or directory to delete.  This may
                        be an absolute path, in which case @p iDirFildes is
                        ignored; or it may be a relative path, in which case it
                        is parsed with @p iDirFildes as the starting point.
    @param ulFlags      Unlink flags.  The only flag value is #RED_AT_REMOVEDIR,
                        which means to return #RED_ENOTDIR if @p pszPath names a
                        non-directory, just like red_rmdir().  When
                        #REDCONF_API_POSIX_RMDIR is false, #RED_AT_REMOVEDIR is
                        prohibited; when #REDCONF_API_POSIX_UNLINK is false,
                        #RED_AT_REMOVEDIR is required.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for @p iDirFildes; or no search permission for a component of
      the prefix in @p pszPath; or no write permission for the parent directory
      where the name would be removed.
    - #RED_EBADF: @p pszPath does not specify an absolute path and @p iDirFildes
      is neither a valid pseudo file descriptor nor a valid file descriptor open
      for reading.
    - #RED_EBUSY: @p pszPath names the root directory; or #REDCONF_DELETE_OPEN
      is false and either: a) @p pszPath points to an inode with open handles
      and a link count of one, or b) #REDCONF_API_POSIX_CWD is true and
      @p pszPath points to the CWD of a task.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or #REDCONF_API_POSIX_CWD is true and the path ends with dot
      or dot-dot; or @p ulFlags is invalid.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: The path does not name an existing file; or the @p pszPath
      argument points to an empty string (and there is no volume with an empty
      path prefix); or #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW
      are both enabled, and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOTDIR: A component of the prefix in @p pszPath does not name a
      directory; or @p pszPath does not specify an absolute path and
      @p iDirFildes is a valid file descriptor for a non-directory.
    - #RED_ENOTEMPTY: The path names a directory which is not empty.
    - #RED_ENOSPC: The file system does not have enough space to modify the
      parent directory to perform the deletion.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_unlinkat(
    int32_t     iDirFildes,
    const char *pszPath,
    uint32_t    ulFlags)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        /*  RED_AT_REMOVEDIR is the only supported flag.  It is prohibited when
            rmdir is disabled and required when unlink is disabled.
        */
      #if REDCONF_API_POSIX_RMDIR == 0
        if(ulFlags != 0U)
      #elif REDCONF_API_POSIX_UNLINK == 0
        if(ulFlags != RED_AT_REMOVEDIR)
      #else
        if((ulFlags & ~RED_AT_REMOVEDIR) != 0U)
      #endif
        {
            ret = -RED_EINVAL;
        }
        else
        {
            uint32_t    ulDirInode;
            const char *pszLocalPath;

            ret = PathStartingPoint(iDirFildes, pszPath, NULL, &ulDirInode, &pszLocalPath);
            if(ret == 0)
            {
                const char *pszName;
                uint32_t    ulPInode;

                ret = RedPathToName(ulDirInode, pszLocalPath, -RED_EBUSY, &ulPInode, &pszName);
                if(ret == 0)
                {
                    uint32_t ulInode;

                    ret = RedCoreLookup(ulPInode, pszName, &ulInode);

                  #if REDCONF_API_POSIX_RMDIR == 1
                    /*  Skip the stat if RedModeTypeCheck() is guaranteed to
                        pass, which is the case when RED_AT_REMOVEDIR is absent,
                        since Reliance Edge allows directories to be unlinked by
                        red_unlink() or by this function without that flag.
                    */
                    if((ret == 0) && ((ulFlags & RED_AT_REMOVEDIR) != 0U))
                    {
                        REDSTAT InodeStat;

                        ret = RedCoreStat(ulInode, &InodeStat);
                        if(ret == 0)
                        {
                            ret = RedModeTypeCheck(InodeStat.st_mode, FTYPE_DIR);
                          #if REDCONF_API_POSIX_SYMLINK == 1
                            if(ret == -RED_ENOLINK)
                            {
                                ret = -RED_ENOTDIR;
                            }
                          #endif
                        }
                    }
                  #endif

                    if(ret == 0)
                    {
                        bool fOrphan = false;

                        ret = InodeUnlinkCheck(ulInode);

                      #if REDCONF_DELETE_OPEN == 1
                        if(ret == -RED_EBUSY)
                        {
                            fOrphan = true;
                            ret = 0;
                        }
                      #endif

                        if(ret == 0)
                        {
                            ret = RedCoreUnlink(ulPInode, pszName, fOrphan);
                        }

                      #if REDCONF_DELETE_OPEN == 1
                        if((ret == 0) && fOrphan)
                        {
                            InodeOrphaned(ulInode);
                        }
                      #endif
                    }
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif /* (REDCONF_READ_ONLY == 0) && ((REDCONF_API_POSIX_UNLINK == 1) || (REDCONF_API_POSIX_RMDIR == 1)) */


#if (REDCONF_READ_ONLY == 0) && (REDCONF_POSIX_OWNER_PERM == 1)
/** @brief Change the mode of a file or directory.

    @param pszPath  The name and location of the file or directory to change the
                    mode of.
    @param uMode    The new mode bits for the file or directory.  The supported
                    mode bits are defined in #RED_S_IALLUGO.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszPath.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or @p uMode contains bits other than #RED_S_IALLUGO.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of the path prefix does not name an existing
      directory; or the @p pszPath argument points to an empty string (and there
      is no volume with an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOTDIR: A component of the path prefix is not a directory.
    - #RED_EROFS: The file or directory resides on a read-only file system.
    - #RED_EPERM: The current user is unprivileged and is not the owner of the
      file or directory indicated by @p pszPath.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_chmod(
    const char *pszPath,
    uint16_t    uMode)
{
    return red_fchmodat(RED_AT_FDNONE, pszPath, uMode, 0U);
}


/** @brief Change the mode of a file or directory, optionally via a path which
           is relative to a given directory.

    This function is similar to red_chmod(), except that it optionally supports
    parsing a relative path starting from a directory specified via file
    descriptor.

    @param iDirFildes   File descriptor for the directory from which @p pszPath,
                        if it is a relative path, should be parsed.  May also be
                        one of the pseudo file descriptors: #RED_AT_FDCWD,
                        #RED_AT_FDABS, or #RED_AT_FDNONE; see the documentation
                        of those macros for details.
    @param pszPath      The name and location of the file or directory to change
                        the mode of.  This may be an absolute path, in which
                        case @p iDirFildes is ignored; or it may be a relative
                        path, in which case it is parsed with @p iDirFildes as
                        the starting point.
    @param uMode        The new mode bits for the file or directory.  The
                        supported mode bits are defined in #RED_S_IALLUGO.
    @param ulFlags      Chmod flags.  The only flag value is
                        #RED_AT_SYMLINK_NOFOLLOW, which means that if @p pszPath
                        names a symbolic link, change the mode of the symbolic
                        link itself rather than what the link points at.  The
                        #RED_AT_SYMLINK_NOFOLLOW flag is permitted (but has no
                        effect) when symbolic links are disabled.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for @p iDirFildes; or no search permission for a component of
      the prefix in @p pszPath.
    - #RED_EBADF: @p pszPath does not specify an absolute path and @p iDirFildes
      is neither a valid pseudo file descriptor nor a valid file descriptor open
      for reading.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or @p uMode contains bits other than #RED_S_IALLUGO; or
      @p ulFlags is invalid.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of the path prefix does not name an existing
      directory; or the @p pszPath argument points to an empty string (and there
      is no volume with an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOTDIR: A component of the prefix in @p pszPath does not name a
      directory; or @p pszPath does not specify an absolute path and
      @p iDirFildes is a valid file descriptor for a non-directory.
    - #RED_EPERM: The current user is unprivileged and is not the owner of the
      file or directory indicated by @p pszPath.
    - #RED_EROFS: The file or directory resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_fchmodat(
    int32_t     iDirFildes,
    const char *pszPath,
    uint16_t    uMode,
    uint32_t    ulFlags)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        if((ulFlags & RED_AT_SYMLINK_NOFOLLOW) != ulFlags)
        {
            ret = -RED_EINVAL;
        }
        else
        {
            uint32_t    ulDirInode;
            const char *pszLocalPath;

            ret = PathStartingPoint(iDirFildes, pszPath, NULL, &ulDirInode, &pszLocalPath);
            if(ret == 0)
            {
                uint32_t ulInode;

                ret = RedPathLookup(ulDirInode, pszLocalPath, ulFlags, &ulInode);
                if(ret == 0)
                {
                    ret = RedCoreChmod(ulInode, uMode);
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


/** @brief Change the mode of an open file or directory.

    @param iFildes  A file descriptor for the file or directory to change the
                    mode of.
    @param uMode    The new mode bits for the file or directory.  The supported
                    mode bits are defined in #RED_S_IALLUGO.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor.
    - #RED_EINVAL: @p uMode contains bits other than #RED_S_IALLUGO.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EPERM: The current user is unprivileged and is not the owner of the
      file or directory underlying @p iFildes.
    - #RED_EROFS: The file or directory resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_fchmod(
    int32_t     iFildes,
    uint16_t    uMode)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        REDHANDLE *pHandle;

        ret = FildesToHandle(iFildes, FTYPE_ANY, &pHandle);

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->pOpenIno->bVolNum);
        }
      #endif

        if(ret == 0)
        {
            ret = RedCoreChmod(pHandle->pOpenIno->ulInode, uMode);
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


/** @brief Change the user and group ownership of a file or directory.

    @param pszPath  The name and location of the file or directory to change the
                    ownership of.
    @param ulUID    The new user ID for the file or directory.  A value of
                    #RED_UID_KEEPSAME indicates that the user ID will not be
                    changed.
    @param ulGID    The new group ID for the file or directory.  A value of
                    #RED_GID_KEEPSAME indicates that the group ID will not be
                    changed.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: POSIX permissions prohibit the current user from performing
      the operation: no search permission for a component of the prefix in
      @p pszPath.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of the path prefix does not name an existing
      directory; or the @p pszPath argument points to an empty string (and there
      is no volume with an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOTDIR: A component of the path prefix is not a directory.
    - #RED_EPERM: The current user is unprivileged and @p ulUID is neither
      #RED_UID_KEEPSAME nor the current UID of the file or directory.
    - #RED_EROFS: The file or directory resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_chown(
    const char *pszPath,
    uint32_t    ulUID,
    uint32_t    ulGID)
{
    return red_fchownat(RED_AT_FDNONE, pszPath, ulUID, ulGID, 0U);
}


/** @brief Change the user and group ownership of a file or directory,
           optionally via a path which is relative to a given directory.

    This function is similar to red_chown(), except that it optionally supports
    parsing a relative path starting from a directory specified via file
    descriptor.

    @param iDirFildes   File descriptor for the directory from which @p pszPath,
                        if it is a relative path, should be parsed.  May also be
                        one of the pseudo file descriptors: #RED_AT_FDCWD,
                        #RED_AT_FDABS, or #RED_AT_FDNONE; see the documentation
                        of those macros for details.
    @param pszPath      The name and location of the file or directory to change
                        the ownership of.  This may be an absolute path, in
                        which case @p iDirFildes is ignored; or it may be a
                        relative path, in which case it is parsed with
                        @p iDirFildes as the starting point.
    @param ulUID        The new user ID for the file or directory.  A value of
                        #RED_UID_KEEPSAME indicates that the user ID will not be
                        changed.
    @param ulGID        The new group ID for the file or directory.  A value of
                        #RED_GID_KEEPSAME indicates that the group ID will not
                        be changed.
    @param ulFlags      Chown flags.  The only flag value is
                        #RED_AT_SYMLINK_NOFOLLOW, which means that if @p pszPath
                        names a symbolic link, change the ownership of the
                        symbolic link itself rather than what the link points
                        at.  The #RED_AT_SYMLINK_NOFOLLOW flag is permitted (but
                        has no effect) when symbolic links are disabled.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: POSIX permissions prohibit the current user from performing
      the operation: no search permission for @p iDirFildes; or no search
      permission for a component of the prefix in @p pszPath.
    - #RED_EBADF: @p pszPath does not specify an absolute path and @p iDirFildes
      is neither a valid pseudo file descriptor nor a valid file descriptor open
      for reading.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or @p ulFlags is invalid.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of the path prefix does not name an existing
      directory; or the @p pszPath argument points to an empty string (and there
      is no volume with an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOTDIR: A component of the prefix in @p pszPath does not name a
      directory; or @p pszPath does not specify an absolute path and
      @p iDirFildes is a valid file descriptor for a non-directory.
    - #RED_EPERM: The current user is unprivileged and @p ulUID is neither
      #RED_UID_KEEPSAME nor the current UID of the file or directory.
    - #RED_EROFS: The file or directory resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_fchownat(
    int32_t     iDirFildes,
    const char *pszPath,
    uint32_t    ulUID,
    uint32_t    ulGID,
    uint32_t    ulFlags)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        if((ulFlags & RED_AT_SYMLINK_NOFOLLOW) != ulFlags)
        {
            ret = -RED_EINVAL;
        }
        else
        {
            uint32_t    ulDirInode;
            const char *pszLocalPath;

            ret = PathStartingPoint(iDirFildes, pszPath, NULL, &ulDirInode, &pszLocalPath);
            if(ret == 0)
            {
                uint32_t ulInode;

                ret = RedPathLookup(ulDirInode, pszLocalPath, ulFlags, &ulInode);
                if(ret == 0)
                {
                    ret = RedCoreChown(ulInode, ulUID, ulGID);
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


/** @brief Change the user and group ownership of an open file or directory.

    @param iFildes  A file descriptor for the file or directory to change the
                    ownership of.
    @param ulUID    The new user ID for the file or directory.  A value of
                    #RED_UID_KEEPSAME indicates that the user ID will not be
                    changed.
    @param ulGID    The new group ID for the file or directory.  A value of
                    #RED_GID_KEEPSAME indicates that the group ID will not be
                    changed.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EPERM: The current user is unprivileged and @p ulUID is neither
      #RED_UID_KEEPSAME nor the current UID of the file or directory.
    - #RED_EROFS: The file or directory resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_fchown(
    int32_t     iFildes,
    uint32_t    ulUID,
    uint32_t    ulGID)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        REDHANDLE *pHandle;

        ret = FildesToHandle(iFildes, FTYPE_ANY, &pHandle);

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->pOpenIno->bVolNum);
        }
      #endif

        if(ret == 0)
        {
            ret = RedCoreChown(pHandle->pOpenIno->ulInode, ulUID, ulGID);
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_POSIX_OWNER_PERM == 1) */


#if (REDCONF_READ_ONLY == 0) && (REDCONF_INODE_TIMESTAMPS == 1)
/** @brief Change the access and modification times of the file or directory.

    @param pszPath  The name and location of the file or directory to change the
                    times of.
    @param pulTimes Pointer to an array of two timestamps, expressed as the
                    number of seconds since 1970-01-01, where @p pulTimes[0]
                    specifies the new access time and @p pulTimes[1] specifies
                    the new modification time.  If @p pulTimes is `NULL`, the
                    access and modification times of the file or directory are
                    set to the current time.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszPath; or @p pulTimes is
      `NULL`, the current user is unprivileged, and the current user is neither
      the owner of the file or directory named by @p pszPath nor is write
      permission granted for that file or directory.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of the path prefix does not name an existing
      directory; or the @p pszPath argument points to an empty string (and there
      is no volume with an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOTDIR: A component of the path prefix is not a directory.
    - #RED_EPERM: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: @p pulTimes is
      _not_ `NULL`, and the current user is neither privileged nor the owner of
      the file or directory named by @p pszPath.
    - #RED_EROFS: The file or directory resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_utimes(
    const char     *pszPath,
    const uint32_t *pulTimes)
{
    return red_utimesat(RED_AT_FDNONE, pszPath, pulTimes, 0U);
}


/** @brief Change the access and modification times of the file or directory,
           optionally via a path which is relative to a given directory.

    This function is similar to red_utimes(), except that it optionally supports
    parsing a relative path starting from a directory specified via file
    descriptor.

    @param iDirFildes   File descriptor for the directory from which @p pszPath,
                        if it is a relative path, should be parsed.  May also be
                        one of the pseudo file descriptors: #RED_AT_FDCWD,
                        #RED_AT_FDABS, or #RED_AT_FDNONE; see the documentation
                        of those macros for details.
    @param pszPath      The name and location of the file or directory to change
                        the times of.  This may be an absolute path, in which
                        case @p iDirFildes is ignored; or it may be a relative
                        path, in which case it is parsed with @p iDirFildes as
                        the starting point.
    @param pulTimes     Pointer to an array of two timestamps, expressed as the
                        number of seconds since 1970-01-01, where @p pulTimes[0]
                        specifies the new access time and @p pulTimes[1]
                        specifies the new modification time.  If @p pulTimes is
                        `NULL`, the access and modification times of the file or
                        directory are set to the current time.
    @param ulFlags      Utimes flags.  The only flag value is
                        #RED_AT_SYMLINK_NOFOLLOW, which means that if @p pszPath
                        names a symbolic link, change the timestamps of the
                        symbolic link itself rather than what the link points
                        at.  The #RED_AT_SYMLINK_NOFOLLOW flag is permitted (but
                        has no effect) when symbolic links are disabled.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for @p iDirFildes; or no search permission for a component of
      the prefix in @p pszPath; or @p pulTimes is `NULL`, the current user is
      unprivileged, and the current user is neither the owner of the file or
      directory named by @p pszPath nor is write permission granted for that
      file or directory.
    - #RED_EBADF: @p pszPath does not specify an absolute path and @p iDirFildes
      is neither a valid pseudo file descriptor nor a valid file descriptor open
      for reading.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or @p ulFlags is invalid.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of the path prefix does not name an existing
      directory; or the @p pszPath argument points to an empty string (and there
      is no volume with an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOTDIR: A component of the prefix in @p pszPath does not name a
      directory; or @p pszPath does not specify an absolute path and
      @p iDirFildes is a valid file descriptor for a non-directory.
    - #RED_EPERM: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: @p pulTimes is
      _not_ `NULL`, and the current user is neither privileged nor the owner of
      the file or directory named by @p pszPath.
    - #RED_EROFS: The file or directory resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_utimesat(
    int32_t         iDirFildes,
    const char     *pszPath,
    const uint32_t *pulTimes,
    uint32_t        ulFlags)
{
    REDSTATUS       ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        if((ulFlags & RED_AT_SYMLINK_NOFOLLOW) != ulFlags)
        {
            ret = -RED_EINVAL;
        }
        else
        {
            uint32_t    ulDirInode;
            const char *pszLocalPath;

            ret = PathStartingPoint(iDirFildes, pszPath, NULL, &ulDirInode, &pszLocalPath);
            if(ret == 0)
            {
                uint32_t ulInode;

                ret = RedPathLookup(ulDirInode, pszLocalPath, ulFlags, &ulInode);
                if(ret == 0)
                {
                    ret = RedCoreUTimes(ulInode, pulTimes);
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


/** @brief Change the access and modification times of the file or directory.

    @param iFildes  The file descriptor of the file or directory to change the
                    times of.
    @param pulTimes Pointer to an array of two timestamps, expressed as the
                    number of seconds since 1970-01-01, where @p pulTimes[0]
                    specifies the new access time and @p pulTimes[1] specifies
                    the new modification time.  If @p pulTimes is `NULL`, the
                    access and modification times of the file or directory are
                    set to the current time.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: @p pulTimes is
      `NULL`, the current user is unprivileged, and the current user is neither
      the owner of the file or directory underlying @p iFildes nor is write
      permission granted for that file or directory.
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EPERM: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: @p pulTimes is
      _not_ `NULL`, and the current user is neither privileged nor the owner of
      the file or directory underlying @p iFildes.
    - #RED_EROFS: The file descriptor resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_futimes(
    int32_t         iFildes,
    const uint32_t *pulTimes)
{
    REDSTATUS       ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        REDHANDLE *pHandle;

        ret = FildesToHandle(iFildes, FTYPE_ANY, &pHandle);

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->pOpenIno->bVolNum);
        }
      #endif

        if(ret == 0)
        {
            ret = RedCoreUTimes(pHandle->pOpenIno->ulInode, pulTimes);
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_INODE_TIMESTAMPS == 1) */


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_MKDIR == 1)
/** @brief Create a new directory.

    Unlike POSIX mkdir, this function has no second argument for the
    permissions, which default to #RED_S_IDIR_DEFAULT.  To create a directory
    with specified permissions, see red_mkdir2().

    @param pszPath  The name and location of the directory to create.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszPath; or no write
      permission for the parent directory where the directory would be created.
    - #RED_EEXIST: @p pszPath points to an existing file or directory.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or the path ends with dot or dot-dot.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENFILE: No available inodes to create the directory.
    - #RED_ENOENT: A component of the path prefix does not name an existing
      directory; or the @p pszPath argument points to an empty string (and there
      is no volume with an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOSPC: The file system does not have enough space for the new
      directory or to extend the parent directory of the new directory.
    - #RED_ENOTDIR: A component of the path prefix is not a directory.
    - #RED_EROFS: The parent directory resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_mkdir(
    const char *pszPath)
{
    return red_mkdirat(RED_AT_FDNONE, pszPath, RED_S_IDIR_DEFAULT);
}


#if REDCONF_POSIX_OWNER_PERM == 1
/** @brief Create a new directory.

    @param pszPath  The name and location of the directory to create.
    @param uMode    The mode bits for the new directory.  The supported mode
                    bits are defined in #RED_S_IALLUGO.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: POSIX permissions prohibit the current user from performing
      the operation: no search permission for a component of the prefix in
      @p pszPath; or no write permission for the parent directory where the
      directory would be created.
    - #RED_EEXIST: @p pszPath points to an existing file or directory.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or the path ends with dot or dot-dot; or @p uMode includes
      bits other than #RED_S_IALLUGO.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENFILE: No available inodes to create the directory.
    - #RED_ENOENT: A component of the path prefix does not name an existing
      directory; or the @p pszPath argument points to an empty string (and there
      is no volume with an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOSPC: The file system does not have enough space for the new
      directory or to extend the parent directory of the new directory.
    - #RED_ENOTDIR: A component of the path prefix is not a directory.
    - #RED_EROFS: The parent directory resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_mkdir2(
    const char *pszPath,
    uint16_t    uMode)
{
    return red_mkdirat(RED_AT_FDNONE, pszPath, uMode);
}
#endif /* REDCONF_POSIX_OWNER_PERM == 1 */


/** @brief Create a new directory, optionally via a path which is relative to a
           given directory.

    This function is similar to red_mkdir(), except that it optionally supports
    parsing a relative path starting from a directory specified via file
    descriptor.

    @param iDirFildes   File descriptor for the directory from which @p pszPath,
                        if it is a relative path, should be parsed.  May also be
                        one of the pseudo file descriptors: #RED_AT_FDCWD,
                        #RED_AT_FDABS, or #RED_AT_FDNONE; see the documentation
                        of those macros for details.
    @param pszPath      The name and location of the directory to create.  This
                        may be an absolute path, in which case @p iDirFildes is
                        ignored; or it may be a relative path, in which case it
                        is parsed with @p iDirFildes as the starting point.
    @param uMode        The mode bits for the new directory.  The supported mode
                        bits are defined in #RED_S_IALLUGO.  This parameter has
                        no effect if #REDCONF_POSIX_OWNER_PERM is false.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for @p iDirFildes; or no search permission for a component of
      the prefix in @p pszPath; or no write permission for the parent directory
      where the directory would be created.
    - #RED_EBADF: @p pszPath does not specify an absolute path and @p iDirFildes
      is neither a valid pseudo file descriptor nor a valid file descriptor open
      for reading.
    - #RED_EEXIST: @p pszPath points to an existing file or directory.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or the path ends with dot or dot-dot; or
      #REDCONF_POSIX_OWNER_PERM is true and @p uMode includes bits other than
      #RED_S_IALLUGO.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENFILE: No available inodes to create the directory.
    - #RED_ENOENT: A component of the path prefix does not name an existing
      directory; or the @p pszPath argument points to an empty string (and there
      is no volume with an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOSPC: The file system does not have enough space for the new
      directory or to extend the parent directory of the new directory.
    - #RED_ENOTDIR: A component of the prefix in @p pszPath does not name a
      directory; or @p pszPath does not specify an absolute path and
      @p iDirFildes is a valid file descriptor for a non-directory.
    - #RED_EROFS: The parent directory resides on a read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_mkdirat(
    int32_t     iDirFildes,
    const char *pszPath,
    uint16_t    uMode)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        uint32_t    ulDirInode;
        const char *pszLocalPath;

        ret = PathStartingPoint(iDirFildes, pszPath, NULL, &ulDirInode, &pszLocalPath);
        if(ret == 0)
        {
            const char *pszName;
            uint32_t    ulPInode;

            ret = RedPathToName(ulDirInode, pszLocalPath, -RED_EEXIST, &ulPInode, &pszName);
            if(ret == 0)
            {
                uint32_t ulInode;
                uint16_t uMkdirMode;

              #if REDCONF_POSIX_OWNER_PERM == 0
                /*  If uMode were passed into RedCoreCreate(), there would be an
                    error if it included unsupported bits.  Since it is
                    documented to have "no effect" in this configuration, don't
                    use uMode at all.
                */
                uMkdirMode = RED_S_IDIR_DEFAULT;

                (void)uMode;
              #else
                uMkdirMode = uMode;

                if((uMkdirMode & RED_S_IALLUGO) != uMkdirMode)
                {
                    ret = -RED_EINVAL;
                }
                else
              #endif
                {
                    ret = RedCoreCreate(ulPInode, pszName, RED_S_IFDIR | uMkdirMode, &ulInode);
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_MKDIR == 1) */


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_RMDIR == 1)
/** @brief Delete a directory.

    The given directory name is deleted and the corresponding directory inode
    will be deleted.

    If #REDCONF_DELETE_OPEN is true, then deleting a directory with open handles
    (file descriptors or directory streams) works as in POSIX rmdir.  If
    #REDCONF_DELETE_OPEN is false, then unlike  POSIX rmdir, deleting a
    directory with open handles will fail with an #RED_EBUSY error.

    If the path names a directory which is not empty, the deletion will fail.
    If the path names the root directory of a file system volume, the deletion
    will fail.

    If the path names a regular file, the deletion will fail.  This provides
    type checking and may be useful in cases where an application knows the path
    to be deleted should name a directory.

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
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszPath; or no write
      permission for the parent directory where the directory would be removed.
    - #RED_EBUSY: @p pszPath names the root directory; or #REDCONF_DELETE_OPEN
      is false and either: a) @p pszPath points to a directory with open
      handles, or b) #REDCONF_API_POSIX_CWD is true and @p pszPath points to the
      CWD of a task.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted; or #REDCONF_API_POSIX_CWD is true and the path ends with dot
      or dot-dot.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: The path does not name an existing directory; or the
      @p pszPath argument points to an empty string (and there is no volume with
      an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
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
    return red_unlinkat(RED_AT_FDNONE, pszPath, RED_AT_REMOVEDIR);
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
    If @p pszNewPath points to an inode with a link count of one and open
    handles (file descriptors or directory streams), then:
    - If #REDCONF_DELETE_OPEN is true, then the rename succeeds as with POSIX
      rename.
    - If #REDCONF_DELETE_OPEN is false, then unlike POSIX rename, the rename
      will fail with #RED_EBUSY.

    If the rename deletes the old destination, it may free data in the committed
    state, which will not return to free space until after a transaction point.
    Similarly, if the deleted inode was part of the committed state, the inode
    slot will not be available until after a transaction point.

    @param pszOldPath   The path of the file or directory to rename.
    @param pszNewPath   The new name and location after the rename.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszOldPath or
      @p pszNewPath; or no write permission for the parent directories of
      @p pszOldPath or @p pszNewPath.
    - #RED_EBUSY: @p pszOldPath or @p pszNewPath names the root directory; or
      #REDCONF_RENAME_ATOMIC is true and #REDCONF_DELETE_OPEN is false and
      either a) @p pszNewPath points to an inode with open handles and a link
      count of one or b) #REDCONF_API_POSIX_CWD is true and the @p pszNewPath
      points to an inode which is the CWD of at least one task.
    - #RED_EEXIST: #REDCONF_RENAME_ATOMIC is false and @p pszNewPath exists.
    - #RED_EINVAL: @p pszOldPath is `NULL`; or @p pszNewPath is `NULL`; or the
      volume containing the path is not mounted; or #REDCONF_API_POSIX_CWD is
      true and either path ends with dot or dot-dot; or #REDCONF_API_POSIX_CWD
      is false and @p pszNewPath ends with dot or dot-dot; or the rename is
      cyclic.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EISDIR: The @p pszNewPath argument names a directory and the
      @p pszOldPath argument names a non-directory.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszOldPath or @p pszNewPath cannot be resolved because
      it either contains a symbolic link loop or nested symbolic links which
      exceed the nesting limit.
    - #RED_ENAMETOOLONG: The length of a component of either @p pszOldPath or
      @p pszNewPath is longer than #REDCONF_NAME_MAX.
    - #RED_ENOENT: The link named by @p pszOldPath does not name an existing
      entry; or either @p pszOldPath or @p pszNewPath point to an empty string
      (and there is no volume with an empty path prefix); or
      #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are both enabled,
      and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving either path requires
      following a symbolic link.
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
    return red_renameat(RED_AT_FDNONE, pszOldPath, RED_AT_FDNONE, pszNewPath);
}


/** @brief Rename a file or directory, optionally via paths which are relative
           to given directories.

    This function is similar to red_rename(), except that it optionally supports
    parsing relative paths starting from directories specified via file
    descriptor.

    See red_rename() for further details on renaming which also apply to this
    function.

    @param iOldDirFildes    File descriptor for the directory from which
                            @p pszOldPath, if it is a relative path, should be
                            parsed.  May also be one of the pseudo file
                            descriptors: #RED_AT_FDCWD, #RED_AT_FDABS, or
                            #RED_AT_FDNONE; see the documentation of those
                            macros for details.
    @param pszOldPath       The path of the file or directory to rename.  This
                            may be an absolute path, in which case
                            @p iOldDirFildes is ignored; or it may be a relative
                            path, in which case it is parsed with
                            @p iOldDirFildes as the starting point.
    @param iNewDirFildes    File descriptor for the directory from which
                            @p pszNewPath, if it is a relative path, should be
                            parsed.  May also be one of the pseudo file
                            descriptors, same as @p iOldDirFildes.  May be equal
                            to @p iOldDirFildes.
    @param pszNewPath       The new name and location after the rename.  This
                            may be an absolute path or a relative path, just
                            like @p pszOldPath, except that relative paths are
                            parsed relative to @p iNewDirFildes as the starting
                            point.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for @p iOldDirFildes or @p iNewDirFildes; or no search
      permission for a component of the prefix in @p pszOldPath or
      @p pszNewPath; or no write permission for the parent directories of
      @p pszOldPath or @p pszNewPath.
    - #RED_EBADF: @p pszOldPath and/or @p pszNewPath do not specify an absolute
      path and @p iOldDirFildes and/or @p iNewDirFildes (respectively) are
      neither valid pseudo file descriptors nor valid file descriptors open for
      reading.
    - #RED_EBUSY: @p pszOldPath or @p pszNewPath names the root directory; or
      #REDCONF_RENAME_ATOMIC is true and #REDCONF_DELETE_OPEN is false and
      either a) @p pszNewPath points to an inode with open handles and a link
      count of one or b) #REDCONF_API_POSIX_CWD is true and the @p pszNewPath
      points to an inode which is the CWD of at least one task.
    - #RED_EEXIST: #REDCONF_RENAME_ATOMIC is false and @p pszNewPath exists.
    - #RED_EINVAL: @p pszOldPath is `NULL`; or @p pszNewPath is `NULL`; or the
      volume containing the path is not mounted; or #REDCONF_API_POSIX_CWD is
      true and either path ends with dot or dot-dot; or #REDCONF_API_POSIX_CWD
      is false and @p pszNewPath ends with dot or dot-dot; or the rename is
      cyclic.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EISDIR: The @p pszNewPath argument names a directory and the
      @p pszOldPath argument names a non-directory.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszOldPath or @p pszNewPath cannot be resolved because
      it either contains a symbolic link loop or nested symbolic links which
      exceed the nesting limit.
    - #RED_ENAMETOOLONG: The length of a component of either @p pszOldPath or
      @p pszNewPath is longer than #REDCONF_NAME_MAX.
    - #RED_ENOENT: The link named by @p pszOldPath does not name an existing
      entry; or either @p pszOldPath or @p pszNewPath point to an empty string
      (and there is no volume with an empty path prefix); or
      #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are both enabled,
      and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving either path requires
      following a symbolic link.
    - #RED_ENOTDIR: A component of either path prefix is not a directory; or
      @p pszOldPath names a directory and @p pszNewPath names a file; or either
      path does not specify an absolute path and its corresponding directory
      file descriptor (@p iOldDirFildes or @p iNewDirFildes) is a valid file
      descriptor for a non-directory.
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
int32_t red_renameat(
    int32_t     iOldDirFildes,
    const char *pszOldPath,
    int32_t     iNewDirFildes,
    const char *pszNewPath)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        uint8_t     bOldVolNum;
        uint32_t    ulOldCwdInode;
        const char *pszOldLocalPath;

        ret = PathStartingPoint(iOldDirFildes, pszOldPath, &bOldVolNum, &ulOldCwdInode, &pszOldLocalPath);
        if(ret == 0)
        {
            uint8_t     bNewVolNum;
            uint32_t    ulNewCwdInode;
            const char *pszNewLocalPath;

            ret = PathStartingPoint(iNewDirFildes, pszNewPath, &bNewVolNum, &ulNewCwdInode, &pszNewLocalPath);

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
                    uint32_t    ulDestInode = INODE_INVALID;
                    bool        fOrphan = false;

                    ret = RedPathToName(ulNewCwdInode, pszNewLocalPath, -RED_EBUSY, &ulNewPInode, &pszNewName);

                  #if REDCONF_RENAME_ATOMIC == 1
                    if(ret == 0)
                    {
                        ret = RedCoreLookup(ulNewPInode, pszNewName, &ulDestInode);
                        if(ret == 0)
                        {
                            ret = InodeUnlinkCheck(ulDestInode);

                          #if REDCONF_DELETE_OPEN == 1
                            if(ret == -RED_EBUSY)
                            {
                                fOrphan = true;
                                ret = 0;
                            }
                          #endif
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
                        ret = RedCoreRename(ulOldPInode, pszOldName, ulNewPInode, pszNewName, fOrphan);
                    }

                  #if (REDCONF_RENAME_ATOMIC == 1) && (REDCONF_DELETE_OPEN == 1)
                    if((ret == 0) && fOrphan)
                    {
                        InodeOrphaned(ulDestInode);
                    }
                  #endif
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_RENAME == 1) */


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_LINK == 1)
/** @brief Create a hard link.

    This creates an additional name (link) for the file named by @p pszPath.
    The new name refers to the same file with the same contents.  If a name is
    deleted, but the underlying file has other names, the file continues to
    exist.  The link count (accessible via red_fstat()) indicates the number of
    names that a file has.  All of a file's names are on equal footing: there is
    nothing special about the original name.

    If @p pszPath names a directory, the operation will fail.

    @param pszPath      The path indicating the inode for the new link.
    @param pszHardLink  The name and location for the new link.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszPath or @p pszHardLink;
      or no write permission for the parent directory of @p pszHardLink.
    - #RED_EEXIST: @p pszHardLink resolves to an existing file.
    - #RED_EINVAL: @p pszPath or @p pszHardLink is `NULL`; or the volume
      containing the paths is not mounted; or #REDCONF_API_POSIX_CWD is true and
      @p pszHardLink ends with dot or dot-dot.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath or @p pszHardLink cannot be resolved because
      it either contains a symbolic link loop or nested symbolic links which
      exceed the nesting limit.
    - #RED_EMLINK: Creating the link would exceed the maximum link count of the
      inode named by @p pszPath.
    - #RED_ENAMETOOLONG: The length of a component of either @p pszPath or
      @p pszHardLink is longer than #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of either path prefix does not exist; or the file
      named by @p pszPath does not exist; or either @p pszPath or @p pszHardLink
      point to an empty string (and there is no volume with an empty path
      prefix); or #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled, and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving either path requires
      following a symbolic link.
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
    return red_linkat(RED_AT_FDNONE, pszPath, RED_AT_FDNONE, pszHardLink, 0U);
}


/** @brief Create a hard link, optionally via paths which are relative to given
           directories.

    This function is similar to red_link(), except that it optionally supports
    parsing relative paths starting from directories specified via file
    descriptor.

    See red_link() for further details on renaming which also apply to this
    function.

    @param iDirFildes           File descriptor for the directory from which
                                @p pszPath, if it is a relative path, should
                                be parsed.  May also be one of the pseudo file
                                descriptors: #RED_AT_FDCWD, #RED_AT_FDABS, or
                                #RED_AT_FDNONE; see the documentation of those
                                macros for details.
    @param pszPath              The path indicating the inode for the new link.
                                This may be an absolute path, in which case
                                @p iDirFildes is ignored; or it may be a
                                relative path, in which case it is parsed with
                                @p iDirFildes as the starting point.
    @param iHardLinkDirFildes   File descriptor for the directory from which
                                @p pszHardLink, if it is a relative path, should
                                be parsed.  May also be one of the pseudo file
                                descriptors, same as @p iDirFildes.  May be
                                equal to @p iDirFildes.
    @param pszHardLink          The name and location for the new link.  This
                                may be an absolute path or a relative path, just
                                like @p pszPath, except that relative paths are
                                parsed relative to @p iHardLinkDirFildes as the
                                starting point.
    @param ulFlags              Link flags.  The only flag value is
                                #RED_AT_SYMLINK_FOLLOW, which means that if
                                @p pszPath names a symbolic link, follow the
                                symbolic link and create a hard link to what it
                                resolves to, rather than creating a hard link
                                which points at the symbolic link itself.  The
                                #RED_AT_SYMLINK_FOLLOW flag is permitted (but
                                has no effect) when symbolic links are disabled.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for @p iDirFildes or @p iHardLinkDirFildes; or no search
      permission for a component of the prefix in @p pszPath or @p pszHardLink;
      or no write permission for the parent directory of @p pszHardLink.
    - #RED_EBADF: @p pszPath and/or @p pszHardLink do not specify an absolute
      path and @p iDirFildes and/or @p iHardLinkDirFildes (respectively) are
      neither valid pseudo file descriptors nor valid file descriptors open for
      reading.
    - #RED_EEXIST: @p pszHardLink resolves to an existing file.
    - #RED_EINVAL: @p pszPath or @p pszHardLink is `NULL`; or the volume
      containing the paths is not mounted; or @p pszHardLink ends with dot or
      dot-dot; or @p ulFlags is invalid.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath or @p pszHardLink cannot be resolved because
      it either contains a symbolic link loop or nested symbolic links which
      exceed the nesting limit.
    - #RED_EMLINK: Creating the link would exceed the maximum link count of the
      inode named by @p pszPath.
    - #RED_ENAMETOOLONG: The length of a component of either @p pszPath or
      @p pszHardLink is longer than #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of either path prefix does not exist; or the file
      named by @p pszPath does not exist; or either @p pszPath or @p pszHardLink
      point to an empty string (and there is no volume with an empty path
      prefix); or #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled, and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving either path requires
      following a symbolic link.
    - #RED_ENOSPC: There is insufficient free space to expand the directory that
      would contain the link.
    - #RED_ENOTDIR: A component of either path prefix is not a directory; or
      either path does not specify an absolute path and its corresponding
      directory file descriptor (@p iDirFildes or @p iHardLinkDirFildes) is a
      valid file descriptor for a non-directory.
    - #RED_EPERM: The @p pszPath argument names a directory.
    - #RED_EROFS: The requested link requires writing in a directory on a
      read-only file system.
    - #RED_EUSERS: Cannot become a file system user: too many users.
    - #RED_EXDEV: @p pszPath and @p pszHardLink are on different file system
      volumes.
*/
int32_t red_linkat(
    int32_t     iDirFildes,
    const char *pszPath,
    int32_t     iHardLinkDirFildes,
    const char *pszHardLink,
    uint32_t    ulFlags)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        if((ulFlags & RED_AT_SYMLINK_FOLLOW) != ulFlags)
        {
            ret = -RED_EINVAL;
        }
        else
        {
            uint8_t     bVolNum;
            uint32_t    ulDirInode;
            const char *pszLocalPath;

            ret = PathStartingPoint(iDirFildes, pszPath, &bVolNum, &ulDirInode, &pszLocalPath);
            if(ret == 0)
            {
                uint8_t     bLinkVolNum;
                uint32_t    ulLinkCwdInode;
                const char *pszLinkLocalPath;

                ret = PathStartingPoint(iHardLinkDirFildes, pszHardLink, &bLinkVolNum, &ulLinkCwdInode, &pszLinkLocalPath);

                if((ret == 0) && (bVolNum != bLinkVolNum))
                {
                    ret = -RED_EXDEV;
                }

                if(ret == 0)
                {
                    uint32_t ulInode;
                    uint32_t ulLookupFlags = 0U;

                  #if (REDCONF_API_POSIX_SYMLINK == 1) && (REDOSCONF_SYMLINK_FOLLOW == 1)
                    /*  linkat(), compared to the other *at() APIs, has the
                        reverse following behavior and the reverse flag.
                        Translate accordingly, since RedPathLookup() only
                        implements the NOFOLLOW flag.
                    */
                    if((ulFlags & RED_AT_SYMLINK_FOLLOW) == 0U)
                    {
                        ulLookupFlags |= RED_AT_SYMLINK_NOFOLLOW;
                    }
                  #endif

                    ret = RedPathLookup(ulDirInode, pszLocalPath, ulLookupFlags, &ulInode);
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
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_LINK == 1) */


/** @brief Get the status of a file or directory.

    See the ::REDSTAT type for the details of the information returned.

    @param pszPath  The path of the file or directory whose status is to be
                    retrieved.
    @param pStat    Pointer to a ::REDSTAT buffer to populate.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszPath.
    - #RED_EINVAL: @p pszPath is `NULL`; or @p pStat is `NULL`; or the volume
      containing the path is not mounted.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: The path does not name an existing file or directory; or the
      @p pszPath argument points to an empty string (and there is no volume with
      an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOTDIR: A component of the path prefix is not a directory.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_stat(
    const char *pszPath,
    REDSTAT    *pStat)
{
    return red_fstatat(RED_AT_FDNONE, pszPath, pStat, 0U);
}


/** @brief Get the status of a file or directory, optionally via a path which is
           relative to a given directory.

    This function is similar to red_stat(), except that it optionally supports
    parsing a relative path starting from a directory specified via file
    descriptor.

    See the ::REDSTAT type for the details of the information returned.

    @param iDirFildes   File descriptor for the directory from which @p pszPath,
                        if it is a relative path, should be parsed.  May also be
                        one of the pseudo file descriptors: #RED_AT_FDCWD,
                        #RED_AT_FDABS, or #RED_AT_FDNONE; see the documentation
                        of those macros for details.
    @param pszPath      The path of the file or directory whose status is to be
                        retrieved.  This may be an absolute path, in which case
                        @p iDirFildes is ignored; or it may be a relative path,
                        in which case it is parsed with @p iDirFildes as the
                        starting point.
    @param pStat        Pointer to a ::REDSTAT buffer to populate.
    @param ulFlags      Stat flags.  The only flag value is
                        #RED_AT_SYMLINK_NOFOLLOW, which means that if @p pszPath
                        names a symbolic link, get the status of the symbolic
                        link itself rather than what the link points at.  The
                        #RED_AT_SYMLINK_NOFOLLOW flag is permitted (but has no
                        effect) when symbolic links are disabled.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for @p iDirFildes; or no search permission for a component of
      the prefix in @p pszPath.
    - #RED_EBADF: @p pszPath does not specify an absolute path and @p iDirFildes
      is neither a valid pseudo file descriptor nor a valid file descriptor open
      for reading.
    - #RED_EINVAL: @p pszPath is `NULL`; or @p pStat is `NULL`; or @p ulFlags is
      invalid; or the volume containing the path is not mounted.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: The path does not name an existing directory; or the
      @p pszPath argument points to an empty string (and there is no volume with
      an empty path prefix); or #REDCONF_API_POSIX_SYMLINK and
      #REDOSCONF_SYMLINK_FOLLOW are both enabled, and path resolution
      encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
    - #RED_ENOTDIR: A component of the prefix in @p pszPath does not name a
      directory; or @p pszPath does not specify an absolute path and
      @p iDirFildes is a valid file descriptor for a non-directory.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_fstatat(
    int32_t     iDirFildes,
    const char *pszPath,
    REDSTAT    *pStat,
    uint32_t    ulFlags)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        if((ulFlags & RED_AT_SYMLINK_NOFOLLOW) != ulFlags)
        {
            ret = -RED_EINVAL;
        }
        else
        {
            uint32_t    ulDirInode;
            const char *pszLocalPath;

            ret = PathStartingPoint(iDirFildes, pszPath, NULL, &ulDirInode, &pszLocalPath);
            if(ret == 0)
            {
                uint32_t ulInode;

                ret = RedPathLookup(ulDirInode, pszLocalPath, ulFlags, &ulInode);
                if(ret == 0)
                {
                    ret = RedCoreStat(ulInode, pStat);
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


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

        ret = FildesToHandle(iFildes, FTYPE_ANY, &pHandle);

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->pOpenIno->bVolNum);
        }
      #endif

        if(ret == 0)
        {
            ret = RedCoreStat(pHandle->pOpenIno->ulInode, pStat);
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}


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
    return ReadSub(iFildes, pBuffer, ulLength, false, 0U);
}


/** @brief Read from an open file at a given position.

    Equivalent to red_read(), except that reading starts at the given position
    and the file offset is not modified.

    @param iFildes      The file descriptor from which to read.
    @param pBuffer      The buffer to populate with data read.  Must be at least
                        @p ulLength bytes in size.
    @param ulLength     Number of bytes to attempt to read.
    @param ullOffset    The file offset at which to read.

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
int32_t red_pread(
    int32_t     iFildes,
    void       *pBuffer,
    uint32_t    ulLength,
    uint64_t    ullOffset)
{
    return ReadSub(iFildes, pBuffer, ulLength, true, ullOffset);
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
      cannot be returned properly; or #REDCONF_API_POSIX_FRESERVE is true and
      space was reserved with red_freserve() but is being written
      non-sequentially.
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
    return WriteSub(iFildes, pBuffer, ulLength, false, 0U);
}


/** @brief Write to an open file at a given position.

    Equivalent to red_write(), except that writing starts at the given position
    and the file offset is not modified.

    @param iFildes      The file descriptor to write to.
    @param pBuffer      The buffer containing the data to be written.  Must be
                        at least @p ulLength bytes in size.
    @param ulLength     Number of bytes to attempt to write.
    @param ullOffset    The file offset at which to write.

    @return On success, returns a nonnegative value indicating the number of
            bytes actually written.  On error, -1 is returned and #red_errno is
            set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor open
      for writing.  This includes the case where the file descriptor is for a
      directory.
    - #RED_EFBIG: No data can be written to the @p ullOffset file offset since
      the resulting file size would exceed the maximum file size.
    - #RED_EINVAL: @p pBuffer is `NULL`; or @p ulLength exceeds INT32_MAX and
      cannot be returned properly; or #REDCONF_API_POSIX_FRESERVE is true and
      space was reserved with red_freserve() but is being written
      non-sequentially.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ENOSPC: No data can be written because there is insufficient free
      space.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_pwrite(
    int32_t     iFildes,
    const void *pBuffer,
    uint32_t    ulLength,
    uint64_t    ullOffset)
{
    return WriteSub(iFildes, pBuffer, ulLength, true, ullOffset);
}


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

        ret = FildesToHandle(iFildes, FTYPE_ANY, &pHandle);

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->pOpenIno->bVolNum);
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
#endif /* REDCONF_READ_ONLY == 0 */


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
        ret = FildesToHandle(iFildes, FTYPE_NOTDIR, &pHandle);

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pHandle->pOpenIno->bVolNum);
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

                    ret = RedCoreStat(pHandle->pOpenIno->ulInode, &s);
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
        OPENINODE *pOpenIno = NULL;

        ret = FildesToHandle(iFildes, FTYPE_NOTDIR, &pHandle);
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

        if(ret == 0)
        {
            pOpenIno = pHandle->pOpenIno;
          #if REDCONF_VOLUME_COUNT > 1U
            ret = RedCoreVolSetCurrent(pOpenIno->bVolNum);
          #endif
        }

      #if REDCONF_API_POSIX_FRESERVE == 1
        if((ret == 0) && ((pOpenIno->bFlags & OIFLAG_RESERVED) != 0U))
        {
            ret = RedCoreFileUnreserve(pOpenIno->ulInode, pOpenIno->ullResOff);

            if(ret == 0)
            {
                pOpenIno->bFlags &= ~OIFLAG_RESERVED;
            }
        }
      #endif

        if(ret == 0)
        {
            ret = RedCoreFileTruncate(pOpenIno->ulInode, ullSize);
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif


#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FRESERVE == 1)
/** @brief Expand a file and reserve space to allow writing the expanded region.

    The intended use case for this function is when an application intends to
    write a file of a known size and wants to ensure ahead-of-time that there is
    space to write the entire file.

    This function will increase the file size to @p ullSize and reserve space to
    allow writing the region between the old file size and @p ullSize.  The
    reserved area may _only_ be written sequentially.  The writes may occur via
    multiple file descriptors, but the writes to the underlying inode must be
    sequential.  When the entire reserved area has been written, the file may
    once again be written non-sequentially.

    The space reservation is _not_ persistent.  The reservation goes away when:
    -# All file descriptors for the underlying inode are closed.
    -# The file is truncated via red_ftruncate().
    -# When the volume is unmounted.
    -# After an unclean shutdown (power loss or system failure).

    If, after using this function, the application determines that not all of
    the reserved space is needed, the file can be truncated with red_ftruncate()
    to correct the file size and unreserve the unneeded space.

    The value of the file offset in the file descriptor is not modified by this
    function.

    @param iFildes  The file descriptor of the file for which to reserve space.
    @param ullSize  The new size of the file.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor open
      for writing.  This includes the case where the file descriptor is for a
      directory.
    - #RED_EFBIG: @p ullSize exceeds the maximum file size.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_EINVAL: Space has already been reserved for this file; or @p ullSize
      is less than or equal to the file size.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled and @p iFildes is a
      file descriptor for a symbolic link.
    - #RED_ENOSPC: Insufficient free space for the reservation.  When this error
      occurs, the file size is unchanged.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
int32_t red_freserve(
    int32_t     iFildes,
    uint64_t    ullSize)
{
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        REDHANDLE  *pHandle;
        OPENINODE  *pOpenIno = NULL;

        ret = FildesToHandle(iFildes, FTYPE_FILE, &pHandle);
        if(ret == -RED_EISDIR)
        {
            /*  Similar to red_write() (see comment there), the RED_EBADF error
                for a non-writable file descriptor takes precedence.
            */
            ret = -RED_EBADF;
        }

        if(ret == 0)
        {
            pOpenIno = pHandle->pOpenIno;

            if((pHandle->bFlags & HFLAG_WRITEABLE) == 0U)
            {
                ret = -RED_EBADF;
            }
            else if((pOpenIno->bFlags & OIFLAG_RESERVED) != 0U)
            {
                ret = -RED_EINVAL;
            }
            else
            {
                /*  Flags don't conflict; no error.
                */
            }
        }

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(pOpenIno->bVolNum);
        }
      #endif

        if(ret == 0)
        {
            REDSTAT sta;

            ret = RedCoreStat(pOpenIno->ulInode, &sta);

            if(ret == 0)
            {
                if(ullSize > sta.st_size)
                {
                    ret = RedCoreFileReserve(pOpenIno->ulInode, sta.st_size, ullSize - sta.st_size);

                    if(ret == 0)
                    {
                        pOpenIno->bFlags |= OIFLAG_RESERVED;
                        pOpenIno->ullResOff = sta.st_size;
                    }
                }
                else
                {
                    ret = -RED_EINVAL;
                }
            }
        }

        PosixLeave();
    }

    return PosixReturn(ret);
}
#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FRESERVE == 1) */


#if REDCONF_API_POSIX_READDIR == 1
/** @brief Open a directory stream for reading.

    @param pszPath  The path of the directory to open.

    @return On success, returns a pointer to a ::REDDIR object that can be used
            with red_readdir() and red_closedir().  On error, returns `NULL` and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EACCES: #REDCONF_POSIX_OWNER_PERM is enabled and POSIX permissions
      prohibit the current user from performing the operation: no search
      permission for a component of the prefix in @p pszPath; or no read
      permission for the directory named by @p pszPath.
    - #RED_EINVAL: @p pszPath is `NULL`; or the volume containing the path is
      not mounted.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENOENT: A component of @p pszPath does not exist; or the @p pszPath
      argument points to an empty string (and there is no volume with an empty
      path prefix); or #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW
      are both enabled, and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
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
        ret = FildesOpen(RED_AT_FDNONE, pszPath, RED_O_RDONLY, FTYPE_DIR, 0U, &iFildes);
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


/** @brief Open a directory stream for reading from a file descriptor.

    Like red_opendir(), except it operates on a directory file descriptor
    instead of a path to a directory.

    POSIX says that upon successful return from fdopendir(), any further use of
    the file descriptor is undefined.  With Reliance Edge, further use of the
    file descriptor is allowed.  However, note that @p iFildes and the returned
    ::REDDIR pointer refer to the same underlying object: thus, if @p iFildes
    is closed with red_close(), that also closes the ::REDDIR; and vice versa,
    if the ::REDDIR is closed with red_closedir(), that also closes @p iFildes.

    @param iFildes  The directory file descriptor to convert into a ::REDDIR
                    handle.

    @return On success, returns a pointer to a ::REDDIR object that can be used
            with red_readdir() and red_closedir().  On error, returns `NULL`
            and #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor.
    - #RED_ENOTDIR: The @p iFildes argument is not a directory file descriptor.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
REDDIR *red_fdopendir(
    int32_t     iFildes)
{
    REDDIR     *pDir = NULL;
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        REDHANDLE *pHandle;

        ret = FildesToHandle(iFildes, FTYPE_DIR, &pHandle);
        if(ret == 0)
        {
            /*  POSIX says to return EBADF if the file descriptor isn't open
                for reading.  Since Reliance Edge only allows directories to
                be opened with O_RDONLY, the file descriptor should always be
                readable.
            */
            REDASSERT((pHandle->bFlags & HFLAG_READABLE) != 0U);

            pDir = pHandle;
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
            ret = RedCoreVolSetCurrent(pDirStream->pOpenIno->bVolNum);
        }
      #endif

        if(ret == 0)
        {
            ret = RedCoreDirRead(pDirStream->pOpenIno->ulInode, &pDirStream->o.ulDirPosition, pDirStream->dirent.d_name, &pDirStream->dirent.d_ino);
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
    red_seekdir(pDirStream, 0U);
}


/** @brief Set the position of a directory stream.

    @p ulPosition should have been obtained from a previous call to
    red_telldir().  The directory position reverts to where it was when the
    @p ulPosition value was obtained from red_telldir().  For example, if you
    save the position with red_telldir(), call red_readdir(), call red_seekdir()
    with the saved position, and call red_readdir() again, then the second
    red_readdir() will yield the same results as the first (assuming there is
    not another thread concurrently modifying the directory).

    If @p ulPosition was not obtained from an earlier call to red_telldir(),
    then the result of a subsequent red_readdir() is undefined.

    Since this function (like its POSIX equivalent) cannot return an error,
    it takes no action in error conditions, such as when @p pDirStream is
    invalid.

    @param pDirStream   The directory stream whose position is to be updated.
    @param ulPosition   The new directory position, obtained from a previous
                        call to red_telldir().
*/
void red_seekdir(
    REDDIR     *pDirStream,
    uint32_t    ulPosition)
{
    if(PosixEnter() == 0)
    {
        if(DirStreamIsValid(pDirStream))
        {
            /*  POSIX says: "If the value of loc [ulPosition] was not obtained
                from an earlier call to telldir(), [...] the results of
                subsequent calls to readdir() are unspecified."

                In Reliance Edge, the directory position is the index of the
                directory entry.  The values returned by red_telldir() will be
                between 0 and the dirent count (inclusive).  However, it's
                possible that the directory size was larger when red_telldir()
                was invoked, so ulPosition could be between 0 and max dirent
                count (inclusive).  It's not a problem if ulPosition is beyond
                the end of the directory, since when given such a position,
                red_readdir() will behave just like the position is _at_ the end
                of the directory.

                ulPosition is technically invalid if it is larger than max
                dirent count.  However, computing that upper limit from here
                would be awkward, and we have no way to return an error anyway.
                So we don't worry about it: even if ulPosition is larger than
                red_telldir() will ever return, go ahead and set it in the
                handle.  For red_readdir(), any position beyond the end of the
                directory is equivalent to being at the end of the directory,
                even impossibly high positions.  Ignoring an erroneous position
                is acceptable, because POSIX allows any behavior for invalid
                positions.
            */
            pDirStream->o.ulDirPosition = ulPosition;
        }

        PosixLeave();
    }
}


/** @brief Return the current position of a directory stream.

    POSIX defines no error conditions for telldir().  If Reliance Edge detects
    an error condition, such as when @p pDirStream is invalid, the returned
    position value is always zero (which is also a valid position).

    @param pDirStream   The directory stream whose position is to be queried.

    @return The current position of the directory stream, which may be used as
            an argument to a subsequent call to red_seekdir().
*/
uint32_t red_telldir(
    REDDIR     *pDirStream)
{
    uint32_t    ulPosition = 0U;

    if(PosixEnter() == 0)
    {
        if(DirStreamIsValid(pDirStream))
        {
            ulPosition = pDirStream->o.ulDirPosition;
        }

        PosixLeave();
    }

    return ulPosition;
}


/** @brief Close a directory stream.

    After calling this function, @p pDirStream should no longer be used.

    @param pDirStream   The directory stream to close.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: @p pDirStream is not an open directory stream.
    - #RED_EIO: A disk I/O error occurred.  This error is only possible when
      #REDCONF_DELETE_OPEN is true.
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
            ret = HandleClose(pDirStream, 0U);
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
    - #RED_ELOOP: #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are
      both enabled and @p pszPath cannot be resolved because it either contains
      a symbolic link loop or nested symbolic links which exceed the nesting
      limit.
    - #RED_ENAMETOOLONG: The length of a component of @p pszPath is longer than
      #REDCONF_NAME_MAX.
    - #RED_ENOENT: A component of @p pszPath does not name an existing
      directory; or the volume does not exist; or the @p pszPath argument points
      to an empty string (and there is no volume with an empty path prefix); or
      #REDCONF_API_POSIX_SYMLINK and #REDOSCONF_SYMLINK_FOLLOW are both enabled,
      and path resolution encountered an empty symbolic link.
    - #RED_ENOLINK: #REDCONF_API_POSIX_SYMLINK is enabled,
      #REDOSCONF_SYMLINK_FOLLOW is disabled, and resolving @p pszPath requires
      following a symbolic link.
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

        ret = PathStartingPoint(RED_AT_FDCWD, pszPath, &bVolNum, &ulCwdInode, &pszLocalPath);
        if(ret == 0)
        {
            uint32_t ulInode;

            /*  Resolve the new CWD.
            */
            ret = RedPathLookup(ulCwdInode, pszLocalPath, 0U, &ulInode);
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
                    TASKSLOT *pTask = TaskFind();

                    if((pTask == NULL) || (pTask->pCwd == NULL))
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
                        /*  Dereference the old CWD inode.  If orphaned, it can
                            now be freed.  However, the chdir operation should
                            not fail if there is an error freeing the orphaned
                            old CWD, because the unlinking of the old CWD is
                            unrelated to changing the CWD.
                        */
                        ret = OpenInoDeref(pTask->pCwd, true, false);

                        if(ret == 0)
                        {
                            pTask->pCwd = OpenInoFind(bVolNum, ulInode, true);

                            if(pTask->pCwd != NULL)
                            {
                                pTask->pCwd->uRefs++;
                            }
                            else
                            {
                                REDERROR();
                                ret = -RED_EFUBAR;
                            }
                        }
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
    - #RED_ENOENT: #REDCONF_DELETE_OPEN is true and the current working
      directory has been removed.
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
            const TASKSLOT *pTask = TaskFind();

            if((pTask == NULL) || (pTask->pCwd == NULL))
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
                ret = RedCoreVolSetCurrent(pTask->pCwd->bVolNum);
                if(ret == 0)
              #endif
                {
                    /*  The CWD for an unmounted volume is always the root
                        directory -- so in that case, the loop below is not
                        entered, and we end up populating the buffer with just
                        the volume path prefix and a path separator, which is
                        exactly as it should be.
                    */
                    REDASSERT(gpRedVolume->fMounted || (pTask->pCwd->ulInode == INODE_ROOTDIR));

                    ret = DirInodeToPath(pTask->pCwd->ulInode, pszBuffer, ulBufferSize, 0U);
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


/** @brief Populate a buffer with the path to a directory.

    @note Reliance Edge does not have a maximum path length; paths can be
          arbitrarily long.  Thus, no buffer is guaranteed to be large enough to
          store the path.  If it is important that calls to this function
          succeed, you need to analyze your application to determine the maximum
          length of a directory path.  Alternatively, if dynamic memory
          allocation is used, this function can be called in a loop, with the
          buffer size increasing if the function fails with a RED_ERANGE error;
          repeat until the call succeeds.

    @param iFildes      An open directory file descriptor for the directory
                        whose path is to be retrieved.
    @param pszBuffer    The buffer to populate with the path.
    @param ulBufferSize The size in bytes of @p pszBuffer.
    @param ulFlags      The only flag value is #RED_GETDIRPATH_NOVOLUME, which
                        means to exclude the volume path prefix for the path put
                        into @p pszBuffer.

    @return On success, @p pszBuffer is returned.  On error, `NULL` is returned
            and #red_errno is set appropriately.

    <b>Errno values</b>
    - #RED_EBADF: The @p iFildes argument is not a valid file descriptor.
    - #RED_EINVAL: @p pszBuffer is `NULL`; or @p ulBufferSize is zero; or
      @p ulFlags is invalid.
    - #RED_EIO: A disk I/O error occurred.
    - #RED_ENOENT: #REDCONF_DELETE_OPEN is true and @p iFildes is an open file
      descriptor for an unlinked directory.
    - #RED_ENOTDIR: The @p iFildes argument is a valid file descriptor for a
      non-directory.
    - #RED_ERANGE: @p ulBufferSize is greater than zero but too small for the
      path string.
    - #RED_EUSERS: Cannot become a file system user: too many users.
*/
char *red_getdirpath(
    int32_t     iFildes,
    char       *pszBuffer,
    uint32_t    ulBufferSize,
    uint32_t    ulFlags)
{
    REDSTATUS   ret;
    char       *pszReturn;

    if((pszBuffer == NULL) || (ulBufferSize == 0U) || ((ulFlags & ~RED_GETDIRPATH_NOVOLUME) != 0U))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        ret = PosixEnter();
        if(ret == 0)
        {
            REDHANDLE *pHandle;

            ret = FildesToHandle(iFildes, FTYPE_DIR, &pHandle);

          #if REDCONF_VOLUME_COUNT > 1U
            if(ret == 0)
            {
                ret = RedCoreVolSetCurrent(pHandle->pOpenIno->bVolNum);
            }
          #endif

            if(ret == 0)
            {
                ret = DirInodeToPath(pHandle->pOpenIno->ulInode, pszBuffer, ulBufferSize, ulFlags);
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


/** @brief Pointer to where the last file system error (errno) is stored.

    This function is intended to be used via the #red_errno macro, or a similar
    user-defined macro, that can be used both as an lvalue (writable) and an
    rvalue (readable).

    Under normal circumstances, the errno for each task is stored in a
    different location.  Applications do not need to worry about one task
    obliterating an error value that another task needed to read.  This task
    errno is initially zero.  When one of the POSIX-like APIs returns an
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
    /*  The global errno value, used when the caller is not (and cannot become)
        a file system user (which includes when the driver is uninitialized).
    */
    static REDSTATUS iGlobalErrno = 0;
    REDSTATUS       *piErrno;

    if(gfPosixInited)
    {
      #if REDCONF_TASK_COUNT > 1U
        TASKSLOT *pTask;

        /*  If this task has used the file system before, it will already have
            a task slot, which includes the task-specific errno.
        */
        RedOsMutexAcquire();

        pTask = TaskFind();

        RedOsMutexRelease();

        if(pTask == NULL)
        {
            /*  This task is not a file system user, so try to register it as
                one.  This FS mutex must be held in order to register.
            */
            RedOsMutexAcquire();

            pTask = TaskRegister();

            RedOsMutexRelease();

            if(pTask != NULL)
            {
                REDASSERT(pTask->ulTaskId == RedOsTaskId());
                REDASSERT(pTask->iErrno == 0);

                piErrno = &pTask->iErrno;
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
            piErrno = &pTask->iErrno;
        }
      #else /* REDCONF_TASK_COUNT > 1U */
        piErrno = &gaTask[0U].iErrno;
      #endif
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
}
/** @} */

/*-------------------------------------------------------------------
    Helper Functions
-------------------------------------------------------------------*/

/** @brief Read from an open file.

    @param iFildes      The file descriptor from which to read.
    @param pBuffer      The buffer to populate with data read.  Must be at least
                        @p ulLength bytes in size.
    @param ulLength     Number of bytes to attempt to read.
    @param fIsPread     If true, this is red_pread(): @p ullOffset is used for
                        the file offset and the handle's file offset is not
                        modified.  If false, this is red_read(): the handle's
                        file offset is used and updated and @p ullOffset is
                        ignored.
    @param ullOffset    If @p fIsPread is true, the file offset to read from.

    @return On success, returns a nonnegative value indicating the number of
            bytes actually read.  On error, -1 is returned and #red_errno is
            set appropriately.

    See red_read() for the list of the possible #red_errno values.
*/
static int32_t ReadSub(
    int32_t     iFildes,
    void       *pBuffer,
    uint32_t    ulLength,
    bool        fIsPread,
    uint64_t    ullOffset)
{
    uint32_t    ulLenRead = 0U;
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        if(ulLength > (uint32_t)INT32_MAX)
        {
            ret = -RED_EINVAL;
        }
        else
        {
            REDHANDLE *pHandle;

            ret = FildesToHandle(iFildes, FTYPE_NOTDIR, &pHandle);

            if((ret == 0) && ((pHandle->bFlags & HFLAG_READABLE) == 0U))
            {
                ret = -RED_EBADF;
            }

          #if REDCONF_VOLUME_COUNT > 1U
            if(ret == 0)
            {
                ret = RedCoreVolSetCurrent(pHandle->pOpenIno->bVolNum);
            }
          #endif

            if(ret == 0)
            {
                ulLenRead = ulLength;
                ret = RedCoreFileRead(pHandle->pOpenIno->ulInode,
                    fIsPread ? ullOffset : pHandle->o.ullFileOffset, &ulLenRead, pBuffer);
            }

            if(ret == 0)
            {
                REDASSERT(ulLenRead <= ulLength);

                /*  POSIX: "The pread() function shall [...] read from a given
                    position [...] without changing the file offset."
                */
                if(!fIsPread)
                {
                    pHandle->o.ullFileOffset += ulLenRead;
                }
            }
        }

        PosixLeave();
    }

    if(ret == 0)
    {
        ret = (int32_t)ulLenRead;
    }
    else
    {
        ret = PosixReturn(ret);
    }

    return ret;
}


#if REDCONF_READ_ONLY == 0
/** @brief Write to an open file.

    @param iFildes      The file descriptor to write to.
    @param pBuffer      The buffer containing the data to be written.  Must be
                        at least @p ulLength bytes in size.
    @param ulLength     Number of bytes to attempt to write.
    @param fIsPwrite    If true, this is red_pwrite(): @p ullOffset is used for
                        the file offset and the handle's file offset is not
                        modified.  If false, this is red_write(): the handle's
                        file offset is used and updated and @p ullOffset is
                        ignored.
    @param ullOffset    If @p fIsPwrite is true, the file offset to write at.

    @return On success, returns a nonnegative value indicating the number of
            bytes actually read.  On error, -1 is returned and #red_errno is
            set appropriately.

    See red_write() for the list of the possible #red_errno values.
*/
static int32_t WriteSub(
    int32_t     iFildes,
    const void *pBuffer,
    uint32_t    ulLength,
    bool        fIsPwrite,
    uint64_t    ullOffset)
{
    uint32_t    ulLenWrote = 0U;
    REDSTATUS   ret;

    ret = PosixEnter();
    if(ret == 0)
    {
        if(ulLength > (uint32_t)INT32_MAX)
        {
            ret = -RED_EINVAL;
        }
        else
        {
            REDHANDLE  *pHandle;
            uint64_t    ullFileSize = 0U;

            ret = FildesToHandle(iFildes, FTYPE_NOTDIR, &pHandle);
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
                ret = RedCoreVolSetCurrent(pHandle->pOpenIno->bVolNum);
            }
          #endif

            if(ret == 0)
            {
                /*  POSIX: "The pwrite() function shall [...] writes into a
                    given position [...] (regardless of whether O_APPEND is
                    set)."
                */
                bool fAppending = !fIsPwrite && ((pHandle->bFlags & HFLAG_APPENDING) != 0U);
                bool fNeedSize = fAppending;

              #if REDCONF_API_POSIX_FRESERVE == 1
                if((pHandle->pOpenIno->bFlags & OIFLAG_RESERVED) != 0U)
                {
                    fNeedSize = true;
                }
              #endif

                if(fNeedSize)
                {
                    REDSTAT s;

                    ret = RedCoreStat(pHandle->pOpenIno->ulInode, &s);
                    if(ret == 0)
                    {
                        ullFileSize = s.st_size;

                        if(fAppending)
                        {
                            pHandle->o.ullFileOffset = ullFileSize;
                        }
                    }
                }
            }

            if(ret == 0)
            {
                uint64_t ullWriteOff = fIsPwrite ? ullOffset : pHandle->o.ullFileOffset;

              #if REDCONF_API_POSIX_FRESERVE == 1
                if((pHandle->pOpenIno->bFlags & OIFLAG_RESERVED) != 0U)
                {
                    if(ullWriteOff != pHandle->pOpenIno->ullResOff)
                    {
                        ret = -RED_EINVAL;
                    }
                    else
                    {
                        if((ullWriteOff + ulLength) > ullFileSize)
                        {
                            /*  Truncate the write, so that it writes up to the
                                end of the reservation but not further.
                            */
                            ulLenWrote = (uint32_t)(ullFileSize - ullWriteOff);
                        }
                        else
                        {
                            ulLenWrote = ulLength;
                        }

                        ret = RedCoreFileWriteReserved(pHandle->pOpenIno->ulInode, ullWriteOff, &ulLenWrote, pBuffer);
                    }
                }
                else
              #endif
                {
                    ulLenWrote = ulLength;
                    ret = RedCoreFileWrite(pHandle->pOpenIno->ulInode, ullWriteOff, &ulLenWrote, pBuffer);
                }
            }

            if(ret == 0)
            {
                REDASSERT(ulLenWrote <= ulLength);

                /*  POSIX: "The pwrite() function [...] does not change the file
                    offset".
                */
                if(!fIsPwrite)
                {
                    pHandle->o.ullFileOffset += ulLenWrote;
                }

              #if REDCONF_API_POSIX_FRESERVE == 1
                if((pHandle->pOpenIno->bFlags & OIFLAG_RESERVED) != 0U)
                {
                    pHandle->pOpenIno->ullResOff += ulLenWrote;

                    if(pHandle->pOpenIno->ullResOff == ullFileSize)
                    {
                        /*  The reservation has been completely written.
                        */
                        pHandle->pOpenIno->bFlags &= ~OIFLAG_RESERVED;
                        ret = RedCoreFileUnreserve(pHandle->pOpenIno->ulInode, ullFileSize);
                    }
                }
              #endif
            }
        }

        PosixLeave();
    }

    if(ret == 0)
    {
        ret = (int32_t)ulLenWrote;
    }
    else
    {
        ret = PosixReturn(ret);
    }

    return ret;
}
#endif /* REDCONF_READ_ONLY == 0 */


/** @brief Find the starting point for a path.

    In other words, find the volume number and directory inode from which the
    parsing of this path should start.

    The volume number will be set as the current volume.

    @param iDirFildes       File descriptor for the directory from which
                            @p pszPath, if it is a relative path, should be
                            parsed.  May also be one of the pseudo file
                            descriptors: #RED_AT_FDCWD, #RED_AT_FDABS, or
                            #RED_AT_FDNONE.
    @param pszPath          The path to examine.  This may be an absolute path,
                            in which case @p iDirFildes is ignored; or it may be
                            a relative path, in which case @p iDirFildes is used
                            to derive the @p pulDirInode output parameter.
    @param pbVolNum         On successful return, if non-NULL, populated with
                            the volume number for the path.  This volume number
                            is set as the current volume.
    @param pulDirInode      On successful return, populated with the directory
                            inode that the local path starts in.
    @param ppszLocalPath    On successful return, populated with the path
                            stripped of volume path prefixing, if there was any.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0               Operation was successful.
    @retval -RED_EBADF      @p pszPath does not specify an absolute path and
                            @p iDirFildes is neither a valid pseudo file
                            descriptor nor a valid file descriptor open for
                            reading.
    @retval -RED_EINVAL     @p pszPath or @p pulDirInode or @p ppszLocalPath is
                            `NULL`.
    @retval -RED_ENOTDIR    @p pszPath does not specify an absolute path and
                            @p iDirFildes is valid file descriptor for a
                            non-directory.
    @retval -RED_ENOENT     @p pszPath could not be matched to any volume and
                            there is no CWD.
*/
static REDSTATUS PathStartingPoint(
    int32_t         iDirFildes,
    const char     *pszPath,
    uint8_t        *pbVolNum,
    uint32_t       *pulDirInode,
    const char    **ppszLocalPath)
{
    REDSTATUS       ret;

    if((pszPath == NULL) || (pulDirInode == NULL) || (ppszLocalPath == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        uint8_t bVolNum;

        ret = RedPathVolumePrefixLookup(pszPath, &bVolNum);
        if(ret == 0)
        {
            *pulDirInode = INODE_ROOTDIR;
            *ppszLocalPath = &pszPath[RedStrLen(gpRedVolConf->pszPathPrefix)];
        }

        /*  RED_AT_FDABS forces the path to be treated as an absolute path.
        */
        if(iDirFildes != RED_AT_FDABS)
        {
            /*  If the path was _not_ an absolute path, use iDirFildes.  We
                consider the path to be absolute if it exactly matched a
                non-zero length volume path prefix; or if it started with a path
                separator.

                Don't use the CWD if the path was an empty string -- POSIX
                considers empty paths to be an error.
            */
            if(    ((ret == -RED_ENOENT) || ((ret == 0) && (gpRedVolConf->pszPathPrefix[0U] == '\0')))
                && (pszPath[0U] != REDCONF_PATH_SEPARATOR)
                && (pszPath[0U] != '\0'))
            {
                const OPENINODE *pOpenIno = NULL;

              #if REDCONF_API_POSIX_CWD == 1
                if(iDirFildes == RED_AT_FDCWD)
                {
                    const TASKSLOT *pTask = TaskFind();

                    if((pTask == NULL) || (pTask->pCwd == NULL))
                    {
                        /*  This should be unreachable unless there is a coding
                            error and this function is being called without first
                            calling PosixEnter().
                        */
                        REDERROR();
                        ret = -RED_EFUBAR;
                    }
                    else
                    {
                        pOpenIno = pTask->pCwd;
                    }
                }
                else
              #endif
                {
                    REDHANDLE *pHandle;

                    ret = FildesToHandle(iDirFildes, FTYPE_DIR, &pHandle);

                    if((ret == 0) && ((pHandle->bFlags & HFLAG_READABLE) == 0U))
                    {
                        ret = -RED_EBADF;
                    }

                    if(ret == 0)
                    {
                        pOpenIno = pHandle->pOpenIno;
                    }
                }

                if(pOpenIno != NULL)
                {
                    bVolNum = pOpenIno->bVolNum;
                    *pulDirInode = pOpenIno->ulInode;
                    *ppszLocalPath = pszPath;

                    /*  This clears ret, which might equal -RED_ENOENT.
                    */
                  #if REDCONF_VOLUME_COUNT > 1U
                    ret = RedCoreVolSetCurrent(bVolNum);
                  #else
                    ret = 0;
                  #endif
                }
            }
        }

        if((ret == 0) && (pbVolNum != NULL))
        {
            *pbVolNum = bVolNum;
        }
    }

    return ret;
}


/** @brief Get a file descriptor for a path.

    @param iDirFildes   File descriptor for the directory from which @p pszPath,
                        if it is a relative path, should be parsed.  Can be a
                        pseudo file descriptor: #RED_AT_FDCWD, #RED_AT_FDABS,
                        or #RED_AT_FDNONE.
    @param pszPath      Path to a file to open.
    @param ulOpenFlags  The RED_O_* flags the descriptor is opened with.
    @param type         Indicates the expected descriptor type: file, directory,
                        symlink, or a combination.
    @param uMode        The RED_S_* mode bits to use if creating a file.
    @param piFildes     On successful return, populated with the file
                        descriptor.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0                   Operation was successful.
    @retval -RED_EACCES         Permission denied: #REDCONF_POSIX_OWNER_PERM is
                                enabled and the POSIX permissions prohibit the
                                current from performing the operation.
    @retval -RED_EBADF          @p pszPath does not specify an absolute path
                                and @p iDirFildes is neither a valid pseudo
                                file descriptor nor a valid file descriptor
                                open for reading.
    @retval -RED_EINVAL         @p piFildes is `NULL`; or @p pszPath is `NULL`;
                                or the volume is not mounted.
    @retval -RED_EMFILE         There are no available handles.
    @retval -RED_EEXIST         Using #RED_O_CREAT and #RED_O_EXCL, and the
                                indicated path already exists.
    @retval -RED_EISDIR         The path names a directory and @p ulOpenFlags
                                includes #RED_O_WRONLY or #RED_O_RDWR.
    @retval -RED_ENOENT         #RED_O_CREAT is not set and the named file does
                                not exist; or #RED_O_CREAT is set and the parent
                                directory does not exist; or the volume does not
                                exist; or the @p pszPath argument points to an
                                empty string.
    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_ENAMETOOLONG   The length of a component of @p pszPath is
                                longer than #REDCONF_NAME_MAX.
    @retval -RED_ENFILE         Attempting to create a file but the file system
                                has used all available inode slots.
    @retval -RED_ENOLINK        #REDCONF_API_POSIX_SYMLINK is true and either:
                                a) #REDCONF_SYMLINK_FOLLOW is disabled and
                                resolving @p pszPath requires following a
                                symbolic link; or b) @p type is #FTYPE_SYMLINK
                                and @p pszPath does not name a symbolic link.
    @retval -RED_ENOSPC         The file does not exist and #RED_O_CREAT was
                                specified, but there is insufficient free space
                                to expand the directory or to create the new
                                file.
    @retval -RED_ENOTDIR        A component of the prefix in @p pszPath does not
                                name a directory; or @p pszPath does not specify
                                an absolute path and @p iDirFildes is a valid
                                file descriptor for a non-directory; or @p type
                                is #FTYPE_DIR and @p pszPath is a regular file.
    @retval -RED_EROFS          The path resides on a read-only file system and
                                a write operation was requested.
*/
static REDSTATUS FildesOpen(
    int32_t     iDirFildes,
    const char *pszPath,
    uint32_t    ulOpenFlags,
    FTYPE       type,
    uint16_t    uMode,
    int32_t    *piFildes)
{
    uint32_t    ulDirInode;
    const char *pszLocalPath;
    REDSTATUS   ret;

    ret = PathStartingPoint(iDirFildes, pszPath, NULL, &ulDirInode, &pszLocalPath);
    if(ret == 0)
    {
        if(piFildes == NULL)
        {
            ret = -RED_EINVAL;
        }
      #if REDCONF_READ_ONLY == 0
        else if(    gpRedVolume->fReadOnly
                 && ((ulOpenFlags & (RED_O_WRONLY|RED_O_RDWR|RED_O_TRUNC)) != 0U))
        {
            /*  O_WRONLY, O_RDWR, and O_TRUNC are disallowed when read-only.

                Note that O_CREAT _is_ allowed, if -- and only if -- the file
                already exists.  This is handled below.
            */
            ret = -RED_EROFS;
        }
      #endif
        else
        {
            REDHANDLE *pHandle = HandleFindFree();

            /*  Error if all the handles are in use.
            */
            if(pHandle == NULL)
            {
                ret = -RED_EMFILE;
            }
            else
            {
                bool        fCreated = false;
                uint32_t    ulInode = 0U; /* Init'd to quiet warnings. */

              #if REDCONF_READ_ONLY == 0
                if((ulOpenFlags & RED_O_CREAT) != 0U)
                {
                    uint32_t    ulPInode;
                    const char *pszName;

                    ret = RedPathToName(ulDirInode, pszLocalPath, -RED_EISDIR, &ulPInode, &pszName);
                    if(ret == 0)
                    {
                        ret = RedCoreCreate(ulPInode, pszName, uMode, &ulInode);
                        if(ret == 0)
                        {
                            fCreated = true;
                        }
                        /*  Need to lookup the name in two separate error
                            conditions:
                            1) If the file system is read-only, then the core
                               returned an EROFS error without checking whether
                               the name exists; but we still need to know
                               whether it exists.
                            2) If the name already exists, and that's OK, we
                               still need its inode number to open it.
                        */
                        else if(    (ret == -RED_EROFS)
                                 || ((ret == -RED_EEXIST) && ((ulOpenFlags & RED_O_EXCL) == 0U)))
                        {
                            REDSTATUS retCreate = ret;

                            ret = RedCoreLookup(ulPInode, pszName, &ulInode);

                            if(retCreate == -RED_EROFS)
                            {
                                if((ret == 0) && ((ulOpenFlags & RED_O_EXCL) != 0U))
                                {
                                    /*  With a read-only volume, a name that
                                        already exists, and O_CREAT|O_EXCL
                                        flags, return an EEXIST error, just like
                                        we do for a writable volume.
                                    */
                                    ret = -RED_EEXIST;
                                }
                                else if(ret == -RED_ENOENT)
                                {
                                    /*  With a read-only volume, a name that
                                        does _not_ exist, and an O_CREAT flag,
                                        return an EROFS error, as is appropriate
                                        for an attempt to create a file on a
                                        read-only volume.
                                    */
                                    ret = -RED_EROFS;
                                }
                                else
                                {
                                    /*  No action, either we can open the inode
                                        (ret == 0) or we have an error condition
                                        that needs to be propagated.
                                    */
                                }
                            }
                        }
                        else
                        {
                            /*  Propagate the error.
                            */
                        }
                    }
                    else if(ret == -RED_EISDIR) /* If path resolves to root directory */
                    {
                        if((ulOpenFlags & RED_O_EXCL) != 0U)
                        {
                            /*  If we are here, an EEXIST error condition
                                exists.  However, if an EISDIR error condition
                                also exists (O_RDWR or O_WRONLY in open flags),
                                then (to preserve historical behavior) that
                                error takes precedence.
                            */
                            if((ulOpenFlags & RED_O_RDONLY) != 0U)
                            {
                                ret = -RED_EEXIST;
                            }
                        }
                        else
                        {
                            ulInode = INODE_ROOTDIR;
                            ret = 0;
                        }
                    }
                    else
                    {
                        /*  Propagate the error.
                        */
                    }
                }
                else
              #endif /* REDCONF_READ_ONLY == 0 */
                {
                    uint32_t ulLookupFlags;

                  #if REDCONF_API_POSIX_SYMLINK == 1
                    if((ulOpenFlags & RED_O_SYMLINK) || (ulOpenFlags & RED_O_NOFOLLOW))
                    {
                        ulLookupFlags = RED_AT_SYMLINK_NOFOLLOW;
                    }
                    else
                  #endif
                    {
                        ulLookupFlags = 0U;
                    }

                    ret = RedPathLookup(ulDirInode, pszLocalPath, ulLookupFlags, &ulInode);
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
                          #if REDCONF_POSIX_OWNER_PERM == 1
                            uint8_t bAccess = 0U;

                            if((ulOpenFlags & RED_O_RDWR) != 0U)
                            {
                                bAccess |= RED_R_OK | RED_W_OK;
                            }
                            else if((ulOpenFlags & RED_O_RDONLY) != 0U)
                            {
                                bAccess |= RED_R_OK;
                            }
                            else if((ulOpenFlags & RED_O_WRONLY) != 0U)
                            {
                                bAccess |= RED_W_OK;
                            }

                            if((ulOpenFlags & RED_O_TRUNC) != 0U)
                            {
                                bAccess |= RED_W_OK;
                            }

                            ret = RedPermCheck(bAccess, s.st_mode, s.st_uid, s.st_gid);
                          #endif

                            uMode = s.st_mode;
                        }
                    }

                    /*  Error if the inode is not of the expected type.
                    */
                    if(ret == 0)
                    {
                        ret = RedModeTypeCheck(uMode, type);

                      #if (REDCONF_API_POSIX_SYMLINK == 1) && (REDOSCONF_SYMLINK_FOLLOW == 1)
                        /*  POSIX says ELOOP if O_NOFOLLOW and the final path
                            component is a symbolic link (yes, this is ambiguous
                            with other uses of ELOOP).
                        */
                        if((ret == -RED_ENOLINK) && ((ulOpenFlags & RED_O_NOFOLLOW) != 0U))
                        {
                            ret = -RED_ELOOP;
                        }
                      #endif
                    }

                    /*  Directories must always be opened with O_RDONLY.
                    */
                    if((ret == 0) && RED_S_ISDIR(uMode) && ((ulOpenFlags & RED_O_RDONLY) == 0U))
                    {
                        ret = -RED_EISDIR;
                    }

                  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FTRUNCATE == 1)
                    if((ret == 0) && ((ulOpenFlags & RED_O_TRUNC) != 0U))
                    {
                        ret = RedCoreFileTruncate(ulInode, UINT64_SUFFIX(0));
                    }
                  #endif
                }

                if(ret == 0)
                {
                    ret = HandleOpen(pHandle, ulInode);
                }

                if(ret == 0)
                {
                    uint16_t    uHandleIdx = (uint16_t)(pHandle - gaHandle);
                    int32_t     iFildes;

                    if(RED_S_ISDIR(uMode))
                    {
                        pHandle->bFlags |= HFLAG_DIRECTORY;
                    }
                  #if REDCONF_API_POSIX_SYMLINK == 1
                    else if(RED_S_ISLNK(uMode))
                    {
                        pHandle->bFlags |= HFLAG_SYMLINK;
                    }
                  #endif
                    else
                    {
                        /*  No flag for regular files.
                        */
                    }

                    if(((ulOpenFlags & RED_O_RDONLY) != 0U) || ((ulOpenFlags & RED_O_RDWR) != 0U))
                    {
                        pHandle->bFlags |= HFLAG_READABLE;
                    }

                  #if REDCONF_READ_ONLY == 0
                    if(((ulOpenFlags & RED_O_WRONLY) != 0U) || ((ulOpenFlags & RED_O_RDWR) != 0U))
                    {
                        pHandle->bFlags |= HFLAG_WRITEABLE;
                    }

                    if((ulOpenFlags & RED_O_APPEND) != 0U)
                    {
                        pHandle->bFlags |= HFLAG_APPENDING;
                    }
                  #endif

                    iFildes = FildesPack(uHandleIdx, gbRedVolNum);
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

    ret = FildesToHandle(iFildes, FTYPE_ANY, &pHandle);

    if(ret == 0)
    {
        ret = HandleClose(pHandle, RED_TRANSACT_CLOSE);
    }

    return ret;
}


/** @brief Convert a file descriptor into a handle pointer.

    Also validates the file descriptor.

    @param iFildes      The file descriptor for which to get a handle.
    @param expectedType The expected type of the file descriptor: one or more of
                        #FTYPE_DIR, #FTYPE_FILE, #FTYPE_SYMLINK.
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
            || (gaHandle[uHandleIdx].pOpenIno == NULL)
            || (gaHandle[uHandleIdx].pOpenIno->bVolNum != bVolNum)
            || (gauGeneration[bVolNum] != uGeneration))
        {
            ret = -RED_EBADF;
        }
        else
        {
            FTYPE htype;

            switch(gaHandle[uHandleIdx].bFlags & (HFLAG_DIRECTORY|HFLAG_SYMLINK))
            {
                case HFLAG_DIRECTORY:
                    htype = FTYPE_DIR;
                    break;

              #if REDCONF_API_POSIX_SYMLINK == 1
                case HFLAG_SYMLINK:
                    htype = FTYPE_SYMLINK;
                    break;
              #endif

                case 0U:
                default: /* default case should never happen. */
                    htype = FTYPE_FILE;
                    break;
            }

            ret = RedFileTypeCheck(htype, expectedType);

            if(ret == 0)
            {
                *ppHandle = &gaHandle[uHandleIdx];
                ret = 0;
            }
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

    if(!HANDLE_PTR_IS_VALID(pDirStream))
    {
        /*  pDirStream is not a pointer to one of our handles.
        */
        fRet = false;
    }
    else if(    (pDirStream->pOpenIno == NULL)
             || (pDirStream->pOpenIno->bVolNum >= REDCONF_VOLUME_COUNT)
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


/** @brief Find a free handle.

    @return On success, returns a pointer to a free ::REDHANDLE.  If there are
            no free handles, returns `NULL`.
*/
static REDHANDLE *HandleFindFree(void)
{
    REDHANDLE  *pHandle = NULL;
    uint16_t    uIdx;

    /*  Search for an unused handle.
    */
    for(uIdx = 0U; uIdx < ARRAY_SIZE(gaHandle); uIdx++)
    {
        if(gaHandle[uIdx].pOpenIno == NULL)
        {
            pHandle = &gaHandle[uIdx];
            RedMemSet(pHandle, 0U, sizeof(*pHandle));
            break;
        }
    }

    return pHandle;
}


/** @brief Associate a handle with the given inode.

    @param pHandle  Pointer to the ::REDHANDLE to associate with @p ulInode.
    @param ulInode  The inode number to associate with ::REDHANDLE.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p pHandle is `NULL`.
*/
static REDSTATUS HandleOpen(
    REDHANDLE  *pHandle,
    uint32_t    ulInode)
{
    REDSTATUS   ret = 0;

    if(pHandle == NULL)
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        OPENINODE *pOpenIno = OpenInoFind(gbRedVolNum, ulInode, true);

        if(pOpenIno == NULL)
        {
            /*  This should never happen.  There are the same number of open
                inode structures as there are handles, and at most one open
                inode per handle (though possibly less).  Thus, the number of
                available open inodes should always be greater than or equal to
                the number of available handles.
            */
            REDERROR();
            ret = -RED_EFUBAR;
        }
        else
        {
            REDASSERT(pOpenIno->uRefs < OPEN_INODE_COUNT);

            pOpenIno->uRefs++;
        }

        pHandle->pOpenIno = pOpenIno;
    }

    return ret;
}


/** @brief Close a handle.

    In addition to closing the handle, this function dereferences the underlying
    ::OPENINODE with which the handle was associated.  See OpenInoDeref().

    @param pHandle      The handle to close.
    @param ulTransFlag  The transaction flag associated with the close event.
                        If this transaction flag is in the transaction mask, an
                        automatic transaction is performed.  Zero if no
                        automatic transaction is desired.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
    @retval -RED_EINVAL @p pHandle is an invalid handle pointer.
    @retval -RED_EBADF  @p pHandle is not an open handle.
*/
static REDSTATUS HandleClose(
    REDHANDLE  *pHandle,
    uint32_t    ulTransFlag)
{
    REDSTATUS   ret = 0;

    if(!HANDLE_PTR_IS_VALID(pHandle))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else if(pHandle->pOpenIno == NULL)
    {
        ret = -RED_EBADF;
    }
    else
    {
        OPENINODE  *pOpenIno = pHandle->pOpenIno;
      #if REDCONF_READ_ONLY == 0
        uint32_t    ulTransMask = 0U;
        bool        fTransacting = false;  /* Init'd to satisfy picky compilers. */

      #if REDCONF_VOLUME_COUNT > 1U
        ret = RedCoreVolSetCurrent(pOpenIno->bVolNum);
      #endif

        if((ret == 0) && !gpRedVolume->fReadOnly)
        {
            ret = RedCoreTransMaskGet(&ulTransMask);
        }

        if(ret == 0)
        {
            /*  Failure when freeing an orphan is unexpected, and the error
                normally would not be returned by close.  However, any error in
                RedCoreFreeOrphan() (called by OpenInoDeref()) is considered
                critical, thus a subsequent transaction will return RED_EROFS.
                The error returned from RedCoreFreeOrphan() is thus descriptive
                of the error that prevented close from completing.
            */
            fTransacting = (ulTransMask & ulTransFlag) != 0U;

            ret = OpenInoDeref(pOpenIno, true, fTransacting);
        }

        if(ret == 0)
        {
            pHandle->pOpenIno = NULL;

            /*  No core event for close, so close transactions and freeing of
                orphans needs to be implemented here.

                If the volume is read-only, skip those operations.  This avoids
                -RED_EROFS errors when closing files on a read-only volume.
            */
            if(!gpRedVolume->fReadOnly && fTransacting)
            {
                ret = RedCoreVolTransact();
            }
        }
      #else /* REDCONF_READ_ONLY == 0 */
        ret = OpenInoDeref(pOpenIno, false, false);
        if(ret == 0)
        {
            pHandle->pOpenIno = NULL;
        }
      #endif
    }

    return ret;
}


/** @brief Find (or optionally allocate) an open inode.

    @param bVolNum  The volume number of the volume that the inode resides on.
    @param ulInode  The inode number.
    @param fAlloc   If true, allocate an open inode if one doesn't already
                    exist.  If false, return `NULL` if @p ulInode isn't open.

    @return Returns a pointer to the ::OPENINODE structure for the open inode.
            Returns `NULL` if there is no open inode for @p ulInode; if
            @p fAlloc is true, only returns `NULL` if there are no available
            open inodes, which is an unexpected condition.
*/
static OPENINODE *OpenInoFind(
    uint8_t     bVolNum,
    uint32_t    ulInode,
    bool        fAlloc)
{
    OPENINODE  *pOpenIno = NULL;
    OPENINODE  *pFreeIno = NULL;
    uint32_t    ulIdx;

    for(ulIdx = 0U; ulIdx < ARRAY_SIZE(gaOpenInos); ulIdx++)
    {
        OPENINODE *pThisOpenIno = &gaOpenInos[ulIdx];

        if((pThisOpenIno->ulInode == ulInode) && (pThisOpenIno->bVolNum == bVolNum))
        {
            pOpenIno = pThisOpenIno;
            break;
        }

        if((pFreeIno == NULL) && (pThisOpenIno->ulInode == INODE_INVALID))
        {
            pFreeIno = pThisOpenIno;
        }
    }

    if((pOpenIno == NULL) && fAlloc && (pFreeIno != NULL))
    {
        pOpenIno = pFreeIno;

        RedMemSet(pOpenIno, 0U, sizeof(*pOpenIno));

        pOpenIno->ulInode = ulInode;
        pOpenIno->bVolNum = bVolNum;
    }

    return pOpenIno;
}


/** @brief Dereference an open inode, closing it if it becomes unreferenced.

    @param pOpenIno                 The open inode to dereference.
    @param fDoCleanup               If true and the inode becomes unreferenced,
                                    perform any cleanup operations which the
                                    enabled feature set requires.  If false, the
                                    cleanup is skipped.  Should normally be
                                    true, unless the volume is unmounted or the
                                    untransacted changes have been reverted.
    @param fPropagateCleanupError   If true, and @p fDoCleanup is also true,
                                    propagate errors from the cleanup
                                    operations.  If false, cleanup errors will
                                    be ignored.  When true, if an error occurs,
                                    the open inode is neither dereferenced nor
                                    closed.  Whether this should be true depends
                                    on the semantics of the calling operation.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
    @retval -RED_EINVAL @p pOpenIno is an invalid open inode pointer; or
                        @p pOpenIno has zero references.
*/
static REDSTATUS OpenInoDeref(
    OPENINODE  *pOpenIno,
    bool        fDoCleanup,
    bool        fPropagateCleanupError)
{
    REDSTATUS   ret = 0;

    /*  Unused in some configurations.
    */
    (void)fDoCleanup;
    (void)fPropagateCleanupError;

    if(!OI_PTR_IS_VALID(pOpenIno))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else if(pOpenIno->uRefs == 0U)
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        if(pOpenIno->uRefs == 1U)
        {
          #if ((REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FRESERVE == 1)) || (DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1))
            if(fDoCleanup && !gpRedVolume->fReadOnly)
            {
              #if REDCONF_VOLUME_COUNT > 1U
                ret = RedCoreVolSetCurrent(pOpenIno->bVolNum);
                if(ret == 0)
              #endif
                {
                  #if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FRESERVE == 1)
                    if((pOpenIno->bFlags & OIFLAG_RESERVED) != 0U)
                    {
                        ret = RedCoreFileUnreserve(pOpenIno->ulInode, pOpenIno->ullResOff);
                    }

                    if(ret == 0)
                  #endif
                    {
                      #if DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1)
                        if((pOpenIno->bFlags & OIFLAG_ORPHAN) != 0U)
                        {
                            ret = RedCoreFreeOrphan(pOpenIno->ulInode);
                        }
                      #endif
                    }
                }

                if(!fPropagateCleanupError)
                {
                    ret = 0;
                }
            }

            if(ret == 0)
          #endif
            {
                pOpenIno->ulInode = INODE_INVALID;
            }
        }

        if(ret == 0)
        {
            pOpenIno->uRefs--;
        }
    }

    return ret;
}


/** @brief Enter the file system driver.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL The file system driver is uninitialized.
    @retval -RED_EUSERS Cannot become a file system user: too many users.
*/
static REDSTATUS PosixEnter(void)
{
    REDSTATUS ret = 0;

    if(gfPosixInited)
    {
      #if REDCONF_TASK_COUNT > 1U
        RedOsMutexAcquire();

        if(TaskRegister() == NULL)
        {
            ret = -RED_EUSERS;
            RedOsMutexRelease();
        }
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


#if DELETE_SUPPORTED
/** @brief Check whether an inode can be deleted.

    If an inode has a link count of 1 (meaning unlinking another name would
    result in the deletion of the inode) and is referenced, it cannot be deleted
    since this would break those references.  It can be orphaned if unlinking of
    open inodes is supported.

    @param ulInode  The inode whose name is to be unlinked.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EBADF  @p ulInode is not a valid inode.
    @retval -RED_EBUSY  The inode has a link count of one and is referenced.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS InodeUnlinkCheck(
    uint32_t    ulInode)
{
    REDSTATUS   ret = 0;

    if(OpenInoFind(gbRedVolNum, ulInode, false) != NULL)
    {
      #if REDCONF_API_POSIX_LINK == 1
        REDSTAT InodeStat;

        ret = RedCoreStat(ulInode, &InodeStat);

        if((ret == 0) && (InodeStat.st_nlink == 1U))
      #endif
        {
            ret = -RED_EBUSY;
        }
    }

    return ret;
}
#endif /* DELETE_SUPPORTED */


#if DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1)
/** @brief Mark an open inode to indicate that its link count is 0.

    @param ulInode  The inode number.
*/
static void InodeOrphaned(
    uint32_t    ulInode)
{
    OPENINODE  *pOpenIno = OpenInoFind(gbRedVolNum, ulInode, false);

    if(pOpenIno != NULL)
    {
        REDASSERT(pOpenIno->uRefs > 0U);
        REDASSERT((pOpenIno->bFlags & OIFLAG_ORPHAN) == 0U);

        pOpenIno->bFlags |= OIFLAG_ORPHAN;
    }
}
#endif /* DELETE_SUPPORTED && (REDCONF_DELETE_OPEN == 1) */


/** @brief Populate a buffer with the path to a directory inode.

    @param ulDirInode   The inode number.
    @param pszBuffer    The buffer to populate with the path.
    @param ulBufferSize The size in bytes of @p pszBuffer.
    @param ulFlags      The only flag value is #RED_GETDIRPATH_NOVOLUME, which
                        means to exclude the volume path prefix for the path put
                        into @p pszBuffer.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p pszBuffer is `NULL`; or @p ulBufferSize is zero; or
                        @p ulFlags is invalid.
    @retval -RED_EIO    A disk I/O error occurred.
    @retval -RED_ENOENT #REDCONF_DELETE_OPEN is true and @p ulDirInode refers to
                        a directory that has been removed.
    @retval -RED_ERANGE @p ulBufferSize is greater than zero but too small for
                        the path string.
*/
static REDSTATUS DirInodeToPath(
    uint32_t    ulDirInode,
    char       *pszBuffer,
    uint32_t    ulBufferSize,
    uint32_t    ulFlags)
{
    uint32_t    ulInode = ulDirInode;
    uint32_t    ulPInode;
    uint32_t    ulPathLen; /* Length includes terminating NUL */
    REDSTATUS   ret = 0;

    if((pszBuffer == NULL) || (ulBufferSize == 0U) || ((ulFlags & ~RED_GETDIRPATH_NOVOLUME) != 0U))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        pszBuffer[0U] = '\0';
        ulPathLen = 1U;
    }

    /*  Work our way up the path, converting the inode numbers to names,
        building the path in reverse, until we reach the root directory.
    */
    while((ret == 0) && (ulInode != INODE_ROOTDIR))
    {
        /*  The name buffer is static in case REDCONF_NAME_MAX is too big to fit
            on the stack; we're single-threaded so this is safe.  The variable
            name includes "DirInodeToPath" since that might be preserved in the
            linker's map file, making it easier to determine who is allocating
            this memory.
        */
        static char szDirInodeToPathName[REDCONF_NAME_MAX + 1U];
        uint32_t    ulDirPos = 0U;

        /*  Scan the parent directory to convert this inode into a name.  Hard
            linking is prohibited for directories so the inode will have only
            one parent inode and one name.
        */
        ret = RedCoreDirParent(ulInode, &ulPInode);
        while(ret == 0)
        {
            uint32_t ulThisInode;

            ret = RedCoreDirRead(ulPInode, &ulDirPos, szDirInodeToPathName, &ulThisInode);
            if((ret == 0) && (ulThisInode == ulInode))
            {
                /*  Found the matching name.
                */
                break;
            }

            /*  If we get to the end of the parent directory without finding the
                inode of the child directory, something is wrong -- probably
                file system corruption.
            */
            if(ret == -RED_ENOENT)
            {
                REDERROR();
                ret = -RED_EFUBAR;
            }
        }

        /*  Shift the contents of pszBuffer to the right and copy in the next
            name.  For example, if the path is "a/b/c", the contents of
            pszBuffer will be "", then "c", then "b/c", then "a/b/c".
        */
        if(ret == 0)
        {
            /*  Skip the path separator for the first name so that we end up
                with "a/b/c" instead of "a/b/c/".
            */
            bool fPathSeparator = (ulInode != ulDirInode);
            uint32_t ulNameLen = RedNameLen(szDirInodeToPathName);
            uint32_t ulNewLen = ulNameLen;

            if(fPathSeparator)
            {
                ulNewLen++; /* For path separator */
            }

            if((ulPathLen + ulNewLen) > ulBufferSize)
            {
                /*  The path buffer provided by the caller is too small.
                */
                ret = -RED_ERANGE;
            }
            else
            {
                RedMemMove(&pszBuffer[ulNewLen], pszBuffer, ulPathLen);
                RedMemCpy(pszBuffer, szDirInodeToPathName, ulNameLen);
                if(fPathSeparator)
                {
                    pszBuffer[ulNameLen] = REDCONF_PATH_SEPARATOR;
                }

                ulPathLen += ulNewLen;
            }
        }

        /*  Move up the path to the parent directory.
        */
        if(ret == 0)
        {
            ulInode = ulPInode;
        }
    }

    /*  Copy in the volume path prefix, followed by a leading slash for the root
        directory.
    */
    if(ret == 0)
    {
        const char *pszVolume = gpRedVolConf->pszPathPrefix;
        uint32_t    ulVolPrefixLen;

        if((ulFlags & RED_GETDIRPATH_NOVOLUME) != 0U)
        {
            pszVolume = "";
        }

        ulVolPrefixLen = RedStrLen(pszVolume);
        if((ulPathLen + ulVolPrefixLen + 1U) > ulBufferSize)
        {
            /*  The path buffer provided by the caller is too small.
            */
            ret = -RED_ERANGE;
        }
        else
        {
            RedMemMove(&pszBuffer[ulVolPrefixLen + 1U], pszBuffer, ulPathLen);
            RedMemCpy(pszBuffer, gpRedVolConf->pszPathPrefix, ulVolPrefixLen);
            pszBuffer[ulVolPrefixLen] = REDCONF_PATH_SEPARATOR;
        }
    }

    return ret;
}


#if (REDCONF_API_POSIX_CWD == 1) || (REDCONF_TASK_COUNT > 1U)
/** @brief Find the task slot for the calling task.

    @return On success, returns a pointer to the ::TASKSLOT for the calling
            task.  If the calling task is not registered, returns `NULL`.
*/
static TASKSLOT *TaskFind(void)
{
  #if REDCONF_TASK_COUNT == 1U
    /*  Return the one and only task slot.
    */
    return &gaTask[0U];
  #else
    uint32_t    ulIdx;
    uint32_t    ulTaskId = RedOsTaskId();
    TASKSLOT   *pTask = NULL;

    REDASSERT(ulTaskId != 0U);

    for(ulIdx = 0U; ulIdx < ARRAY_SIZE(gaTask); ulIdx++)
    {
        if(gaTask[ulIdx].ulTaskId == ulTaskId)
        {
            pTask = &gaTask[ulIdx];
            break;
        }
    }

    return pTask;
  #endif
}
#endif /* (REDCONF_API_POSIX_CWD == 1) || (REDCONF_TASK_COUNT > 1U) */


#if REDCONF_TASK_COUNT > 1U
/** @brief Register a task as a file system user, if it is not already
           registered as one.

    The caller must hold the FS mutex.

    @return On success, returns a pointer to the task slot assigned to the
            calling task.  If the task was not previously registered, and there
            are no free task slots, returns NULL.
*/
static TASKSLOT *TaskRegister(void)
{
    uint32_t    ulTaskId = RedOsTaskId();
    uint32_t    ulIdx;
    TASKSLOT   *pTask = NULL;
    TASKSLOT   *pFreeTask = NULL;

    REDASSERT(ulTaskId != 0U);

    /*  Scan the task slots to determine if the task is registered as a file
        system task.
    */
    for(ulIdx = 0U; ulIdx < ARRAY_SIZE(gaTask); ulIdx++)
    {
        if(gaTask[ulIdx].ulTaskId == ulTaskId)
        {
            pTask = &gaTask[ulIdx];
            break;
        }

        if((pFreeTask == NULL) && (gaTask[ulIdx].ulTaskId == 0U))
        {
            pFreeTask = &gaTask[ulIdx];
        }
    }

    if((pTask == NULL) && (pFreeTask != NULL))
    {
        pTask = pFreeTask;
        pTask->ulTaskId = ulTaskId;
    }

    return pTask;
}
#endif /* REDCONF_TASK_COUNT > 1U */


#if REDCONF_API_POSIX_CWD == 1
/** @brief Close all current working directory (CWD) references on the current
           volume, returning them to the root directory.

    @param fReset   Whether in-memory state should be discarded (true) or
                    cleaned up (false).

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    @p fReset is false, the CWD inode has been unlinked,
                        and a disk I/O error occurred while freeing the inode.
*/
static REDSTATUS CwdCloseVol(
    bool        fReset)
{
    REDSTATUS   ret = 0;
    uint32_t    ulIdx;

    for(ulIdx = 0U; ulIdx < ARRAY_SIZE(gaTask); ulIdx++)
    {
        TASKSLOT *pTask = &gaTask[ulIdx];

        if((pTask->pCwd != NULL) && (pTask->pCwd->bVolNum == gbRedVolNum))
        {
            ret = CwdClose(pTask, false, fReset);
            if(ret != 0)
            {
                break;
            }
        }
    }

    return ret;
}


/** @brief Reset all current working directories (CWD) to the default.
*/
static void CwdResetAll(void)
{
    uint32_t ulIdx;

    for(ulIdx = 0U; ulIdx < ARRAY_SIZE(gaTask); ulIdx++)
    {
        (void)CwdClose(&gaTask[ulIdx], true, true);
    }
}


/** @brief Close one task's CWD reference.

    @param pTask        The task slot.
    @param fClearVol    Whether to reset the volume number to 0.
    @param fReset       Whether in-memory state should be discarded (true) or
                        cleaned up (false).

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    @p fReset is false, the CWD inode has been unlinked,
                        and a disk I/O error occurred while freeing the inode.
*/
static REDSTATUS CwdClose(
    TASKSLOT   *pTask,
    bool        fClearVol,
    bool        fReset)
{
    REDSTATUS   ret = 0;

    if(pTask->pCwd != NULL)
    {
        /*  This operation is unrelated to unlinking, thus errors freeing an
            orphan should be ignored.
        */
        ret = OpenInoDeref(pTask->pCwd, !fReset, false);
    }

    if(ret == 0)
    {
        uint8_t bVolNum = fClearVol ? 0U : gbRedVolNum;

        pTask->pCwd = OpenInoFind(bVolNum, INODE_ROOTDIR, true);
        if(pTask->pCwd != NULL)
        {
            pTask->pCwd->uRefs++;
        }
        else
        {
            REDERROR();
            ret = -RED_EFUBAR;
        }
    }

    return ret;
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
