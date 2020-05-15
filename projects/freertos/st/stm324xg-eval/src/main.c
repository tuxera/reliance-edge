/** @file
    @brief This project runs Reliance Edge on top of FreeRTOS.  The example task
           runs fsstress and Tuxera's POSIX API test suite if the configuration
           allows.

    Portions of this file are copyright (c) STMicroelectronics, to be used only
    in accordance with the notice below:
*/
/**
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

#include <redfs.h>
#include <redposix.h>
#include <redfse.h>
#include <redconf.h>

#include <stm32f4xx_hal.h>
#include <stm324xg_eval.h>
#include <stm324xg_eval_sd.h>
#include <stm324xg_eval_sram.h>
#include <stm324xg_eval_io.h>

#include "lcd_log.h"
#include "lcd_log_conf.h"

static bool prvSetupHardware(void);
static void SystemClock_Config(void);
static void prvRedExampleTask(void *pParam);
static void prvLcdScrollTask(void *pParam);


static volatile uint8_t initted = 0U;


int main(void)
{
    if(xTaskCreate( prvRedExampleTask,
                    "FILESYSTEM",
                    (1024U * 3U) / sizeof(StackType_t),
                    NULL,
                    tskIDLE_PRIORITY + 1U,
                    NULL)
        != pdPASS)
    {
        return 1;
    }

    if(xTaskCreate( prvLcdScrollTask,
                    "LCDSCROLL",
                    (1024U) / sizeof(StackType_t),
                    NULL,
                    tskIDLE_PRIORITY + 1U,
                    NULL)
        != pdPASS)
    {
        return 1;
    }

    /*  Start the FreeRTOS task scheduler.
    */
    vTaskStartScheduler();

    /*  vTaskStartScheduler() never returns unless there was not enough RAM to
        start the scheduler.
    */
    REDERROR();
    for( ;; );
}


/** @brief Initialize hardware and drivers as needed.
*/
static bool prvSetupHardware( void )
{
    if(HAL_Init() != HAL_OK)
    {
        return false;
    }

    /*  System clock configuration
    */
    SystemClock_Config();

    /*  LCD initialization.
    */

    BSP_LCD_Init();
    LCD_LOG_Init();
    LCD_LOG_SetHeader((uint8_t *) "Reliance Edge Example");

    /*  Joystick initialization.  Tuxera has observed several times where
        BSP_JOY_Init() fails every time it is called until the board is
        physically powered down.
    */
    if(BSP_JOY_Init(JOY_MODE_GPIO) != IO_OK)
    {
        RedOsOutputString("Failed to init joystick control. Try disconnecting power and restarting.\n");
        return false;
    }

    return true;
}


/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 168000000
  *            HCLK(Hz)                       = 168000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 25
  *            PLL_N                          = 336
  *            PLL_P                          = 2
  *            PLL_Q                          = 7
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 5
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    /*  Enable Power Control clock
    */
    __HAL_RCC_PWR_CLK_ENABLE();

    /*  The voltage scaling allows optimizing the power consumption when the device is
        clocked below the maximum system frequency, to update the voltage scaling value
        regarding system frequency refer to product datasheet.
    */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /*  Enable HSE Oscillator and activate PLL with HSE as source
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /*  Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
        clocks dividers
    */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

    /*  STM32F405x/407x/415x/417x Revision Z devices: prefetch is supported
    */
    if (HAL_GetREVID() == 0x1001)
    {
        /*  Enable the Flash prefetch
        */
        __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    }
}


/** @brief Reliance Edge example task.
*/
static void prvRedExampleTask(void *pParam)
{
    /*  We don't use the parameter.
    */
    (void)pParam;

    if(!prvSetupHardware())
    {
        REDERROR();
        while(true)
        {
        }
    }

    initted = 1U;

    RedOsOutputString("\nReliance Edge example task started...\n");

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

    RedOsOutputString("Reliance Edge example task complete.\n");

    /*  FreeRTOS tasks must never return.
    */
    while(true)
    {
    }
}


/** @brief LCD scrolling task

    Enables use of the Joystick control to scroll up and down in the log
    output.
*/
static void prvLcdScrollTask(void *pParam)
{
    (void) pParam;

    while(initted == 0)
    {
        vTaskDelay(100);
    }

    while(true)
    {
        JOYState_TypeDef joyState;

        joyState = BSP_JOY_GetState();

        if(joyState == JOY_DOWN)
        {
            LCD_LOG_ScrollForward();
        }
        else if(joyState == JOY_UP)
        {
            LCD_LOG_ScrollBack();
        }

        vTaskDelay(10);
    }
}
