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
/** @brief
    @file FreeRTOS block device implementation; see osbdev.c for details.
*/
#ifndef OSBDEV_STM32SDIO_H
#define OSBDEV_STM32SDIO_H


#ifdef USE_STM324xG_EVAL
  #include <stm324xg_eval.h>
  #include <stm324xg_eval_sd.h>
#elif defined(USE_STM32746G_DISCO)
  #include <stm32746g_discovery.h>
  #include <stm32746g_discovery_sd.h>
#else
  /*  If you are using a compatible STM32 device other than the two listed above
      and you have SD card driver headers, you can try adding them to the above
      list.
  */
  #error "Unsupported device."
#endif

#if REDCONF_VOLUME_COUNT > 1
  #error "The STM32 SDIO block device implementation does not support multiple volumes."
#endif

#ifndef USE_HAL_DRIVER
  #error "The STM32 StdPeriph driver is not supported. Please use the HAL driver or modify the Reliance Edge block device interface."
#endif


/** @brief Number of times to call BSP_SD_GetStatus() before timing out and
           returning an error.

    See ::CheckStatus().

    NOTE: We have not observed a scenario where BSP_SD_GetStatus() returns
    SD_TRANSFER_BUSY after a transfer command returns successfully.  Set
    SD_STATUS_TIMEOUT to 0U to skip checking BSP_SD_GetStatus().
*/
#define SD_STATUS_TIMEOUT (100000U)

/** @brief 4-byte aligned buffer to use for DMA transfers when passed in
           an unaligned buffer.
*/
static uint32_t gaulAlignedBuffer[512U / sizeof(uint32_t)];


#if SD_STATUS_TIMEOUT > 0U
static REDSTATUS CheckStatus(void);
#endif


/** @brief Initialize a disk.

    @param bVolNum  The volume number of the volume whose block device is being
                    initialized.
    @param mode     The open mode, indicating the type of access required.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    No SD card was found; or BSP_SD_Init() failed.
    @retval -RED_EINVAL The SD card's block size is not the same as the
                        configured sector size; or the SD card is not large
                        enough for the volume; or the volume size is above
                        4GiB, meaning that part of it cannot be accessed
                        through the STM32 SDIO driver.
*/
static REDSTATUS DiskOpen(
    uint8_t         bVolNum,
    BDEVOPENMODE    mode)
{
    REDSTATUS       ret;
    static bool     fSdInitted = false;

    (void)mode;

    if(!fSdInitted)
    {
        if(BSP_SD_Init() == MSD_OK)
        {
            fSdInitted = true;
        }
    }

    if(!fSdInitted)
    {
        /*  Above initialization attempt failed.
        */
        ret = -RED_EIO;
    }
    else if(BSP_SD_IsDetected() == SD_NOT_PRESENT)
    {
        ret = -RED_EIO;
    }
    else
    {
        ret = 0;
    }

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
    (void)bVolNum;
    return 0;
}


