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
    @brief Implements path utilities for the POSIX-like API layer.
*/
#include <redfs.h>

#if REDCONF_API_POSIX == 1

#include <redcoreapi.h>
#include <redvolume.h>
#include <redposix.h>
#include <redpath.h>


#if (REDCONF_API_POSIX_SYMLINK == 1) && (REDOSCONF_SYMLINK_FOLLOW == 1)

/*  The maximum supported "depth" which can be resolved when symbolic links
    point at other symbolic links ("nested" symlinks).  When this limit is
    exceeded, a RED_ELOOP error is returned.

    In POSIX, SYMLOOP_MAX is required to be at least 8, and is defined as:
    "Maximum number of symbolic links that can be reliably traversed in the
    resolution of a pathname in the absence of a loop."  In our implementation,
    there is no limit on the total number of symbolic links, only on the depth
    of the nesting.  There is no special handling for loops, they are followed
    until this limit is reached.

    The following tables show some examples.  The "Symlink Name" is the name of
    the symlink itself, e.g., the final component of the red_symlink()
    @p pszSymlink parameter.  The "Symlink Target" is the contents of the
    symlink, what it "points at", e.g., the red_symlink() @p pszPath parameter.
    The "Follow Result" is the expected result of resolving a path which
    includes the "Symlink Name".

    Nested symlinks can be resolved until the depth limit is exceeded:

    Symlink Name | Symlink Target | Follow Result
    ------------ | -------------- | -------------
    LINK0        | FILE           | OK
    LINK1        | LINK0          | OK
    LINK2        | LINK1          | OK
    LINK3        | LINK2          | OK
    LINK4        | LINK3          | OK
    LINK5        | LINK4          | OK
    LINK6        | LINK5          | OK
    LINK7        | LINK6          | OK
    LINK8        | LINK7          | RED_ELOOP

    Symlinks which point back to themselves, directly or indirectly, cannot be
    resolved:

    Symlink Name | Symlink Target | Follow Result
    ------------ | -------------- | -------------
    LINK_SELF    | LINK_SELF      | RED_ELOOP
    LINK_CIRC0   | LINK_CIRC1     | RED_ELOOP
    LINK_CIRC1   | LINK_CIRC0     | RED_ELOOP

    In the above table, assume that both "LINK_CIRC0" and "LINK_CIRC1" have been
    created prior to the attempt to resolve either of them.  If only one of
    those two symlinks exists, the "Follow Result" would be RED_ENOENT instead.
*/
#define RED_SYMLOOP_MAX 8U

/*  SYMLINKCTX::bIdx value which means "we are _not_ within a symbolic link".
    Must be UINT8_MAX: it's assumed that incrementing from this value will
    result in zero and that decrementing from zero will result in this value.
*/
#define SYMLINK_IDX_NONE UINT8_MAX

/** @brief Data stored for each level of nested symbolic link parsing.
*/
typedef struct
{
    uint32_t ulInode;   /**< Symbolic link inode number. */
    uint64_t ullPos;    /**< Symbolic link byte offset. */
} SYMLINKLEVEL;

/** @brief Context structure for parsing symbolic links during PathWalk().
*/
typedef struct
{
    /** Index into current position in the aStack array.  The initial value is
        SYMLINK_IDX_NONE, which means that no symlink is being parsed.  It is
        incremented to zero when a symlink is encountered and incremented and
        decremented as nested symlinks are entered and exited.
    */
    uint8_t         bIdx;

    SYMLINKLEVEL    aStack[RED_SYMLOOP_MAX]; /**< Symlink stack array. */

    /** Buffer for reading the contents (the target) of a symbolic link.  Only
        one name is read from the symbolic link at a time.

        Size: +1 for NUL terminator, +1 for RED_ENAMETOOLONG check.
    */
    char            szName[REDCONF_NAME_MAX + 2U];
} SYMLINKCTX;

#endif /* (REDCONF_API_POSIX_SYMLINK == 1) && (REDOSCONF_SYMLINK_FOLLOW == 1) */

/*  Lookup the last name in the path.

    Coexists with RED_AT_SYMLINK_NOFOLLOW in PATHWALKCTX::ulFlags.
*/
#define PW_LOOKUP_LAST 0x80000000U

/** @brief Context structure for PathWalk() and its helper functions.
*/
typedef struct
{
    const char *pszPath;        /**< The path being walked. */
    const char *pszLastName;    /**< Final name component in pszPath. */
    const char *pszName;        /**< Current name. */
    uint32_t    ulPathIdx;      /**< Index into pszPath. */
    uint32_t    ulPInode;       /**< Parent inode for ulInode or for the next name. */
    uint32_t    ulInode;        /**< Current inode. */
    uint32_t    ulFlags;        /**< Path parsing flags. */
  #if (REDCONF_API_POSIX_SYMLINK == 1) && (REDOSCONF_SYMLINK_FOLLOW == 1)
    SYMLINKCTX  symlink;        /**< Symbolic link context. */
  #endif
} PATHWALKCTX;


static REDSTATUS PathWalk(uint32_t ulDirInode, const char *pszLocalPath, uint32_t ulFlags, REDSTATUS rootDirError, uint32_t *pulPInode, const char **ppszName, uint32_t *pulInode);
static REDSTATUS PathWalkNext(PATHWALKCTX *pCtx);
static REDSTATUS PathWalkFollow(PATHWALKCTX *pCtx);
#if (REDCONF_API_POSIX_SYMLINK == 1) && (REDOSCONF_SYMLINK_FOLLOW == 1)
static REDSTATUS SymlinkNext(PATHWALKCTX *pCtx);
#endif
static bool IsRootDir(const char *pszLocalPath);
static bool PathHasMoreComponents(const char *pszPathIdx);
#if REDCONF_API_POSIX_CWD == 1
static bool IsDot(const char *pszPathComponent);
static bool IsDotDot(const char *pszPathComponent);
static bool IsDotOrDotDot(const char *pszPathComponent);
static REDSTATUS InodeMustBeSearchableDir(uint32_t ulInode);
#endif


