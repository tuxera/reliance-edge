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
    @brief Error values for Reliance Edge APIs
*/
#ifndef REDERRNO_H
#define REDERRNO_H


/** @brief Return type for Reliance Edge error values.
*/
typedef int32_t REDSTATUS;


/*  The errno numbers are the same as Linux.
*/

/** Operation not permitted. */
#define RED_EPERM            1

/** No such file or directory. */
#define RED_ENOENT           2

/** I/O error. */
#define RED_EIO              5

/** Bad file number. */
#define RED_EBADF            9

/** Out of memory */
#define RED_ENOMEM          12

/** Permission denied. */
#define RED_EACCES          13

/** Device or resource busy. */
#define RED_EBUSY           16

/** File exists. */
#define RED_EEXIST          17

/** Cross-device link. */
#define RED_EXDEV           18

/** Not a directory. */
#define RED_ENOTDIR         20

/** Is a directory. */
#define RED_EISDIR          21

/** Invalid argument. */
#define RED_EINVAL          22

/** File table overflow. */
#define RED_ENFILE          23

/** Too many open files. */
#define RED_EMFILE          24

/** File too large. */
#define RED_EFBIG           27

/** No space left on device. */
#define RED_ENOSPC          28

/** Read-only file system. */
#define RED_EROFS           30

/** Too many links. */
#define RED_EMLINK          31

/** Math result not representable. */
#define RED_ERANGE          34

/** File name too long. */
#define RED_ENAMETOOLONG    36

/** Function not implemented. */
#define RED_ENOSYS          38

/** Directory not empty. */
#define RED_ENOTEMPTY       39

/** Too many symbolic links encountered. */
#define RED_ELOOP           40

/** No data available. */
#define RED_ENODATA         61

/** Link has been severed. */
#define RED_ENOLINK         67

/** Too many users. */
#define RED_EUSERS          87

/** Operation is not supported. */
#define RED_ENOTSUPP        524

/** Nothing will be okay ever again. */
#define RED_EFUBAR          RED_EINVAL


#endif
