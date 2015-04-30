/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                   Copyright (c) 2014-2015 Datalight, Inc.
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
    comply with the terms of the GPLv2 license may obtain a commercial license
    before incorporating Reliance Edge into proprietary software for
    distribution in any form.  Visit http://www.datalight.com/reliance-edge for
    more information.
*/
/** @file
    @brief Prototypes for Reliance Edge test entry points.
*/
#ifndef REDTESTS_H
#define REDTESTS_H

#include <redtypes.h>
#include "redtestutils.h"
#include "redver.h"

/*  This macro is only defined by the error injection project.
*/
#ifdef REDCONF_ERROR_INJECTION
#include <rederrinject.h>
#endif

#define FSSTRESS_SUPPORTED  \
    (    ((RED_KIT == RED_KIT_GPL) || (RED_KIT == RED_KIT_SANDBOX)) \
      && (REDCONF_API_POSIX == 1) && (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_UNLINK == 1) \
      && (REDCONF_API_POSIX_MKDIR == 1) && (REDCONF_API_POSIX_RMDIR == 1) && (REDCONF_API_POSIX_RENAME == 1) \
      && (REDCONF_API_POSIX_LINK == 1) && (REDCONF_API_POSIX_FTRUNCATE == 1) && (REDCONF_API_POSIX_READDIR == 1))

#define FSE_STRESS_TEST_SUPPORTED \
    (    ((RED_KIT == RED_KIT_COMMERCIAL) || (RED_KIT == RED_KIT_SANDBOX)) \
      && (REDCONF_API_FSE == 1) && (REDCONF_READ_ONLY == 0))

#define POSIX_API_TEST_SUPPORTED \
    (    ((RED_KIT == RED_KIT_COMMERCIAL) || (RED_KIT == RED_KIT_SANDBOX)) \
      && (REDCONF_API_POSIX == 1) && (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FORMAT == 1))

#define STOCH_POSIX_TEST_SUPPORTED \
    (    ((RED_KIT == RED_KIT_COMMERCIAL) || (RED_KIT == RED_KIT_SANDBOX)) \
      && (REDCONF_API_POSIX == 1) && (REDCONF_READ_ONLY == 0) && (REDCONF_API_POSIX_FORMAT == 1) \
      && (REDCONF_API_POSIX_READDIR == 1) && (REDCONF_API_POSIX_MKDIR == 1) && (REDCONF_API_POSIX_RMDIR == 1) \
      && (REDCONF_API_POSIX_UNLINK == 1))


#if FSSTRESS_SUPPORTED
int fsstress_main(int argc, char **argv);
#endif

#if STOCH_POSIX_TEST_SUPPORTED
int RedStochPosix(int argc, char ** argv);
#endif

#if FSE_STRESS_TEST_SUPPORTED
void RedFseStressTest(void);
#endif

#if POSIX_API_TEST_SUPPORTED
int RedTestPosixMain(int argc, char * argv[]);
#endif


#endif