/** @brief Convert a volume path prefix to a volume number.

    As a side-effect, the volume named by the path prefix becomes the current
    volume.

    @param pszPath  The path which includes the volume path prefix to parse.
                    Characters after the volume path prefix are ignored.
    @param pbVolNum On successful return, populated with the volume number
                    associated with the named volume.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p pszPath or @p pbVolNum is `NULL`.
    @retval -RED_ENOENT @p pszPath could not be matched to any volume.
*/
REDSTATUS RedPathVolumePrefixLookup(
    const char *pszPath,
    uint8_t    *pbVolNum)
{
    REDSTATUS   ret = 0;

    if((pszPath == NULL) || (pbVolNum == NULL))
    {
        ret = -RED_EINVAL;
    }
    else
    {
        uint8_t     bMatchVol = UINT8_MAX;
        uint32_t    ulMatchLen = 0U;
        uint8_t     bDefaultVolNum = UINT8_MAX;
        uint8_t     bVolNum;

        for(bVolNum = 0U; bVolNum < REDCONF_VOLUME_COUNT; bVolNum++)
        {
            const char *pszPrefix = gaRedVolConf[bVolNum].pszPathPrefix;
            uint32_t    ulPrefixLen = RedStrLen(pszPrefix);

            if(ulPrefixLen == 0U)
            {
                /*  A volume with a path prefix of an empty string is the
                    default volume, used when the path does not match the
                    prefix of any other volume.

                    The default volume should only be found once.  During
                    initialization, RedCoreInit() ensures that all volume
                    prefixes are unique (including empty prefixes).
                */
                REDASSERT(bDefaultVolNum == UINT8_MAX);
                bDefaultVolNum = bVolNum;
            }
            /*  For a path to match, it must either be the prefix exactly, or
                be followed by a path separator character.  Thus, with a volume
                prefix of "/foo", both "/foo" and "/foo/bar" are matches, but
                "/foobar" is not.
            */
            else if(    (RedStrNCmp(pszPath, pszPrefix, ulPrefixLen) == 0)
                     && ((pszPath[ulPrefixLen] == '\0') || (pszPath[ulPrefixLen] == REDCONF_PATH_SEPARATOR)))
            {
                /*  The length of this match should never exactly equal the
                    length of a previous match: that would require a duplicate
                    volume name, which should have been detected during init.
                */
                REDASSERT(ulPrefixLen != ulMatchLen);

                /*  If multiple prefixes match, the longest takes precedence.
                    Thus, if there are two prefixes "Flash" and "Flash/Backup",
                    the path "Flash/Backup/" will not be erroneously matched
                    with the "Flash" volume.
                */
                if(ulPrefixLen > ulMatchLen)
                {
                    bMatchVol = bVolNum;
                    ulMatchLen = ulPrefixLen;
                }
            }
            else
            {
                /*  No match, keep looking.
                */
            }
        }

        if(bMatchVol != UINT8_MAX)
        {
            /*  The path matched a volume path prefix.
            */
            bVolNum = bMatchVol;
        }
        else if(bDefaultVolNum != UINT8_MAX)
        {
            /*  The path didn't match any of the prefixes, but one of the
                volumes has a path prefix of "", so an unprefixed path is
                assigned to that volume.
            */
            bVolNum = bDefaultVolNum;
            REDASSERT(ulMatchLen == 0U);
        }
        else
        {
            /*  The path cannot be assigned a volume.
            */
            ret = -RED_ENOENT;
        }

      #if REDCONF_VOLUME_COUNT > 1U
        if(ret == 0)
        {
            ret = RedCoreVolSetCurrent(bVolNum);
        }
      #endif

        if(ret == 0)
        {
            *pbVolNum = bVolNum;
        }
    }

    return ret;
}


/** @brief Convert a volume name to a volume number.

    As a side-effect, the named volume becomes the current volume.

    @param pszVolume    The volume path to parse.  Any characters beyond the
                        volume name, other than path separators, will result in
                        an error.
    @param pbVolNum     If non-NULL, on successful return, populated with the
                        volume number associated with the named volume.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p pszVolume is `NULL`.
    @retval -RED_ENOENT @p pszVolume could not be matched to any volume: this
                        includes the case where @p pszVolume begins with a
                        volume prefix but contains further characters other than
                        path separators.
*/
REDSTATUS RedPathVolumeLookup(
    const char *pszVolume,
    uint8_t    *pbVolNum)
{
    uint8_t     bVolNum;
    REDSTATUS   ret;

    ret = RedPathVolumePrefixLookup(pszVolume, &bVolNum);
    if(ret == 0)
    {
        const char *pszExtraPath = &pszVolume[RedStrLen(gpRedVolConf->pszPathPrefix)];

        /*  Since this string is expected to name a volume, it should either
            terminate after the volume prefix or contain only path separators.
            Allowing path separators here means that red_mount("/data/") is OK
            with a path prefix of "/data".
        */
        if(pszExtraPath[0U] != '\0')
        {
            if(!IsRootDir(pszExtraPath))
            {
                ret = -RED_ENOENT;
            }
        }
    }

    if((ret == 0) && (pbVolNum != NULL))
    {
        *pbVolNum = bVolNum;
    }

    return ret;
}


