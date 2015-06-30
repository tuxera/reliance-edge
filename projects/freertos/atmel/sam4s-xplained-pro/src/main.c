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
/*  This example project implements an example program which logs input levels
    to a Reliance Edge volume using the File System Essentials interface.
    
    See the included README.txt file for more information.
*/


/*  Include header files for all drivers that have been imported from
    Atmel Software Framework (ASF).

    Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
*/
#include <asf.h>

#include <FreeRTOS.h>
#include <task.h>

#include <redfs.h>
#include <redfse.h>
#include <redcoreapi.h>


#define VOLUME_INDEX (0U)

/*  2UL = first allocable inode in the File System Essentials configuration.
*/
#define LOGFILE_INDEX   (2UL)

/*  Use these to determine which sensors are read and logged in this example.

    Note that the temperature and light sensors are part of the IO1 Xplained
    Pro expansion board, and thus should not be logged if using the SAM4S
    Xplained Pro board without that expansion. The IO1 expansion board should
    be connected to expansion header 2 (EXT2) to be used with this project.
*/
#define LOG_BUTTON      1
#define LOG_TEMPERATURE 0
#define LOG_LIGHT       0


static void vRedLogTask(void *pParam);
static void vRedTransactTask(void *pParam);
static REDSTATUS InitRedfs(void);
#if LOG_TEMPERATURE
static void TempToString(double temp, char *buffer);
#endif
static void UlToString(uint32_t ulInt, char *buffer);
static REDSTATUS WriteEntry(char *logEntry);


/*  This value is set by the log task once the file system is initialized and
    the volume is mounted. Other tasks should wait for this to become true.
*/
static volatile bool fMounted = false;


