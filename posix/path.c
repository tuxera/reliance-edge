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
    @brief Implements path utilities for the POSIX-like API layer.
*/
#include <redfs.h>

#if REDCONF_API_POSIX == 1

#include <redcoreapi.h>
#include <redvolume.h>
#include <redposix.h>
#include <redpath.h>


static REDSTATUS PathWalk(uint32_t ulCwdInode, const char *pszLocalPath, REDSTATUS rootDirError, uint32_t *pulPInode, const char **ppszName, uint32_t *pulInode);
static bool IsRootDir(const char *pszLocalPath);
static bool PathHasMoreComponents(const char *pszPathIdx);
#if REDCONF_API_POSIX_CWD == 1
static bool IsDot(const char *pszPathComponent);
static bool IsDotDot(const char *pszPathComponent);
static bool IsDotOrDotDot(const char *pszPathComponent);
static REDSTATUS InodeMustBeDir(uint32_t ulInode);
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

    @param ulCwdInode   The current working directory inode: i.e., the directory
                        inode from which to start parsing @p pszLocalPath.  If
                        @p pszLocalPath is an absolute path, this should be
                        `INODE_ROOTDIR`.
    @param pszLocalPath The path to lookup; this is a local path, without any
                        volume prefix.
    @param pulInode     On successful return, populated with the number of the
                        inode named by @p pszLocalPath.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0                   Operation was successful.
    @retval -RED_EINVAL         @p pszLocalPath is `NULL`; or @p pulInode is
                                `NULL`.
    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_ENOENT         @p pszLocalPath does not name an existing file
                                or directory.
    @retval -RED_ENOTDIR        A component of the path other than the last is
                                not a directory.
    @retval -RED_ENAMETOOLONG   The length of a component of @p pszLocalPath is
                                longer than #REDCONF_NAME_MAX.
*/
REDSTATUS RedPathLookup(
    uint32_t    ulCwdInode,
    const char *pszLocalPath,
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
        ret = PathWalk(ulCwdInode, pszLocalPath, 0, NULL, NULL, pulInode);
    }

    return ret;
}


