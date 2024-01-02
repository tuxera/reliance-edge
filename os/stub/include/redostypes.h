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
    @brief Defines OS-specific types for use in common code.
*/
#ifndef REDOSTYPES_H
#define REDOSTYPES_H


/** @brief Implementation-defined timestamp type.

    This can be an integer, a structure, or a pointer: anything that is
    convenient for the implementation.  Since the underlying type is not fixed,
    common code should treat this as an opaque type.
*/
typedef uint64_t REDTIMESTAMP;

/** @brief Implementation-defined block device context type.

    The underlying type of the context parameter passed into RedOsBDevConfig().
    This can be anything that is convenient for the implementation: a void
    pointer, a pointer to an opaque structure, a string (`const char *`), an
    integer, etc.

    Common code should treat this as an opaque type.  OS-specific code may
    assume the OS-specific underlying type.  In code for the host tools,
    compiled with the host operating systems (e.g., Windows or Linux), this is
    assumed to be a string (`const char *`).
*/
typedef void *REDBDEVCTX;


#endif