/** @brief Lookup the inode named by the given path.

    @param ulDirInode   The directory inode from which to start parsing
                        @p pszLocalPath.  If @p pszLocalPath is an absolute
                        path, this should be `INODE_ROOTDIR`.
    @param pszLocalPath The path to lookup; this is a local path, without any
                        volume prefix.
    @param ulFlags      Either zero or #RED_AT_SYMLINK_NOFOLLOW.  If the latter,
                        and @p pszLocalPath names a symbolic link, then
                        @p pulInode is populated with the inode number of the
                        symbolic link, rather than the inode number of what the
                        symbolic link points at.
    @param pulInode     On successful return, populated with the number of the
                        inode named by @p pszLocalPath.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0                   Operation was successful.
    @retval -RED_EACCES         Permission denied: #REDCONF_POSIX_OWNER_PERM is
                                enabled and the current user lacks search
                                permissions for a component of the path other
                                than the last.
    @retval -RED_EINVAL         @p pszLocalPath is `NULL`; or @p pulInode is
                                `NULL`.
    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_ENOENT         @p pszLocalPath does not name an existing file
                                or directory.
    @retval -RED_ENOLINK        #REDCONF_API_POSIX_SYMLINK is true and
                                #REDOSCONF_SYMLINK_FOLLOW is false and a
                                component of the path other than the last is a
                                symbolic link.
    @retval -RED_ENOTDIR        A component of the path other than the last is
                                not a directory.
    @retval -RED_ENAMETOOLONG   The length of a component of @p pszLocalPath is
                                longer than #REDCONF_NAME_MAX.

    The following errors are possible only when #REDCONF_API_POSIX_SYMLINK and
    #REDOSCONF_SYMLINK_FOLLOW are both true:

    @retval -RED_ELOOP          The path cannot be resolved because it either
                                contains a symbolic link loop or nested symbolic
                                links which exceed the nesting limit.
    @retval -RED_ENOENT         The path references a symbolic link (possibly
                                indirectly, via another symbolic link) which
                                contains an empty string.
    @retval -RED_ENAMETOOLONG   The path references a symbolic link (possibly
                                indirectly, via another symbolic link) which
                                contains a name longer than #REDCONF_NAME_MAX.
*/
REDSTATUS RedPathLookup(
    uint32_t    ulDirInode,
    const char *pszLocalPath,
    uint32_t    ulFlags,
    uint32_t   *pulInode)
{
    REDSTATUS   ret;

    if(pulInode == NULL)
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        ret = PathWalk(ulDirInode, pszLocalPath, ulFlags, 0, NULL, NULL, pulInode);
    }

    return ret;
}


/** @brief Given a path, return the parent inode number and a pointer to the
           last component in the path (the name).

    @param ulDirInode   The directory inode from which to start parsing
                        @p pszLocalPath.  If @p pszLocalPath is an absolute
                        path, this should be `INODE_ROOTDIR`.
    @param pszLocalPath The path to examine; this is a local path, without any
                        volume prefix.
    @param rootDirError Error to return if the path resolves to the root
                        directory.  Must be nonzero, since this function cannot
                        populate @p pulPInode or @p ppszName for the root
                        directory.
    @param pulPInode    On successful return, populated with the inode number of
                        the parent directory of the last component in the path.
                        For example, with the path "a/b/c", populated with the
                        inode number of "b".
    @param ppszName     On successful return, populated with a pointer to the
                        last component in the path.  For example, with the path
                        "a/b/c", populated with a pointer to "c".

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0                   Operation was successful.
    @retval -RED_EACCES         Permission denied: #REDCONF_POSIX_OWNER_PERM is
                                enabled and the current user lacks search
                                permissions for a component of the path other
                                than the last.
    @retval -RED_EINVAL         @p pszLocalPath is `NULL`; or @p pulPInode is
                                `NULL`; or @p ppszName is `NULL`; or
                                @p rootDirError is zero; or
                                #REDCONF_API_POSIX_CWD is true and the last
                                component of the path is dot or dot-dot.
    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_ENOENT         A component of @p pszLocalPath other than the
                                last does not exist.
    @retval -RED_ENOLINK        #REDCONF_API_POSIX_SYMLINK is true and
                                #REDOSCONF_SYMLINK_FOLLOW is false and a
                                component of the path other than the last is a
                                symbolic link.
    @retval -RED_ENOTDIR        A component of the path other than the last is
                                not a directory.
    @retval -RED_ENAMETOOLONG   The length of a component of @p pszLocalPath is
                                longer than #REDCONF_NAME_MAX.
    @retval rootDirError        The path names the root directory.

    The following errors are possible only when #REDCONF_API_POSIX_SYMLINK and
    #REDOSCONF_SYMLINK_FOLLOW are both true:

    @retval -RED_ELOOP          The path (excluding the final path component)
                                cannot be resolved because it either contains a
                                symbolic link loop or nested symbolic links
                                which exceed the nesting limit.
    @retval -RED_ENOENT         The path (excluding the final path component)
                                references a symbolic link (possibly indirectly,
                                via another symbolic link) which contains an
                                empty string.
    @retval -RED_ENAMETOOLONG   The path (excluding the final path component)
                                references a symbolic link (possibly indirectly,
                                via another symbolic link) which contains a name
                                longer than #REDCONF_NAME_MAX.
*/
REDSTATUS RedPathToName(
    uint32_t        ulDirInode,
    const char     *pszLocalPath,
    REDSTATUS       rootDirError,
    uint32_t       *pulPInode,
    const char    **ppszName)
{
    REDSTATUS       ret;

    if((rootDirError == 0) || (pulPInode == NULL) || (ppszName == NULL))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        ret = PathWalk(ulDirInode, pszLocalPath, 0U, rootDirError, pulPInode, ppszName, NULL);
      #if REDCONF_API_POSIX_CWD == 1
        if(ret == 0)
        {
            /*  Error if the last path component is dot or dot-dot.  For some
                of the callers, an error is required by POSIX; for the others,
                the complexity of allowing this is not worth it.  The Linux
                implementation of the relevant POSIX functions always fail when
                the path ends with dot or dot-dot, though not always for the
                same reason or with the same errno.
            */
            if(IsDotOrDotDot(*ppszName))
            {
                /*  Depending on the situation, POSIX would have this fail with
                    one of several different errors: EINVAL, ENOTEMPTY, EEXIST,
                    EISDIR, or EPERM.  For simplicity, we ignore those
                    distinctions and return the same error in all cases.
                */
                ret = -RED_EINVAL;
            }
        }
      #endif
    }

    return ret;
}


