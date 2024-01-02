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
    @brief File type checking utility function.
*/
#include <redfs.h>

#if REDCONF_API_POSIX == 1

#include <redstat.h>


/** @brief Check that a mode is consistent with the given expected type.

    @param uMode        An inode mode, indicating the inode type.
    @param expectedType The expected type of the file descriptor: one or more of
                        #FTYPE_DIR, #FTYPE_FILE, #FTYPE_SYMLINK.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0               Operation was successful.
    @retval -RED_EISDIR     #RED_S_ISDIR(@p uMode) is true, but @p expectedType
                            does not include #FTYPE_DIR.
    @retval -RED_ENOLINK    Either #RED_S_ISLNK(@p uMode) is true, or
                            @p expectedType is exactly #FTYPE_SYMLINK, but not
                            both.  Takes precedence over other error conditions.
    @retval -RED_ENOTDIR    #RED_S_ISREG(@p uMode) is true, but @p expectedType
                            does not include #FTYPE_FILE.
*/
REDSTATUS RedModeTypeCheck(
    uint16_t    uMode,
    FTYPE       expectedType)
{
    FTYPE       modeType;

    if(RED_S_ISDIR(uMode))
    {
        modeType = FTYPE_DIR;
    }
  #if REDCONF_API_POSIX_SYMLINK == 1
    else if(RED_S_ISLNK(uMode))
    {
        modeType = FTYPE_SYMLINK;
    }
  #endif
    else if(RED_S_ISREG(uMode))
    {
        modeType = FTYPE_FILE;
    }
    else
    {
        REDERROR();
        modeType = 0U;
    }

    return RedFileTypeCheck(modeType, expectedType);
}


/** @brief Check that a file type is consistent with the given expected type.

    @param actualType   The file type: exactly one of #FTYPE_DIR, #FTYPE_FILE,
                        #FTYPE_SYMLINK.
    @param expectedType The expected type: one or more of #FTYPE_DIR,
                        #FTYPE_FILE, #FTYPE_SYMLINK.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0               Operation was successful.
    @retval -RED_EISDIR     @p actualType is #FTYPE_DIR, but @p expectedType
                            does not include #FTYPE_DIR.
    @retval -RED_ENOLINK    @p actualType and @p expectedType mismatch, and
                            either @p actualType or @p expectedType are exactly
                            #FTYPE_SYMLINK.  Takes precedence over other error
                            conditions.
    @retval -RED_ENOTDIR    @p actualType is #FTYPE_FILE, but @p expectedType
                            does not include #FTYPE_FILE.
*/
REDSTATUS RedFileTypeCheck(
    FTYPE       actualType,
    FTYPE       expectedType)
{
    REDSTATUS   ret;

    if((actualType & expectedType) == 0U)
    {
      #if REDCONF_API_POSIX_SYMLINK == 1
        if((actualType == FTYPE_SYMLINK) || (expectedType == FTYPE_SYMLINK))
        {
            ret = -RED_ENOLINK;
        }
        else
      #endif
        if(actualType == FTYPE_DIR)
        {
            ret = -RED_EISDIR;
        }
        else
        {
            REDASSERT(actualType == FTYPE_FILE);

            ret = -RED_ENOTDIR;
        }
    }
    else
    {
        /*  File type is compatible with expected type(s).
        */
        ret = 0;
    }

    return ret;

}


#endif /* REDCONF_API_POSIX == 1 */

