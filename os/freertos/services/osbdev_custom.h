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
/** @brief
    @file FreeRTOS block device implementation; see osbdev.c for details.
*/
#ifndef OSBDEV_CUSTOM_H
#define OSBDEV_CUSTOM_H


/*  Hi, there!  You might be seeing this error message and wondering what to do
    about it.  The gist of it is that FreeRTOS does not provide a standard
    interface for communicating with the block device.  You need to fill in
    these functions to tell Reliance Edge how it should be opening, closing,
    reading from, and writing to your block device (whatever that may be: SD,
    MMC, eMMC, CF, USB, etc.).  This is discussed in detail in the Reliance Edge
    Developer's Guide (available at tuxera.com/products/reliance-edge); see the
    _Porting Guide_ chapter, in particular the "Block Device" section, and the
    _FreeRTOS Integration_ chapter.  This directory contains several examples of
    how the block device can be implemented (these examples are described in the
    documentation just referenced, and in comments near the top of osbdev.c).
    These examples may be useful to you as a reference; if you are lucky, one of
    them might even work for you.
*/
#error "FreeRTOS block device not implemented"


/*  If you need to include headers to access your block device, do so here.  For
    example, if you are using an SD card driver whose interfaces are defined in
    sd.h, that would be included here.
#include <foobar.h>
*/


/** @brief Initialize a disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    initialized.
    @param mode     The open mode, indicating the type of access required.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS DiskOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    REDSTATUS       ret;

    /*  Avoid warnings about unused function parameters.
    */
    (void)bVolNum;
    (void)mode;

    /*  Insert code here to open/initialize the block device.
    */
    REDERROR();
    ret = -RED_ENOSYS;

    return ret;
}


/** @brief Uninitialize a disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    uninitialized.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0   Operation was successful.
*/
static REDSTATUS DiskClose(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;

    /*  Avoid warnings about unused function parameters.
    */
    (void)bVolNum;

    /*  Insert code here to close/deinitialize the block device.
    */
    REDERROR();
    ret = -RED_ENOSYS;

    return ret;
}


/** @brief Return the disk geometry.

    @param bVolNum  The volume number of the volume whose block device geometry
                    is being queried.
    @param pInfo    On successful return, populated with the geometry of the
                    block device.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0               Operation was successful.
    @retval -RED_EIO        A disk I/O or driver error occurred.
    @retval -RED_ENOTSUPP   The geometry cannot be queried on this block device.
*/
static REDSTATUS DiskGetGeometry(
    uint8_t     bVolNum,
    BDEVINFO   *pInfo)
{
    REDSTATUS   ret;

    /*  Avoid warnings about unused function parameters.
    */
    (void)bVolNum;
    (void)pInfo;

    /*  Insert code here to read the block device geometry.
    */
    REDERROR();
    ret = -RED_ENOSYS;

    return ret;
}


/** @brief Read sectors from a disk.

    @param bVolNum          The volume number of the volume whose block device
                            is being read from.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to read.
    @param pBuffer          The buffer into which to read the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS DiskRead(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    void       *pBuffer)
{
    REDSTATUS   ret;

    /*  Avoid warnings about unused function parameters.
    */
    (void)bVolNum;
    (void)ullSectorStart;
    (void)ulSectorCount;
    (void)pBuffer;

    /*  Insert code here to read sectors from the block device.
    */
    REDERROR();
    ret = -RED_ENOSYS;

    return ret;
}


#if REDCONF_READ_ONLY == 0

/** @brief Write sectors to a disk.

    @param bVolNum          The volume number of the volume whose block device
                            is being written to.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to write.
    @param pBuffer          The buffer from which to write the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS DiskWrite(
    uint8_t     bVolNum,
    uint64_t    ullSectorStart,
    uint32_t    ulSectorCount,
    const void *pBuffer)
{
    REDSTATUS   ret;

    /*  Avoid warnings about unused function parameters.
    */
    (void)bVolNum;
    (void)ullSectorStart;
    (void)ulSectorCount;
    (void)pBuffer;

    /*  Insert code here to write sectors to the block device.
    */
    REDERROR();
    ret = -RED_ENOSYS;

    return ret;
}


/** @brief Flush any caches beneath the file system.

    @param bVolNum  The volume number of the volume whose block device is being
                    flushed.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O error occurred.
*/
static REDSTATUS DiskFlush(
    uint8_t     bVolNum)
{
    REDSTATUS   ret;

    /*  Avoid warnings about unused function parameters.
    */
    (void)bVolNum;

    /*  Insert code here to flush the block device.  If writing to the block
        device is inherently synchronous (no hardware or software cache), then
        this can do nothing and return success.
    */
    REDERROR();
    ret = -RED_ENOSYS;

    return ret;
}

#endif /* REDCONF_READ_ONLY == 0 */


#endif /* OSBDEV_CUSTOM_H */