/** @brief Walk the given path to the file or directory it names, producing a
           parent inode, final path component, and/or inode number.

    @param ulDirInode   The directory inode from which to start parsing
                        @p pszLocalPath.  If @p pszLocalPath is an absolute
                        path, this should be `INODE_ROOTDIR`.
    @param pszLocalPath The path to examine; this is a local path, without any
                        volume prefix.
    @param ulFlags      Either zero or #RED_AT_SYMLINK_NOFOLLOW.  If the latter,
                        and @p pszLocalPath names a symbolic link, then
                        @p pulInode (if non-NULL) is populated with the inode
                        number of the symbolic link, rather than the inode
                        number of what the symbolic link points at.
    @param rootDirError Error to return if @p pulPInode is non-`NULL` and the
                        path resolves to the root directory.  Must be nonzero
                        unless @p pulPInode is `NULL`.
    @param pulPInode    On successful return, if non-NULL, populated with the
                        inode number of the parent directory of the last
                        component in the path.  For example, with the path
                        "a/b/c", populated with the inode number of "b".
    @param ppszName     On successful return, if non-NULL, populated with a
                        pointer to the last component in the path.  For example,
                        with the path "a/b/c", populated with a pointer to "c".
                        With the path "a/..", populated with a pointer to "..".
    @param pulInode     On successful return, if non-NULL, populated with the
                        number of the inode named by @p pszLocalPath.  If NULL,
                        the last component in the path is not looked-up, and
                        there will be no error if it does not exist.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0                   Operation was successful.
    @retval -RED_EACCES         Permission denied: #REDCONF_POSIX_OWNER_PERM is
                                enabled and the current user lacks search
                                permissions for a component of the path other
                                than the last.
    @retval -RED_EINVAL         @p pszLocalPath is `NULL`; or @p ulDirInode is
                                invalid; or @p pulPInode is non-`NULL`,
                                indicating the caller wanted the parent inode,
                                but @p rootDirError is zero, which gives us no
                                error to return if the path names the root
                                directory (which has no parent inode).
    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_ENOENT         A component of @p pszLocalPath does not exist
                                (possibly excepting the last component,
                                depending on whether @p pulInode is NULL); or
                                #REDCONF_DELETE_OPEN is true and @p ulDirInode
                                refers to a directory that has been removed.
    @retval -RED_ENOLINK        #REDCONF_API_POSIX_SYMLINK is true and
                                #REDOSCONF_SYMLINK_FOLLOW is false and a
                                component of the path other than the last is a
                                symbolic link.
    @retval -RED_ENOTDIR        A component of the path other than the last is
                                not a directory.
    @retval -RED_ENAMETOOLONG   The length of a component of @p pszLocalPath is
                                longer than #REDCONF_NAME_MAX.
    @retval rootDirError        @p pulPInode is non-`NULL` and the path names
                                the root directory.

    The following errors are possible only when #REDCONF_API_POSIX_SYMLINK and
    #REDOSCONF_SYMLINK_FOLLOW are both true:

    @retval -RED_ELOOP          Walking the path encountered either a symbolic
                                link loop or a nested symbolic link which could
                                not be resolved due to exceeding the nesting
                                limit.
    @retval -RED_ENOENT         Walking the path encountered a symbolic link
                                which contains an empty string.
    @retval -RED_ENAMETOOLONG   Walking the path encountered a symbolic link
                                which contains a name longer than
                                #REDCONF_NAME_MAX.
*/
static REDSTATUS PathWalk(
    uint32_t        ulDirInode,
    const char     *pszLocalPath,
    uint32_t        ulFlags,
    REDSTATUS       rootDirError,
    uint32_t       *pulPInode,
    const char    **ppszName,
    uint32_t       *pulInode)
{
    REDSTATUS       ret = 0;

    if(    (ulDirInode == INODE_INVALID)
        || (pszLocalPath == NULL)
        || ((ulFlags & RED_AT_SYMLINK_NOFOLLOW) != ulFlags)
        || ((pulPInode != NULL) && (rootDirError == 0)))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        PATHWALKCTX ctx = {0U};

        ctx.pszPath = pszLocalPath;
        ctx.pszLastName = pszLocalPath;
        ctx.ulInode = ulDirInode;
        ctx.ulFlags = ulFlags | ((pulInode != NULL) ? PW_LOOKUP_LAST : 0U);
      #if (REDCONF_API_POSIX_SYMLINK == 1) && (REDOSCONF_SYMLINK_FOLLOW == 1)
        ctx.symlink.bIdx = SYMLINK_IDX_NONE;
      #endif

        ret = RedCoreDirParent(ctx.ulInode, &ctx.ulPInode);

        while(ret == 0)
        {
            ret = PathWalkNext(&ctx);

            if(ret == 0)
            {
                if(ctx.pszName == NULL)
                {
                    break; /* Reached the end of the path. */
                }

                ret = PathWalkFollow(&ctx);
            }
        }

        if(ret == 0)
        {
            if(pulPInode != NULL)
            {
                if(ctx.ulPInode == INODE_INVALID)
                {
                    /*  If we get here, the path resolved the root directory,
                        which has no parent inode.
                    */
                    ret = rootDirError;
                }
                else
                {
                    *pulPInode = ctx.ulPInode;
                }
            }

            if(ppszName != NULL)
            {
                *ppszName = ctx.pszLastName;
            }

            if(pulInode != NULL)
            {
                *pulInode = ctx.ulInode;
            }
        }
    }

    return ret;
}