/** @brief Return the disk geometry.

    @param bVolNum  The volume number of the volume whose block device geometry
                    is being queried.
    @param pInfo    On successful return, populated with the geometry of the
                    block device.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EIO    A disk I/O or driver error occurred.
*/
static REDSTATUS DiskGetGeometry(
    uint8_t                 bVolNum,
    BDEVINFO               *pInfo)
{
    HAL_SD_CardInfoTypedef  sdCardInfo = {{0}};

    BSP_SD_GetCardInfo(&sdCardInfo);

    /*  Note: the actual card block size is sdCardInfo.CardBlockSize, but the
        interface only supports a 512 byte block size. Further, one card has
        been observed to report a 1024-byte block size, but it worked fine with
        a 512-byte Reliance Edge ulSectorSize.

        Shifting sdCardInfo.CardCapacity does a unit conversion from bytes to
        512-byte sectors.
    */
    pInfo->ulSectorSize = 512U;
    pInfo->ullSectorCount = sdCardInfo.CardCapacity >> 9U;

    return 0;
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
    REDSTATUS   redStat = 0;
    uint32_t    ulSectorSize = gaRedBdevInfo[bVolNum].ulSectorSize;
    uint8_t     bSdError;

    if(IS_UINT32_ALIGNED_PTR(pBuffer))
    {
        bSdError = BSP_SD_ReadBlocks_DMA(CAST_UINT32_PTR(pBuffer), ullSectorStart * ulSectorSize, ulSectorSize, ulSectorCount);

        if(bSdError != MSD_OK)
        {
            redStat = -RED_EIO;
        }
      #if SD_STATUS_TIMEOUT > 0U
        else
        {
            redStat = CheckStatus();
        }
      #endif
    }
    else
    {
        uint32_t ulSectorIdx;

        for(ulSectorIdx = 0U; ulSectorIdx < ulSectorCount; ulSectorIdx++)
        {
            bSdError = BSP_SD_ReadBlocks_DMA(gaulAlignedBuffer, (ullSectorStart + ulSectorIdx) * ulSectorSize, ulSectorSize, 1U);

            if(bSdError != MSD_OK)
            {
                redStat = -RED_EIO;
            }
          #if SD_STATUS_TIMEOUT > 0U
            else
            {
                redStat = CheckStatus();
            }
          #endif

            if(redStat == 0)
            {
                uint8_t *pbBuffer = CAST_VOID_PTR_TO_UINT8_PTR(pBuffer);

                RedMemCpy(&pbBuffer[ulSectorIdx * ulSectorSize], gaulAlignedBuffer, ulSectorSize);
            }
            else
            {
                break;
            }
        }
    }

    return redStat;
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
    REDSTATUS   redStat = 0;
    uint32_t    ulSectorSize = gaRedBdevInfo[bVolNum].ulSectorSize;
    uint8_t     bSdError;

    if(IS_UINT32_ALIGNED_PTR(pBuffer))
    {
        bSdError = BSP_SD_WriteBlocks_DMA(CAST_UINT32_PTR(CAST_AWAY_CONST(void, pBuffer)), ullSectorStart * ulSectorSize,
                                          ulSectorSize, ulSectorCount);

        if(bSdError != MSD_OK)
        {
            redStat = -RED_EIO;
        }
      #if SD_STATUS_TIMEOUT > 0U
        else
        {
            redStat = CheckStatus();
        }
      #endif
    }
    else
    {
        uint32_t ulSectorIdx;

        for(ulSectorIdx = 0U; ulSectorIdx < ulSectorCount; ulSectorIdx++)
        {
            const uint8_t *pbBuffer = CAST_VOID_PTR_TO_CONST_UINT8_PTR(pBuffer);

            RedMemCpy(gaulAlignedBuffer, &pbBuffer[ulSectorIdx * ulSectorSize], ulSectorSize);

            bSdError = BSP_SD_WriteBlocks_DMA(gaulAlignedBuffer, (ullSectorStart + ulSectorIdx) * ulSectorSize, ulSectorSize, 1U);

            if(bSdError != MSD_OK)
            {
                redStat = -RED_EIO;
            }
          #if SD_STATUS_TIMEOUT > 0U
            else
            {
                redStat = CheckStatus();
            }
          #endif

            if(redStat != 0)
            {
                break;
            }
        }
    }

    return redStat;
}


/** @brief Flush any caches beneath the file system.

    @param bVolNum  The volume number of the volume whose block device is being
                    flushed.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0   Operation was successful.
*/
static REDSTATUS DiskFlush(
    uint8_t bVolNum)
{
    /*  Disk transfer is synchronous; nothing to flush.
    */
    (void)bVolNum;
    return 0;
}


#if SD_STATUS_TIMEOUT > 0U
/** @brief Wait until BSP_SD_GetStatus returns SD_TRANSFER_OK.

    This function calls BSP_SD_GetStatus repeatedly as long as it returns
    SD_TRANSFER_BUSY up to SD_STATUS_TIMEOUT times.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           SD_TRANSFER_OK was returned.
    @retval -RED_EIO    SD_TRANSFER_ERROR received, or timed out waiting for
                        SD_TRANSFER_OK.
*/
static REDSTATUS CheckStatus(void)
{
    REDSTATUS                   redStat = 0;
    uint32_t                    ulTimeout = SD_STATUS_TIMEOUT;
    HAL_SD_TransferStateTypedef transferState;

    do
    {
        transferState = BSP_SD_GetStatus();
        ulTimeout--;
    } while((transferState == SD_TRANSFER_BUSY) && (ulTimeout > 0U));

    if(transferState != SD_TRANSFER_OK)
    {
        redStat = -RED_EIO;
    }

    return redStat;
}
#endif /* SD_STATUS_TIMEOUT > 0U */

#endif /* REDCONF_READ_ONLY == 0 */


#endif /* OSBDEV_STM32SDIO_H */

