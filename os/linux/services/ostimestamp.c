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
    @brief Implements timestamp functions.

    The functionality implemented herein is not needed for the file system
    driver, only to provide accurate results with performance tests.
*/
#include <redfs.h>

#include <sys/time.h>


/** @brief Initialize the timestamp service.

    The behavior of invoking this function when timestamps are already
    initialized is undefined.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0   Operation was successful.
*/
REDSTATUS RedOsTimestampInit(void)
{
    return 0;
}


/** @brief Uninitialize the timestamp service.

    The behavior of invoking this function when timestamps are not initialized
    is undefined.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0   Operation was successful.
*/
REDSTATUS RedOsTimestampUninit(void)
{
    return 0;
}


/** @brief Retrieve a timestamp.

    The behavior of invoking this function when timestamps are not initialized
    is undefined

    @return A timestamp which can later be passed to RedOsTimePassed() to
            determine the amount of time which passed between the two calls.
*/
REDTIMESTAMP RedOsTimestamp(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return (REDTIMESTAMP)((tv.tv_sec * (uint64_t)1000000U) + tv.tv_usec);
}


/** @brief Determine how much time has passed since a timestamp was retrieved.

    The behavior of invoking this function when timestamps are not initialized
    is undefined.

    @param tsSince  A timestamp acquired earlier via RedOsTimestamp().

    @return The number of microseconds which have passed since @p tsSince.
*/
uint64_t RedOsTimePassed(
    REDTIMESTAMP    tsSince)
{
    REDTIMESTAMP    now;

    now = RedOsTimestamp();

    return (uint64_t)(now - tsSince);
}