/** @brief Determine the next name to follow to continue walking a path.

    @param pCtx PathWalk() context pointer.  On successful return,
                `pCtx->pszName` points at the next name; if the pointer is
                `NULL`, then there are no more names in the path.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p pCtx (or the path pointer therein) is `NULL`.

    The following errors are possible only when #REDCONF_API_POSIX_SYMLINK and
    #REDOSCONF_SYMLINK_FOLLOW are both true:

    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_ELOOP          The next name comes from a newly-encountered
                                symbolic link which cannot be read because the
                                maximum supported depth of symbolic link nesting
                                has been reached.
    @retval -RED_ENOENT         The next name comes from a symbolic link and is
                                an empty string.
    @retval -RED_ENAMETOOLONG   The next name comes from a symbolic link and is
                                longer than #REDCONF_NAME_MAX.
*/
static REDSTATUS PathWalkNext(
    PATHWALKCTX    *pCtx)
{
    REDSTATUS       ret = 0;

    if((pCtx == NULL) || (pCtx->pszPath == NULL))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        /*  Default to telling the caller that the path has no more names.
        */
        pCtx->pszName = NULL;

        /*  If PathWalkFollow() reaches the end of the path while PW_LOOKUP_LAST
            is clear, then the inode is invalid.  In such cases, we already know
            that there isn't a next path component.
        */
        if(pCtx->ulInode != INODE_INVALID)
        {
          #if (REDCONF_API_POSIX_SYMLINK == 1) && (REDOSCONF_SYMLINK_FOLLOW == 1)
            ret = SymlinkNext(pCtx);
            if((ret == 0) && (pCtx->pszName == NULL))
          #endif
            {
                /*  Skip over path separators, to get pszPath[ulPathIdx]
                    pointing at the next path component.
                */
                while(pCtx->pszPath[pCtx->ulPathIdx] == REDCONF_PATH_SEPARATOR)
                {
                    pCtx->ulPathIdx++;
                }

                if(pCtx->pszPath[pCtx->ulPathIdx] != '\0')
                {
                    pCtx->pszName = &pCtx->pszPath[pCtx->ulPathIdx];

                    /*  Point pszLastName at the first character of the path
                        component; at the end of PathWalk(), it will point at
                        the first character of the very last path component
                        (name, dot, or dot-dot).
                    */
                    pCtx->pszLastName = pCtx->pszName;

                    /*  Move on to the next path component.
                    */
                    pCtx->ulPathIdx += RedNameLen(pCtx->pszName);
                }
            }
        }
    }

    return ret;
}


