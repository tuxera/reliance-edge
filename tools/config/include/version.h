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
#ifndef VERSION_H
#define VERSION_H

#define CONFIG_VERSION      "3.0"

// Hex representation of version above. Most significant byte is the
// major version, etc.  The low byte should be left 00.  This is exported
// with the configuration to allow Reliance Edge to print a more helpful
// message if the configuration is too old.
#define CONFIG_VERSION_VAL  0x03000000U

#endif // VERSION_H
