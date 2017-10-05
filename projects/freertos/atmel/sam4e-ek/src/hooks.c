/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                   Copyright (c) 2014-2017 Datalight, Inc.
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
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

#include <redfs.h>


/** @brief Handler for Atmel SAM4E-EK hard faults.

    The hard fault handler has been observed to be invoked when attempts are
    made to dynamically allocate a large amount of memory.  The default handler
    simply loops forever, resulting in a silent hang.  Overriding the default
    handler at least makes it obvious that something has gone wrong.
*/
void HardFault_Handler(void)
{
    /*  Produce output even if asserts are disabled.
    */
    fprintf(stderr, "Hard fault handler invoked!\n\r");

    REDERROR();

    /*  Don't return.  After a hard fault, things are possibly in a bad state.
        Even when we get here from malloc(), if we return from here malloc()
        returns a bogus pointer instead of a NULL pointer, so error recovery is
        not possible.
    */
    while(true)
    {
    }
}


/** @brief Handler for asserts firing from the FreeRTOS code.
*/
void vAssertCalled(
    uint32_t    ulLine,
    const char *pcFile)
{
    /*  The following two variables are just to ensure the parameters are not
        optimized away and therefore unavailable when viewed in the debugger.
    */
    volatile uint32_t ulLineNumber = ulLine, ulSetNonZeroInDebuggerToReturn = 0;
    volatile const char * const pcFileName = pcFile;

  #if REDCONF_ASSERTS == 1
    /*  Invoke Reliance Edge assertion handler if available.
    */
    RedOsAssertFail(pcFile, ulLine);
  #endif

    taskENTER_CRITICAL();
    while( ulSetNonZeroInDebuggerToReturn == 0 )
    {
        /*  If you want to get out of this function in the debugger to see the
            assert() location then set ulSetNonZeroInDebuggerToReturn to a
            non-zero value.
        */
    }
    taskEXIT_CRITICAL();

    (void)pcFileName;
    (void)ulLineNumber;
}


#if configCHECK_FOR_STACK_OVERFLOW > 0
/*  Prototype to quiet a warning.
*/
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName);


/** @brief Handler for stack overflows.
*/
void vApplicationStackOverflowHook(
    TaskHandle_t    pxTask,
    char           *pcTaskName)
{
    (void)pcTaskName;
    (void)pxTask;

    /*  Run time stack overflow checking is performed if
        configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
        function is called if a stack overflow is detected.
    */
    vAssertCalled(__LINE__, __FILE__);
}
#endif