/** @brief Follow the next name in the path being walked.

    @param pCtx PathWalk() context pointer.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0                   Operation was successful.
    @retval -RED_EACCES         Permission denied: #REDCONF_POSIX_OWNER_PERM is
                                enabled and the current user lacks search
                                permissions for the parent directory of the next
                                name.
    @retval -RED_EINVAL         @p pCtx (or the path/name pointers therein) are
                                `NULL`.
    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_ENOENT         The next name does not exist in its parent
                                directory.
    @retval -RED_ENOLINK        #REDCONF_API_POSIX_SYMLINK is true and
                                #REDOSCONF_SYMLINK_FOLLOW is false and a
                                component of the path other than the last is a
                                symbolic link.
    @retval -RED_ENOTDIR        The parent "directory" was not a directory.
    @retval -RED_ENAMETOOLONG   The next name is longer than #REDCONF_NAME_MAX.
*/
static REDSTATUS PathWalkFollow(
    PATHWALKCTX    *pCtx)
{
    REDSTATUS       ret = 0;

    /*  On entry:
        - pCtx->pszName is the next path component to be followed
        - pCtx->ulInode is the parent directory for the name
        - pCtx->ulPInode is the grandparent directory for the name

        On exit:
        - pCtx->pszName is unchanged
        - pCtx->ulInode is the inode number of the name: unless the name was the
          final path component and PW_LOOKUP_LAST was clear, in which case it is
          INODE_INVALID
        - pCtx->ulPInode is the parent directory for the name
    */
    if((pCtx == NULL) || (pCtx->pszPath == NULL) || (pCtx->pszName == NULL))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
  #if REDCONF_API_POSIX_CWD == 1
    else if(IsDot(pCtx->pszName))
    {
        /*  E.g., "foo/." is valid only if "foo" is a searchable directory.
        */
        ret = InodeMustBeSearchableDir(pCtx->ulInode);

        /*  Nothing else to do: with a dot entry, we're already where we need to
            be for the next path component.
        */
    }
    else if(IsDotDot(pCtx->pszName))
    {
        /*  E.g., "foo/.." is valid only if "foo" is a searchable directory.
        */
        ret = InodeMustBeSearchableDir(pCtx->ulInode);
        if(ret == 0)
        {
            /*  "As a special case, in the root directory, dot-dot may refer to
                the root directory itself."  So sayeth POSIX.  Although it says
                "may", this seems to be the norm (e.g., on Linux), so implement
                that behavior here.
            */
            if(pCtx->ulInode != INODE_ROOTDIR)
            {
                /*  Update ulInode to its parent.
                */
                pCtx->ulInode = pCtx->ulPInode;
            }

            /*  Update ulPInode to be ulInode's parent -- if there are no more
                names in the path, this is needed for ulPInode to be correct
                when the loop ends.
            */
            ret = RedCoreDirParent(pCtx->ulInode, &pCtx->ulPInode);
        }
    }
  #endif /* REDCONF_API_POSIX_CWD == 1 */
    else
    {
        bool fLookup;

        /*  Point ulPInode at the parent inode.  At the end of PathWalk(), this
            will point at the parent inode of the last name.
        */
        pCtx->ulPInode = pCtx->ulInode;

        if((pCtx->ulFlags & PW_LOOKUP_LAST) != 0U)
        {
            /*  When PW_LOOKUP_LAST is set, every name in the path is looked up.
            */
            fLookup = true;
        }
        else
        {
            /*  Otherwise, only lookup the name if it's not the final path
                component.

                This only needs to check the top-level path; symlinks need not
                be considered.  The only case where the top-level path has no
                more components, but there can be more symlink components, is
                when the symlink is the final path component; but the final path
                component is only followed when PW_LOOKUP_LAST is set, and if it
                is, we never come here.
            */
            fLookup = PathHasMoreComponents(&pCtx->pszPath[pCtx->ulPathIdx]);
        }

        if(fLookup)
        {
            /*  Lookup the inode of the name.
            */
            ret = RedCoreLookup(pCtx->ulPInode, pCtx->pszName, &pCtx->ulInode);
        }
        else
        {
            /*  Since the lookup was skipped, the inode number for the name is
                unknown.  PathWalkNext() checks for this condition.
            */
            pCtx->ulInode = INODE_INVALID;
        }
    }

    return ret;
}


