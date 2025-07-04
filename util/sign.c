/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2025 Tuxera US Inc.
                      All Rights Reserved Worldwide.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; use version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but "AS-IS," WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, see <https://www.gnu.org/licenses/>.
*/
/*  Businesses and individuals that for commercial or other reasons cannot
    comply with the terms of the GPLv2 license must obtain a commercial
    license before incorporating Reliance Edge into proprietary software
    for distribution in any form.

    Visit https://www.tuxera.com/products/tuxera-edge-fs/ for more information.
*/
/** @file
    @brief Implements a sign on message.
*/
#include <redfs.h>


/** @brief Display the Reliance Edge signon message.
*/
void RedSignOn(void)
{
  #if REDCONF_OUTPUT == 1

    /*  Use RedOsOutputString() instead of RedPrintf() to reduce code size.
    */
    RedOsOutputString(RED_PRODUCT_NAME "\n");
    RedOsOutputString(RED_PRODUCT_EDITION "\n");
    RedOsOutputString(RED_PRODUCT_LEGAL "\n");
    RedOsOutputString(RED_PRODUCT_PATENT "\n");

  #else

    /*  Always embed the copyright into the program data.  Use "volatile" to try
        to avoid the compiler removing the variables.
    */
    static volatile const char szVersion[] = RED_PRODUCT_NAME;
    static volatile const char szEdition[] = RED_PRODUCT_EDITION;
    static volatile const char szCopyright[] = RED_PRODUCT_LEGAL;
    static volatile const char szPatent[] = RED_PRODUCT_PATENT;

    (void)szVersion;
    (void)szEdition;
    (void)szCopyright;
    (void)szPatent;

  #endif
}