int main (void)
{
    TaskHandle_t vTask;

    const usart_serial_options_t usart_serial_options =
    {
        .baudrate   = CONF_BAUDRATE,
        .charlength = CONF_CHARLENGTH,
        .paritytype = CONF_PARITY,
        .stopbits   = CONF_STOPBITS,
    };

    irq_initialize_vectors();
    cpu_irq_enable();

    sysclk_init();
	board_init();
	stdio_serial_init(CONF_USART, &usart_serial_options);
    sd_mmc_init();
    ioport_init();

  #if LOG_TEMPERATURE
    at30tse_init();
  #endif
  #if LOG_LIGHT
	/* Configure ADC pin for light sensor. */
	gpio_configure_pin(LIGHT_SENSOR_GPIO, LIGHT_SENSOR_FLAGS);

	/* Enable ADC clock. */
	pmc_enable_periph_clk(ID_ADC);

	/* Configure ADC. */
	adc_init(ADC, sysclk_get_cpu_hz(), 1000000, ADC_MR_STARTUP_SUT0);
	adc_enable_channel(ADC, ADC_CHANNEL_4)  ;
	adc_configure_trigger(ADC, ADC_TRIG_SW, 1);
  #endif

    if(xTaskCreate(vRedLogTask, "REDLOGTASK", (1024U * 3U) / sizeof(StackType_t), NULL, tskIDLE_PRIORITY + 1U, &vTask) != pdPASS)
    {
        fprintf(stderr, "Failed to create Reliance Edge log example task\n\r");
        return 1;
    }

    if(xTaskCreate(vRedTransactTask, "REDTRANSACT", (1024U * 3U) / sizeof(StackType_t), NULL, tskIDLE_PRIORITY + 1U, &vTask) != pdPASS)
    {
        fprintf(stderr, "Failed to create Reliance Edge transact task\n\r");
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


/*  Task to log sensory information.
    
    This task reads information from sensors periodically and writes it to the
    disk. Each set of samples is written in one line, prefaced by an index
    number. The user LED is also toggled on the board each time a sample is
    written.
    
    This is a FreeRTOS task, and should never exit.
*/
static void vRedLogTask(
    void   *pParam)
{
    REDSTATUS   rstat = 0;

    /*  We don't use the parameter.
    */
    REDASSERT(pParam == NULL);
    (void)pParam;

    printf("\n\rBeginning log task.\n\r");

    rstat = InitRedfs();
    if(rstat == 0)
    {
        fMounted = true;
    }

    /*  Take periodic samples of a button and/or other sensors and write them
        to the disk.
    */
    while(rstat == 0)
    {
        char            logEntry[40];
      #if LOG_BUTTON
        bool            fButtonUp;
      #endif
      #if LOG_TEMPERATURE
        double          temperature;
      #endif
      #if LOG_LIGHT
        uint32_t        ulAdcValue;
      #endif
      
        static uint32_t ulCounter = 0;

        UlToString(ulCounter, logEntry);

      #if LOG_BUTTON
        fButtonUp = ioport_get_pin_level(GPIO_PUSH_BUTTON_0);
        RedStrNCpy(&(logEntry[strlen(logEntry)]), (fButtonUp ? "\tfalse" : "\ttrue"), 8);
      #endif
      
      #if LOG_TEMPERATURE
        if(at30tse_read_temperature(&temperature) == TWI_SUCCESS)
        {
            RedStrNCpy(&(logEntry[strlen(logEntry)]), "\t", 2);
            TempToString(temperature, &(logEntry[strlen(logEntry)]));
        }
      #endif

      #if LOG_LIGHT
        /*  Get light sensor information.
        */
        adc_start(ADC);
        ulAdcValue = adc_get_channel_value(ADC, ADC_CHANNEL_4);

        RedStrNCpy(&(logEntry[strlen(logEntry)]), "\t", 2);
        UlToString(ulAdcValue, &(logEntry[strlen(logEntry)]));
      #endif
      
        RedStrNCpy(&(logEntry[strlen(logEntry)]), "\n", 2);
        
        printf("%s\r", logEntry);

        rstat = WriteEntry(logEntry);
        if(rstat == 0)
        {
            /*  Toggle an LED just to show that we're working.
            */
            ioport_toggle_pin_level(LED0_GPIO);
            
            ulCounter++;

            /*  Wait in milliseconds before recording another sample.
            */
            vTaskDelay(200 / portTICK_RATE_MS);
        }
    }

    printf("Reliance Edge log example ended.\n\r");
    
    while(true)
    {
        /*  Allow other tasks to execute
        */
        vTaskDelay(5000 / portTICK_RATE_MS);
    }
}


/*  Task to perform periodic file system transactions.
            
    In order to ensure the disk does get transacted, make sure other tasks do
    not indefinitely block the processor (e.g. call vTaskDelay routinely).
*/
static void vRedTransactTask(void *pParam)
{
    REDSTATUS rstat = 0;

    /*  We don't use the parameter.
    */
    REDASSERT(pParam == NULL);
    (void)pParam;
    

    /*  Wait for another task to mount the volume.
    */
    while (!fMounted)
    {
        vTaskDelay(100 / portTICK_RATE_MS);
    }

    while(true)
    {
        /*  Allow other tasks to execute; transact every second.
        */
        vTaskDelay(1000 / portTICK_RATE_MS);

        if(rstat == 0)
        {
            rstat = RedFseTransact(VOLUME_INDEX);
            if(rstat != 0)
            {
                printf("Error %lu transacting volume %u.\r\n", -rstat, VOLUME_INDEX);
            }
        }
    }
}


/*  Initializes the Reliance Edge driver and mounts the volume. A negative
    return status implies that the volume was not mounted and the file system
    is left uninitialized.
*/
REDSTATUS InitRedfs()
{
    REDSTATUS rstat = RedFseInit();

    if(rstat != 0)
    {
        REDERROR();
        printf("Unexpected error number %ld returned from RedFseInit.\n\r", -rstat);
    }
    else
    {
        rstat = RedFseMount(VOLUME_INDEX);
        if(rstat != 0)
        {
            REDASSERT(rstat == -RED_EIO);
            printf("Failed to mount volume %u. Ensure the SD card is inserted and formatted for the\r\ncurrent Reliance Edge configuration.\n\r", VOLUME_INDEX);

            (void) RedFseUninit();
        }
    }

    return rstat;
}


#if LOG_TEMPERATURE
/*  Take a double and convert it to a string, rounding to the first decimal
    place and appending "oC" for degrees in Celsius.
*/
static void TempToString(
    double      temp,
    char       *buffer)
{
    uint32_t    ulTemp;
    double      decimal;
    uint32_t    ulDecimal;

    /*  A valid temperature will
    */
    if((temp >= (double) INT32_MAX) || (temp <= (double) INT32_MIN))
    {
        strcpy(buffer, "err");
        return;
    }

    /*  Discard the sign; we'll have to print it manually.
    */
    ulTemp = (temp >= 0) ? ((int32_t) temp) : ((int32_t) (temp * -1));

    /*  Extract the first decimal place. (Note: using UlToString would not work
        with more than one decimal place, since leading zeros are discarded.)
    */
    decimal = (temp >= 0) ? ((temp - (double) ulTemp) * 10) : ((temp + (double) ulTemp) * 10);
    ulDecimal = (decimal >= 0) ? ((int32_t) decimal) : ((int32_t) (decimal * -1));

    if((temp < 0) && (ulTemp != 0) && (ulDecimal != 0))
    {
        RedStrNCpy(buffer, "-", 2);
    }

    UlToString(ulTemp, &buffer[strlen(buffer)]);
    RedStrNCpy(&buffer[strlen(buffer)], ".", 2);
    UlToString(ulDecimal, &buffer[strlen(buffer)]);
    RedStrNCpy(&buffer[strlen(buffer)], "oC", 3);
}
#endif /* LOG_TEMPERATURE */


/*  Converts an integer to a string of characters and stores it in the given
    buffer.
*/
static void UlToString(
    uint32_t    ulInt,
    char       *buffer)
{
    const char *digits = "0123456789";
    int         place = 0;
    uint32_t    digit = ulInt;

    REDASSERT(buffer != NULL);

    do
    {
        digit /= 10;
        place++;
    } while(digit > 0);

    digit = ulInt;
    buffer[place] = '\0';

    while(place > 0)
    {
        buffer[place - 1] = digits[digit % 10];
        place--;
        digit /= 10;
    }
}


/*  Appends data to file number FILE_INDEX.
*/
static REDSTATUS WriteEntry(
    char       *logEntry)
{
    REDSTATUS   rstat = 0;
    int64_t     fileSize;

    fileSize = RedFseSizeGet(VOLUME_INDEX, LOGFILE_INDEX);
    if(fileSize < 0)
    {
        printf("Unexpected error %ld returned from RedFseSizeGet.\n\r", (int32_t) -fileSize);
        rstat = (REDSTATUS) fileSize;
    }
    else
    {
        int32_t     written;
        int32_t     writeLength = strlen(logEntry);
            
        written = RedFseWrite(VOLUME_INDEX, LOGFILE_INDEX, fileSize, writeLength, (void *) logEntry);
        if(written < 0)
        {
            if((written == -RED_EFBIG) || (written == -RED_ENOSPC))
            {
                printf("Error: out of room on disk or in file.\n\r");
            }
            printf("Unexpected error %ld returned from RedFseWrite.\n\r", -written);
            rstat = written;
        }
        else if(written != writeLength)
        {
                printf("Unexpected value returned from RedFseWrite: %ld.\n\r", -written);
        }
    }

    return rstat;
}