#if (REDCONF_API_POSIX_SYMLINK == 1) && (REDOSCONF_SYMLINK_FOLLOW == 1)
/** @brief Retrieve the next symbolic link name, if any.

    @param pCtx PathWalk() context pointer.  On successful return,
                `pCtx->pszName` points at the next name; if the pointer is
                `NULL`, then there is no symbolic link name, and path walking
                should continue with the top-level path.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0                   Operation was successful.
    @retval -RED_EINVAL         @p pCtx (or the path pointer therein) is `NULL`;
                                or `pCtx->pszName` is non-`NULL`.
    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_ELOOP          The next name comes from a newly-encountered
                                symbolic link which cannot be read because the
                                maximum supported depth of symbolic link nesting
                                has been reached.
    @retval -RED_ENOENT         The next name comes from a symbolic link and is
                                an empty string.
    @retval -RED_ENAMETOOLONG   The next name comes from a symbolic link and is
                                longer than #REDCONF_NAME_MAX.
*/
static REDSTATUS SymlinkNext(
    PATHWALKCTX    *pCtx)
{
    REDSTATUS       ret = 0;

    /*  pCtx->pszName == NULL indicates that there is no next symlink name.
        This function is called from PathWalkNext(), which initializes that
        pointer to NULL, so it's not initialized here.
    */
    if((pCtx == NULL) || (pCtx->pszPath == NULL) || (pCtx->pszName != NULL))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        SYMLINKCTX *pSymlinkCtx = &pCtx->symlink;

        /*  As an optimization, the below can be skipped for the root directory:
            we know that's not a symlink.
        */
        if(pCtx->ulInode != INODE_ROOTDIR)
        {
            REDSTAT sb;

            /*  Check if the last-resolved name was a symlink.
            */
            ret = RedCoreStat(pCtx->ulInode, &sb);
            if((ret == 0) && RED_S_ISLNK(sb.st_mode))
            {
                bool fFollow;

                /*  It's a symlink.  Do we follow it?
                */
                if((pCtx->ulFlags & RED_AT_SYMLINK_NOFOLLOW) != 0U)
                {
                    /*  If RED_AT_SYMLINK_NOFOLLOW was specified, we only follow
                        the symlink if it is _not_ the final path component.

                        Note that we only need to check the top-level path.  If
                        the top-level path has no more components, the only case
                        where there are still symlinks to be followed is when
                        the final path component was a symlink and NOFOLLOW was
                        clear -- but we don't come here if NOFOLLOW was clear.
                    */
                    fFollow = PathHasMoreComponents(&pCtx->pszPath[pCtx->ulPathIdx]);
                }
                else
                {
                    /*  Otherwise, we follow every symlink.
                    */
                    fFollow = true;
                }

                if(fFollow)
                {
                    /*  Initialize our context structure for a newly-visited
                        symlink.
                    */
                    pSymlinkCtx->bIdx++;

                    if(pSymlinkCtx->bIdx >= ARRAY_SIZE(pSymlinkCtx->aStack))
                    {
                        ret = -RED_ELOOP;
                    }
                    else
                    {
                        pSymlinkCtx->aStack[pSymlinkCtx->bIdx].ulInode = pCtx->ulInode;
                        pSymlinkCtx->aStack[pSymlinkCtx->bIdx].ullPos = 0U;

                        /*  If the symlink contains a relative path, the first
                            component of the relative path is parsed from the
                            parent directory of the symlink, not from the
                            symlink inode.  If the symlink contains an absolute
                            path, the ulInode will be changed to INODE_ROOTDIR
                            in the loop below.
                        */
                        pCtx->ulInode = pCtx->ulPInode;
                        ret = RedCoreDirParent(pCtx->ulInode, &pCtx->ulPInode);
                    }
                }
            }
        }

        /*  While:
            a) No error; and
            b) Within a symbolic link; and
            c) The next name has not yet been found.
        */
        while(    (ret == 0)
               && (pSymlinkCtx->bIdx != SYMLINK_IDX_NONE)
               && (pCtx->pszName == NULL))
        {
            SYMLINKLEVEL   *pLvl = &pSymlinkCtx->aStack[pSymlinkCtx->bIdx];
            uint32_t        ulNameIdx;
            uint32_t        ulReadLen;
            const uint32_t  ulReadMax = REDCONF_NAME_MAX + 1U; /* Extra byte for ENAMETOOLONG check */
            bool            fReadRetry;

            /*  Retry loop for when leading or redundant path separators require
                us to read more than once in order to get the whole name.  This
                loop can execute many times if there is a run of redundant path
                separators which is many times the maximum name length.
            */
            do
            {
                ulNameIdx = 0U;
                fReadRetry = false;

                /*  Read from the symbolic link inode.
                */
                ulReadLen = ulReadMax;
                ret = RedCoreFileRead(pLvl->ulInode, pLvl->ullPos, &ulReadLen, pSymlinkCtx->szName);
                if(ret == 0)
                {
                    /*  Make sure the name is NUL terminated.  The buffer is one
                        byte longer than the read request, so there is always
                        room for the NUL.
                    */
                    REDASSERT(ulReadLen < sizeof(pSymlinkCtx->szName));
                    pSymlinkCtx->szName[ulReadLen] = '\0';

                    if((ulReadLen > 0U) && (pSymlinkCtx->szName[0U] == REDCONF_PATH_SEPARATOR))
                    {
                        /*  If we find a path separator in the first byte of the
                            symbolic link, then it's an absolute path.
                        */
                        if(pLvl->ullPos == 0U)
                        {
                            pCtx->ulPInode = INODE_INVALID;
                            pCtx->ulInode = INODE_ROOTDIR;
                        }

                        /*  Symbolic links may contain redundant path separator
                            characters.  Skip over the path separator characters
                            at the start of the name buffer.
                        */
                        do
                        {
                            pLvl->ullPos++;
                            ulNameIdx++;
                        } while(pSymlinkCtx->szName[ulNameIdx] == REDCONF_PATH_SEPARATOR);

                        /*  Check if we have a complete name after skipping over
                            the path separators.  We know the name is complete
                            if: a) we reached EOF while reading it; or b) the
                            name is terminated by a path separator within the
                            data that we read.
                        */
                        if(    (ulReadLen == ulReadMax)
                            && ((ulNameIdx + RedNameLen(&pSymlinkCtx->szName[ulNameIdx])) == ulReadLen))
                        {
                            /*  Due to the path separators taking up space in
                                the name buffer, we didn't read the whole name.
                                Return to the top of the loop and reread at the
                                new position.
                            */
                            fReadRetry = true;
                        }
                    }
                }
            } while((ret == 0) && fReadRetry);

            if(ret == 0)
            {
                /*  The symbolic link is exhausted if: a) we have reached the
                    EOF, as indicated by zero bytes being read; or b) the next
                    byte in the symlink data is a NUL character, in which case
                    all subsequent data (if any) is ignored.
                */
                if((ulReadLen == 0U) || (pSymlinkCtx->szName[ulNameIdx] == '\0'))
                {
                    if(pLvl->ullPos == 0U)
                    {
                        /*  The symbolic link is empty.  According to POSIX,
                            this should provoke an ENOENT error.
                        */
                        ret = -RED_ENOENT;
                    }
                    else
                    {
                        /*  We have reached the end of a symbolic link that was
                            not empty.  Move back up the stack to resume parsing
                            the previous symbolic link (if any).
                        */
                        pSymlinkCtx->bIdx--;
                    }
                }
                else
                {
                    uint32_t ulNameLen = RedNameLen(&pSymlinkCtx->szName[ulNameIdx]);

                    if(ulNameLen > REDCONF_NAME_MAX)
                    {
                        /*  Because symlink target paths are not validated, it's
                            possible for them to contain names which are too
                            long.
                        */
                        ret = -RED_ENAMETOOLONG;
                    }
                    else
                    {
                        /*  Found the next name.
                        */
                        pCtx->pszName = &pSymlinkCtx->szName[ulNameIdx];

                        /*  Update the position within the symbolic link inode.
                        */
                        pLvl->ullPos += ulNameLen;

                        /*  If the name is terminated by a path separator,
                            move beyond it (as an optimization for the next
                            iteration).  However, if the name is terminated
                            by a NUL, don't increment over it, since that
                            would interfere with detecting the termination
                            of a symlink with a NUL before EOF.
                        */
                        if(pCtx->pszName[ulNameLen] == REDCONF_PATH_SEPARATOR)
                        {
                            pLvl->ullPos++;
                        }
                    }
                }
            }
        }
    }

    return ret;
}
#endif /* (REDCONF_API_POSIX_SYMLINK == 1) && (REDOSCONF_SYMLINK_FOLLOW == 1) */


