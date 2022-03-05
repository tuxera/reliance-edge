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
    @brief Implements test utilites for format options.
*/
#include <redfs.h>
#include <redposix.h>

#if (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FORMAT == 1)

#include <redtestutils.h>


/** @brief Retrieve the options that were used when the volume was formatted.

    If @p pszVolume refers to an unformatted volume (i.e., it's not mounted
    and attempting to mount it fails with an I/O error), then @p pFmtOpt
    is populated with zeroed options (default values) and this function
    returns success.

    @p pszVolume may be either mounted or unmounted.  If initially mounted,
    it remains mounted.  If initially unmounted, an attempt is made to mount
    it and (if mount succeeded) it is then unmounted.

    @param pszVolume    A path prefix identifying the volume to query.
    @param pFmtOpt      On success, populated with the format options.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.
*/
int32_t RedTestFmtOptionsGet(
    const char *pszVolume,
    REDFMTOPT  *pFmtOpt)
{
    REDSTATUS   errnoSave = red_errno;
    bool        fUnmount = true;
    int32_t     status;

    /*  Zeroed options means to use the defaults.
    */
    RedMemSet(pFmtOpt, 0U, sizeof(*pFmtOpt));

    /*  Look for a pre-existing format.
    */
    status = red_mount(pszVolume);
    if((status != 0) && (red_errno == RED_EBUSY))
    {
        /*  The volume was already mounted.  Not an error, keep going.
        */
        status = 0;
        red_errno = errnoSave;
        fUnmount = false; /* Don't unmount it, leave it as we found it. */
    }

    if(status == 0)
    {
        REDSTATFS fsinfo;

        status = red_statvfs(pszVolume, &fsinfo);
        if(status != 0)
        {
          #if REDCONF_OUTPUT == 1
            RedPrintf("Unexpected error %d from red_statvfs()\n", (int)-red_errno);
          #endif
        }
        else
        {
            /*  Preserve the settings of the pre-existing format when
                reformatting.
            */
            pFmtOpt->ulVersion = fsinfo.f_diskver;
            pFmtOpt->ulInodeCount = fsinfo.f_files;
        }

        if(fUnmount)
        {
            if(status != 0)
            {
                (void)red_umount(pszVolume);
            }
            else
            {
                status = red_umount(pszVolume);
              #if REDCONF_OUTPUT == 1
                if(status != 0)
                {
                    RedPrintf("Unexpected error %d from red_umount()\n", (int)-red_errno);
                }
              #endif
            }
        }
    }
    else if(red_errno == RED_EIO)
    {
        /*  Volume not formatted.  Clear the error, caller will see a zeroed
            options structure (the defaults).
        */
        status = 0;
        red_errno = errnoSave;
    }
    else
    {
      #if REDCONF_OUTPUT == 1
        RedPrintf("Unexpected error %d from red_mount()\n", (int)-red_errno);
      #endif
    }

    return status;
}


/** @brief Format while preserving format options from an existing format.

    Note that the technique used by this function is not reliable in cases
    where format could be interrupted by power loss.  The first thing that
    format does is overwrite the master block with zeroes; the last thing that
    format does is write the new master block.  If power is lost between those
    two steps, then it is no longer possible to preserve the original format
    settings by reading the master block.  This function is only used with tests
    where power loss is not a concern; it is not intended to be a model for
    applications, which should instead explicitly specify the format options
    that they want.

    @param pszVolume    A path prefix identifying the volume to format.

    @return On success, zero is returned.  On error, -1 is returned and
            #red_errno is set appropriately.
*/
int32_t RedTestFmtOptionsPreserve(
    const char *pszVolume)
{
    REDFMTOPT   fo;
    int32_t     status;

    status = RedTestFmtOptionsGet(pszVolume, &fo);
    if(status == 0)
    {
        status = red_format2(pszVolume, &fo);
      #if REDCONF_OUTPUT == 1
        if(status != 0)
        {
            RedPrintf("Unexpected error %d from red_format2()\n", (int)-red_errno);
        }
      #endif
    }

    return status;
}
#endif /* (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX == 1) && (REDCONF_API_POSIX_FORMAT == 1) */
