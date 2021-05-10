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
/*  @file
    @brief Wrapper implementation of the C standard memory allocation functions
           over FreeRTOS' pvPortMalloc and vPortFree.
*/

#include <malloc.h>

#include <FreeRTOS.h>

#include <redfs.h>

/*  This is a wrapper to pvPortMalloc.  It allocates the requested amount
    of space plus enough to store the size of the request (to enable realloc).
*/
void *malloc(size_t size)
{
    void *ptr;

    if(size + sizeof(size_t) < size)
    {
        return NULL;
    }

    ptr = pvPortMalloc(size + sizeof(size_t));

    if(ptr == NULL)
    {
        return NULL;
    }

    *((size_t *) ptr) = size;

    return ptr + sizeof(size_t);
}


void free(void *ptr)
{
    if(ptr != NULL)
    {
        vPortFree(ptr - sizeof(size_t));
    }
}


void *realloc(void *ptr, size_t size)
{
    void *newptr;
    size_t oldsize;

    if(ptr == NULL)
    {
        return malloc(size);
    }

    oldsize = *((size_t *) (ptr - sizeof(size_t)));

    if(size == 0)
    {
        newptr = NULL;
    }
    else
    {
        newptr = malloc(size);
        RedMemCpy(newptr, ptr, REDMIN(size, oldsize));
    }

    free(ptr);

    return newptr;
}


void *calloc(size_t size1, size_t size2)
{
    size_t totalSize = size1 * size2;
    void *ptr;

    if(totalSize < size1 || totalSize < size2)
    {
        return NULL;
    }

    ptr = malloc(totalSize);

    if(ptr == NULL)
    {
        return NULL;
    }

    RedMemSet(ptr, 0U, totalSize);

    return ptr;
}
