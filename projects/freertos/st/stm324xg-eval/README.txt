README for the Reliance Edge STM324xG-EVAL project

Overview
========

This is an Atollic TrueSTUDIO project for the ST Microelectonics STM324xG-EVAL
board.  It is intended to demonstrate using Reliance Edge with FreeRTOS on an
STM324xG-EVAL platform.

The program does nothing more than formatting the SD card and mounting it as a
Reliance Edge volume.  A separate task monitors the joystick control and scrolls
outputted text up or down in response.  This feature is not currently useable
because the example program does not print more text than fits on the screen,
but it is included for the developer's convenience should you wish to run an
actual test instead of the example code.

This project includes a customized osoutput.c file instead of
os/freertos/services/osoutput.c.  This is because putchar() did not work as
expected using the Atollic GCC toolchain; the customized file calls
__io_putchar() directly instead. 


Project Setup
=============

STM32Cube
---------

This project does not ship with the drivers, board support package, or
linker scripts for the target.  These source files are all part of the
STM32CubeF4 package.  Follow these steps to allow the project to find the
resources needed:

  - Download STM32CubeF4: http://www.st.com/web/en/catalog/tools/PF259243
  - Unzip the STM32CubeF4 package to a desirable location; e.g.
    C:/ST/STM32Cube_FW_F4.
  - In this project's properties page (in Atollic TrueStudio), select
    Resource -> Linked Resources -> Path Variables tab. Change the
    STM32CubeFWPath to point to the STM32CubeF4 package; e.g. C:/ST/STM32Cube_FW_F4.
  - In the properties page, select C/C++ Build -> Build Variables, and again set
    STM32CubeFWPath to the correct path.

The STM32CubeF4 package also contains a copy of the FreeRTOS source code, which
is used by this project.

This project has been tested to work with versions 1.9.0 and 1.11.0 of
STM32CubeF4. 

Reliance Edge Block Device
--------------------------

Reliance Edge comes with several reference block device interfaces for use on
FreeRTOS.  Before building this project, make sure the correct implementation
is selected.  Open the file Reliance Edge/osbdev.c (os/freertos/services/osbdev.c)
and ensure the macro BDEV_EXAMPLE_IMPLEMENTATION is defined to BDEV_STM32_SDIO.

Tested Versions
---------------

This project has been tested with Atollic TrueStudio versions 5.3 and 5.4.
Both the Pro and the Lite versions may be used.

Resources
---------

Atollic TrueStudio is available at http://timor.atollic.com/truestudio/

The Lite version is free, even though it is a fully functional IDE.  The
Pro version may be used for a monthly fee.