/** @brief Given a path, return the parent inode number and a pointer to the
           last component in the path (the name).

    @param ulCwdInode   The current working directory inode: i.e., the directory
                        inode from which to start parsing @p pszLocalPath.  If
                        @p pszLocalPath is an absolute path, this should be
                        `INODE_ROOTDIR`.
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
    @retval -RED_EINVAL         @p pszLocalPath is `NULL`; or @p pulPInode is
                                `NULL`; or @p ppszName is `NULL`; or
                                @p rootDirError is zero; or
                                #REDCONF_API_POSIX_CWD is true and the last
                                component of the path is dot or dot-dot.
    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_ENOENT         A component of @p pszLocalPath other than the
                                last does not exist.
    @retval -RED_ENOTDIR        A component of the path other than the last is
                                not a directory.
    @retval -RED_ENAMETOOLONG   The length of a component of @p pszLocalPath is
                                longer than #REDCONF_NAME_MAX.
    @retval rootDirError        The path names the root directory.
*/
REDSTATUS RedPathToName(
    uint32_t        ulCwdInode,
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
        ret = PathWalk(ulCwdInode, pszLocalPath, rootDirError, pulPInode, ppszName, NULL);
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

    @param ulCwdInode   The current working directory inode: i.e., the directory
                        inode from which to start parsing @p pszLocalPath.  If
                        @p pszLocalPath is an absolute path, this should be
                        `INODE_ROOTDIR`.
    @param pszLocalPath The path to examine; this is a local path, without any
                        volume prefix.
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
    @retval -RED_EINVAL         @p pszLocalPath is `NULL`; or @p ulCwdInode is
                                invalid; or @p pulPInode is non-`NULL`,
                                indicating the caller wanted the parent inode,
                                but @p rootDirError is zero, which gives us no
                                error to return if the path names the root
                                directory (which has no parent inode).
    @retval -RED_EIO            A disk I/O error occurred.
    @retval -RED_ENOENT         A component of @p pszLocalPath does not exist
                                (possibly excepting the last component,
                                depending on whether @p pulInode is NULL).
    @retval -RED_ENOTDIR        A component of the path other than the last is
                                not a directory.
    @retval -RED_ENAMETOOLONG   The length of a component of @p pszLocalPath is
                                longer than #REDCONF_NAME_MAX.
    @retval rootDirError        @p pulPInode is non-`NULL` and the path names
                                the root directory.
*/
static REDSTATUS PathWalk(
    uint32_t        ulCwdInode,
    const char     *pszLocalPath,
    REDSTATUS       rootDirError,
    uint32_t       *pulPInode,
    const char    **ppszName,
    uint32_t       *pulInode)
{
    REDSTATUS       ret = 0;

    if(
         #if REDCONF_API_POSIX_CWD == 1
           (ulCwdInode == INODE_INVALID)
         #else
           (ulCwdInode != INODE_ROOTDIR)
         #endif
        || (pszLocalPath == NULL)
        || ((pulPInode != NULL) && (rootDirError == 0)))
    {
        REDERROR();
        ret = -RED_EINVAL;
    }
    else
    {
        uint32_t ulInode = ulCwdInode;
        uint32_t ulPInode;
        uint32_t ulPathIdx = 0U;
        uint32_t ulLastNameIdx = 0U;

      #if REDCONF_API_POSIX_CWD == 1
        ret = RedCoreDirParent(ulInode, &ulPInode);
      #else
        ulPInode = INODE_INVALID;
      #endif

        while(ret == 0)
        {
            /*  Skip over path separators, to get pszLocalPath[ulPathIdx]
                pointing at the next path component.
            */
            while(pszLocalPath[ulPathIdx] == REDCONF_PATH_SEPARATOR)
            {
                ulPathIdx++;
            }

            if(pszLocalPath[ulPathIdx] == '\0')
            {
                break;
            }

            /*  Point ulLastNameIdx at the first character of the path
                component; after we exit the loop, it will point at the first
                character of the very last path component (name, dot, or
                dot-dot).
            */
            ulLastNameIdx = ulPathIdx;

          #if REDCONF_API_POSIX_CWD == 1
            if(IsDot(&pszLocalPath[ulPathIdx]))
            {
                /*  E.g., "foo/." is valid only if "foo" is a directory.
                */
                ret = InodeMustBeDir(ulInode);
                if(ret == 0)
                {
                    ulPathIdx++; /* Move past the "." to the next component */
                }
            }
            else if(IsDotDot(&pszLocalPath[ulPathIdx]))
            {
                /*  E.g., "foo/.." is valid only if "foo" is a directory.
                */
                ret = InodeMustBeDir(ulInode);
                if(ret == 0)
                {
                    /*  "As a special case, in the root directory, dot-dot may
                        refer to the root directory itself."  So sayeth POSIX.
                        Although it says "may", this seems to be the norm (e.g.,
                        on Linux), so implement that behavior here.
                    */
                    if(ulInode != INODE_ROOTDIR)
                    {
                        /*  Update ulInode to its parent.
                        */
                        ulInode = ulPInode;
                    }

                    /*  Update ulPInode to be ulInode's parent -- if there are
                        no more names in the path, this is needed for ulPInode
                        to be correct when the loop ends.
                    */
                    ret = RedCoreDirParent(ulInode, &ulPInode);
                }

                if(ret == 0)
                {
                    ulPathIdx += 2U; /* Move past the ".." to the next component */
                }
            }
            else
          #endif /* REDCONF_API_POSIX_CWD == 1 */
            {
                uint32_t ulNameLen;

                /*  Point ulPInode at the parent inode.  After we exit the loop,
                    this will point at the parent inode of the last name.
                */
                ulPInode = ulInode;

                ulNameLen = RedNameLen(&pszLocalPath[ulPathIdx]);

                /*  Lookup the inode of the name; if we are at the last name in
                    the path, only lookup the inode if requested.
                */
                if(    PathHasMoreComponents(&pszLocalPath[ulPathIdx + ulNameLen])
                    || (pulInode != NULL))
                {
                    ret = RedCoreLookup(ulPInode, &pszLocalPath[ulPathIdx], &ulInode);
                }

                /*  Move on to the next path component.
                */
                if(ret == 0)
                {
                    ulPathIdx += ulNameLen;
                }
            }
        }

        if((ret == 0) && (pulPInode != NULL))
        {
            if(ulPInode == INODE_INVALID)
            {
                /*  If we get here, the path resolved the root directory, which
                    has no parent inode.
                */
                ret = rootDirError;
            }
            else
            {
                *pulPInode = ulPInode;
            }
        }

        if(ret == 0)
        {
            if(ppszName != NULL)
            {
                *ppszName = &pszLocalPath[ulLastNameIdx];
            }

            if(pulInode != NULL)
            {
                *pulInode = ulInode;
            }
        }
    }

    return ret;
}


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


/** @brief Make sure the given inode is a directory.

    @param ulInode  The inode to examine.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0               Operation was successful: @p ulInode is a directory.
    @retval -RED_EBADF      @p ulInode is not a valid inode.
    @retval -RED_EIO        A disk I/O error occurred.
    @retval -RED_ENOTDIR    @p ulInode is not a directory.
*/
static REDSTATUS InodeMustBeDir(
    uint32_t    ulInode)
{
    REDSTATUS   ret = 0;

    /*  Root directory is, by definition, a directory.
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

    return ret;
}
#endif /* REDCONF_API_POSIX_CWD == 1 */

#endif /* REDCONF_API_POSIX */

