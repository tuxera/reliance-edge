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
    @brief Implements POSIX permission checks.
*/
#include <redfs.h>

#if (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1)

#include <redstat.h>


/** @brief Check whether the caller has permission to perform an operation.

    @param bAccess  Combination of `RED_*_OK` bits indicating desired access.
    @param uMode    Mode of the file.
    @param ulUid    UID of the file.
    @param ulGid    GID of the file.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EACCES Permission denied.
*/
REDSTATUS RedPermCheck(
    uint8_t     bAccess,
    uint16_t    uMode,
    uint32_t    ulUid,
    uint32_t    ulGid)
{
    REDSTATUS   ret;

    REDASSERT((bAccess & ~RED_MASK_OK) == 0U);

  #if REDOSCONF_PERM_OVERRIDE == 1
    ret = RedOsPermCheck(bAccess, uMode, ulUid, ulGid);
  #else
    ret = 0;
    if(!RedOsIsPrivileged())
    {
        bool fUib = (RedOsUserId() == ulUid);
        bool fGib = RedOsIsGroupMember(ulGid);

        if(    (    ((bAccess & RED_X_OK) != 0U)
                 && (fUib
                      ? ((uMode & RED_S_IXUSR) == 0U)
                      : (fGib
                          ? ((uMode & RED_S_IXGRP) == 0U)
                          : ((uMode & RED_S_IXOTH) == 0U))))
            || (    ((bAccess & RED_W_OK) != 0U)
                 && (fUib
                      ? ((uMode & RED_S_IWUSR) == 0U)
                      : (fGib
                          ? ((uMode & RED_S_IWGRP) == 0U)
                          : ((uMode & RED_S_IWOTH) == 0U))))
            || (    ((bAccess & RED_R_OK) != 0U)
                 && (fUib
                      ? ((uMode & RED_S_IRUSR) == 0U)
                      : (fGib
                          ? ((uMode & RED_S_IRGRP) == 0U)
                          : ((uMode & RED_S_IROTH) == 0U)))))
        {
            ret = -RED_EACCES;
        }
    }
  #endif /* REDOSCONF_PERM_OVERRIDE == 1 */

    return ret;
}


/** @brief Check whether the caller has permission to unlink a file.

    @param uPMode   Mode of the parent directory.
    @param ulPUid   UID of the parent directory.
    @param ulPGid   GID of the parent directory.
    @param ulFUid   UID of the file.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EACCES Permission denied.
*/
REDSTATUS RedPermCheckUnlink(
    uint16_t    uPMode,
    uint32_t    ulPUid,
    uint32_t    ulPGid,
    uint32_t    ulFUid)
{
    REDSTATUS   ret;

  #if REDOSCONF_PERM_OVERRIDE == 1
    ret = RedOsPermCheckUnlink(uPMode, ulPUid, ulPGid, ulFUid);
  #else
    ret = 0;
    if(!RedOsIsPrivileged())
    {
        ret = RedPermCheck(RED_X_OK | RED_W_OK, uPMode, ulPUid, ulPGid);

        if(    (ret == 0)
            && ((uPMode & RED_S_ISVTX) != 0U)
            && (RedOsUserId() != ulFUid)
            && (RedOsUserId() != ulPUid))
        {
            ret = -RED_EACCES;
        }
    }
  #endif

    return ret;
}

#endif /* (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1) */
