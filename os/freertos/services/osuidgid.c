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
    @brief Implements user and group ID functionality.
*/
#include <redfs.h>

#if (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1)


/** @brief Get the effective user ID (UID).

    Notes:
    - Zero is assumed to be the root user.
    - In some environments, this should be the user ID associated with the
      current file system request, rather than the user ID of the running
      process.
    - On operating systems where UID is not a meaningful concept, this can
      return a hard-coded value, such as zero.

    @return The user ID of the process which invoked the file system.
*/
uint32_t RedOsUserId(void)
{
    return RED_ROOT_USER;
}


/** @brief Get the effective group ID (GID).

    Notes:
    - Zero is assumed to be the root group.
    - In some environments, this should be the group ID associated with the
      current file system request, rather than the group ID of the running
      process.
    - On operating systems where GID is not a meaningful concept, this can
      return a hard-coded value, such as zero.

    @return The group ID of the process which invoked the file system.
*/
uint32_t RedOsGroupId(void)
{
    return 0U;
}


/** @brief Determine whether the current user is a member of the given group.

    In most POSIX systems, users have both a primary group and supplemental
    groups.  This function should return true if the current user is a member of
    the @p ulGid group, either as the primary group or via supplemental group
    memberships.

    If the operating environments does not implement supplemental groups, this
    function can be implemented as a one-liner.

    @code{.c}
    return RedOsGroupId() == ulGid;
    @endcode

    If the operating environment does implement supplemental groups, in addition
    to the above check, this function needs to retrieve the list of supplemental
    groups and return true if @p ulGid is among the supplemental groups.

    @param ulGid    The ID of the group.

    @return Whether the current user is a member of the @p ulGid group.

    @retval true    The current user is a member of the @p ulGid group.
    @retval false   The current user is _not_ a member of the @p ulGid group.
*/
bool RedOsIsGroupMember(
    uint32_t    ulGid)
{
    return RedOsGroupId() == ulGid;
}


/** @brief Check whether the process is "privileged", as-per POSIX.

    In many systems, this amounts to whether the effective user ID is root.

    @return Whether or not the process is privileged.
*/
bool RedOsIsPrivileged(void)
{
    return RedOsUserId() == RED_ROOT_USER;
}

#endif /* (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1) */
