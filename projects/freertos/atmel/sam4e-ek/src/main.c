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
    @brief Example application entry point for using Reliance Edge.
*/
#include <asf.h>
#include <stdio_serial.h>
#include "conf_board.h"
#include "conf_clock.h"
#include "conf_example.h"
#include "sd_mmc_protocol.h"
#include "memtest.h"

#include <FreeRTOS.h>
#include <task.h>

#include <redposix.h>
#include <redfse.h>


#define RUN_ATMEL_MEMTEST   0


static void vRedTestTask(void *pParam);


/**
 * \brief Application entry point.
 *
 * \return Unused (ANSI-C compatibility).
 */
int main(void)
{
    TaskHandle_t vTask;

    const usart_serial_options_t usart_serial_options =
    {
        .baudrate   = CONF_TEST_BAUDRATE,
        .charlength = CONF_TEST_CHARLENGTH,
        .paritytype = CONF_TEST_PARITY,
        .stopbits   = CONF_TEST_STOPBITS,
    };

    irq_initialize_vectors();
    cpu_irq_enable();

    sysclk_init();
    board_init();
    stdio_serial_init(CONF_TEST_USART, &usart_serial_options);

    // Initialize SD MMC stack
    sd_mmc_init();

    if(xTaskCreate(vRedTestTask, "REDTEST", (1024U * 3U) / sizeof(StackType_t), NULL, tskIDLE_PRIORITY + 1U, &vTask) != pdPASS)
    {
        fprintf(stderr, "Failed to create Reliance Edge test task\n\r");
        return 1;
    }

    /*  Start the FreeRTOS task scheduler.
    */
    vTaskStartScheduler();

    /*  vTaskStartScheduler() never returns unless there was not enough RAM to
        start the scheduler.
    */
    fprintf(stderr, "Failed to start FreeRTOS task scheduler: insufficient RAM\n\r");
    return 1;
}


/** @brief FreeRTOS task which runs Reliance Edge file system tests.

    @param pParam   Unused, for prototype compatibility with TaskFunction_t.
*/
static void vRedTestTask(
    void   *pParam)
{
    /*  We don't use the parameter.
    */
    (void)pParam;

    printf("\n\rReliance Edge example task started...\n\r");

  #if RUN_ATMEL_MEMTEST == 1
    AtmelMemTest();
  #endif

  #if REDCONF_API_POSIX == 1
    {
        int32_t ret;
        int32_t ret2;

        ret = red_init();
        if(ret == 0)
        {
          #if REDCONF_API_POSIX_FORMAT == 1
            ret = red_format("");
            if(ret == 0)
          #endif
            {
                ret = red_mount("");
                if(ret == 0)
                {
                    /*  Add test code here.
                    */
                    ;;

                    ret2 = red_umount("");
                    if(ret == 0)
                    {
                        ret = ret2;
                    }
                }
            }

            (void)red_uninit();
        }
    }
  #elif REDCONF_API_FSE == 1
    {
        REDSTATUS   ret;
        REDSTATUS   ret2;

        ret = RedFseInit();
        if(ret == 0)
        {
            ret = RedFseMount(0U);
            if(ret == 0)
            {
                /*  Add test code here.
                */
                ;;

                ret2 = RedFseUnmount(0U);
                if(ret == 0)
                {
                    ret = ret2;
                }
            }

            (void)RedFseUninit();
        }
    }
  #endif

    printf("Reliance Edge example task complete.\n\r");

    /*  FreeRTOS tasks must never return.
    */
    while(true)
    {
    }
}

