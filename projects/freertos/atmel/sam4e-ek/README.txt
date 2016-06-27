README for the Reliance Edge Atmel SAM4E-EK FreeRTOS Project

**Note** -- This project WILL NOT WORK until you setup the FreeRTOS directory.
            Keep reading for details.

Project
-------

This is an Atmel Studio project for the Atmel SAM4E-EK (an evaluation board
available from Atmel).  It is intended to demonstrate using Reliance Edge with
FreeRTOS on an Atmel target.

Atmel Studio Framework
----------------------

This project includes a complete copy of the Atmel Studio Framework (ASF)
components that it needs.  Minor modifications to the ASF components have been
made.  One set of modifications was to the SD/MMC driver to add support for
multi-sector read and write requests, which greatly improves file system
performance.  Other modifications quiet compiler warnings.

FreeRTOS
--------

This project does not include a copy of the FreeRTOS source.  This must be
supplied by you, the person building the project.

The Atmel Studio project expects to find the FreeRTOS source in a "src/FreeRTOS"
subdirectory in this project.  That is *only* the FreeRTOS source, not the
FreeRTOS-Plus source (thus there should be a "src/FreeRTOS/Source" directory,
and *NOT* "src/FreeRTOS/FreeRTOS/Source").

Creating this directory can be accomplished in two ways:

1) Copy the FreeRTOS tree into src/FreeRTOS;

2) Create a link src/FreeRTOS which points to a FreeRTOS tree located elsewhere.

The latter avoids the burden of copying the FreeRTOS source (which, including
all the demos, is quite large) into the project.

On Windows hosts (assuming Windows Vista or later), you can use "mklink /j
<link> <target>" to create a link in the src/ subdirectory.  "rmdir" will delete
the link without deleting the directory it points at.

If you open the Atmel Studio project without creating the FreeRTOS directory,
it will fail to build due to missing files.  If the directory is created while
the Atmel Studio project is open, it may be necessary to restart Atmel Studio.

Reliance Edge
-----------

See the developer's guide for setting up Reliance Edge.

The os/freertos/services/osbdev.c has code to use the modified ASF SD/MMC
driver, but it might not be enabled (see the macros in that file).

Tested Versions
---------------

This project has been tested with FreeRTOS v8.2-v9.0 and Atmel Studio 6.2.
Modifications may be required for other software versions.

Resources
---------

Atmel Studio is available at: http://www.atmel.com/tools/atmelstudio.aspx

FreeRTOS is available at: http://www.freertos.org/