/** @brief Determine whether a path names the root directory.

    @param pszLocalPath The path to examine; this is a local path, without any
                        volume prefix.

    @return Returns whether @p pszLocalPath names the root directory.

    @retval true    @p pszLocalPath names the root directory.
    @retval false   @p pszLocalPath does not name the root directory.
*/
static bool IsRootDir(
    const char *pszLocalPath)
{
    bool        fRet;

    if(pszLocalPath == NULL)
    {
        REDERROR();
        fRet = false;
    }
    else
    {
        uint32_t    ulIdx = 0U;

        /*  A string containing nothing but path separators (usually only one)
            names the root directory.  An empty string does *not* name the root
            directory, since in POSIX empty strings typically elicit -RED_ENOENT
            errors.
        */
        while(pszLocalPath[ulIdx] == REDCONF_PATH_SEPARATOR)
        {
            ulIdx++;
        }

        fRet = (ulIdx > 0U) && (pszLocalPath[ulIdx] == '\0');
    }

    return fRet;
}


/** @brief Determine whether there are more components in a path.

    A "component" is a name, dot, or dot-dot.

    Example | Result
    ------- | ------
    ""        false
    "/"       false
    "//"      false
    "a"       true
    "/a"      true
    "//a"     true
    ".."      true
    "/."      true

    @param pszPathIdx   The path to examine, incremented to the point of
                        interest.

    @return Returns whether there are more components in @p pszPathIdx.

    @retval true    @p pszPathIdx has more components.
    @retval false   @p pszPathIdx has no more components.
*/
static bool PathHasMoreComponents(
    const char *pszPathIdx)
{
    bool        fRet;

    if(pszPathIdx == NULL)
    {
        REDERROR();
        fRet = false;
    }
    else
    {
        uint32_t ulIdx = 0U;

        while(pszPathIdx[ulIdx] == REDCONF_PATH_SEPARATOR)
        {
            ulIdx++;
        }

        fRet = pszPathIdx[ulIdx] != '\0';
    }

    return fRet;
}


#if REDCONF_API_POSIX_CWD == 1
/** @brief Determine whether a path component is dot.

    @param pszPathComponent The path component to examine.

    @return Returns whether @p pszPathComponent is dot.

    @retval true    @p pszPathComponent is dot.
    @retval false   @p pszPathComponent is not dot.
*/
static bool IsDot(
    const char *pszPathComponent)
{
    /*  Match "." or "./"
    */
    return (pszPathComponent != NULL)
        && (pszPathComponent[0U] == '.')
        && ((pszPathComponent[1U] == '\0') || (pszPathComponent[1U] == REDCONF_PATH_SEPARATOR));
}


/** @brief Determine whether a path component is dot-dot.

    @param pszPathComponent The path component to examine.

    @return Returns whether @p pszPathComponent is dot-dot.

    @retval true    @p pszPathComponent is dot-dot.
    @retval false   @p pszPathComponent is not dot-dot.
*/
static bool IsDotDot(
    const char *pszPathComponent)
{
    /*  Match ".." or "../"
    */
    return (pszPathComponent != NULL)
        && (pszPathComponent[0U] == '.')
        && (pszPathComponent[1U] == '.')
        && ((pszPathComponent[2U] == '\0') || (pszPathComponent[2U] == REDCONF_PATH_SEPARATOR));
}


/** @brief Determine whether a path component is dot or dot-dot.

    @param pszPathComponent The path component to examine.

    @return Returns whether @p pszPathComponent is dot or dot-dot.

    @retval true    @p pszPathComponent is dot or dot-dot.
    @retval false   @p pszPathComponent is not dot or dot-dot.
*/
static bool IsDotOrDotDot(
    const char *pszPathComponent)
{
    bool fRet;

    fRet = IsDot(pszPathComponent);
    if(!fRet)
    {
        fRet = IsDotDot(pszPathComponent);
    }

    return fRet;
}


/** @brief Make sure the given inode is a searchable directory.

    @param ulInode  The inode to examine.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0               Operation was successful: @p ulInode is a directory.
    @retval -RED_EACCES     #REDCONF_POSIX_OWNER_PERM is true and @p ulInode is
                            a directory without search permissions.
    @retval -RED_EBADF      @p ulInode is not a valid inode.
    @retval -RED_EIO        A disk I/O error occurred.
    @retval -RED_ENOTDIR    @p ulInode is not a directory.
    @retval -RED_ENOLINK    #REDCONF_API_POSIX_SYMLINK is true and @p ulInode is
                            a symbolic link.
*/
static REDSTATUS InodeMustBeSearchableDir(
    uint32_t    ulInode)
{
    REDSTATUS   ret;

    /*  When permissions are disabled, all we're doing here is checking whether
        the inode is a directory -- and the root directory is, by definition, a
        directory.
    */
  #if REDCONF_POSIX_OWNER_PERM == 0
    if(ulInode == INODE_ROOTDIR)
    {
        ret = 0;
    }
    else
  #endif
    {
        REDSTAT sb;

        ret = RedCoreStat(ulInode, &sb);

        if(ret == 0)
        {
            ret = RedModeTypeCheck(sb.st_mode, FTYPE_DIR);
        }

      #if REDCONF_POSIX_OWNER_PERM == 1
        if(ret == 0)
        {
            ret = RedPermCheck(RED_X_OK, sb.st_mode, sb.st_uid, sb.st_gid);
        }
      #endif
    }

    return ret;
}
#endif /* REDCONF_API_POSIX_CWD == 1 */

#endif /* REDCONF_API_POSIX */
