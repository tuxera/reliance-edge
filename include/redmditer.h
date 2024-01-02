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
    @brief Interfaces for the Reliance Edge metadata iteration utility.
*/
#ifndef REDMDITER_H
#define REDMDITER_H


/** @brief Reliance Edge metadata type.
*/
typedef enum
{
    MDTYPE_MASTER,
    MDTYPE_METAROOT,
    MDTYPE_IMAP,
    MDTYPE_INODE,
    MDTYPE_DINDIR,
    MDTYPE_INDIR,
    MDTYPE_DIRECTORY,
    MDTYPE_COUNT /* Count of types */
} MDTYPE;

/** @brief Type for the metadata iteration callback function.

    @param pContext Caller-supplied context pointer.
    @param type     The metadata node type.
    @param ulBlock  Logical block number where the metadata node is located.
    @param pBuffer  Aligned buffer populated with the metadata node contents.

    @return A negated ::REDSTATUS code indicating the operation result.  Any
            nonzero return value will abort the iteration.
*/
typedef REDSTATUS MDITERCB(void *pContext, MDTYPE type, uint32_t ulBlock, void *pBuffer);

/** @brief Parameters for the metadata iteration utility.
*/
typedef struct
{
    uint8_t     bVolNum;        /**< Volume number of volume to iterate. */
    const char *pszDevice;      /**< Device string (optional: can be NULL). */
    MDITERCB   *pfnCallback;    /**< Callback invoked for each metadata node. */
    void       *pContext;       /**< Context pointer passed along as a pfnCallback parameter. */
    bool        fVerify;        /**< Verify each metadata node (signature, CRC, etc.). */
} MDITERPARAM;


REDSTATUS RedMetadataIterate(const MDITERPARAM *pParam);


#endif /* REDMDITER_H */
