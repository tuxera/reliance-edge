# Reliance Edge Quick-Start Guide

_Note: Please read the supplied [README.md](../README.md) file before using
Reliance Edge._

This guide leads you step-by-step through the process of compiling and running
Reliance Edge and its accompanying tools on a Windows system.  Although Reliance
Edge is not designed to be a file system for the Windows platform, following
this guide can help you learn how to use the product.  The testing and host
tools that are compiled are also useful with any port of Reliance Edge.

_WARNING: The `fsstress` and `redfmt` tools clear all data from the given file
or drive.  Be careful not to accidentally specify a drive letter, PhysicalDrive
name, or sensitive file name as the device parameter._

## Requirements

- A computer running Windows 7 or higher, connected to the Internet

## Building the Reliance Edge Win32 Project

First, you will build the tests that are bundled with the product, allowing you
to run Reliance Edge on a Windows computer.

1. Download and install
   [Microsoft Visual Studio Community edition](https://visualstudio.microsoft.com/vs/community/)
   (free).

   _Note: Most recent versions of Visual Studio also work.  Visual Studio 2005
   and earlier are not compatible.  The package titled "Visual Studio Express
   for Windows" is not compatible._

2. Download and install the
   [GNU Make utility for Windows](http://gnuwin32.sourceforge.net/packages/make.htm)
   (free).

   - Download the setup package listed as "Complete package, except sources".

3. Add `make.exe` to the `%PATH%`.

   - Open the Start menu and type "path" in the search box.
   - Click on the entry "Edit environment variables for your account".
   - Find the user variable "Path"; click it and click "Edit".
   - Add the path to the GNU `make.exe` file.  On a normal installation, this
     path will be similar to: `C:\Program Files (x86)\GnuWin32\bin`
   - Click OK on the dialog and the Environment Variables window.

4. Download the Reliance Edge source code.

   - Go to the Reliance Edge GitHub page
     ([github.com/tuxera/reliance-edge](https://github.com/tuxera/reliance-edge))
     and click on the
     [release tab](https://github.com/tuxera/reliance-edge/releases).

   - Click on a source code download link from the latest stable release.
     Once complete, unzip the downloaded file.

   _Note: If you have Git installed, you may prefer to clone the repository:_
   `git clone https://github.com/tuxera/reliance-edge.git`

5. Open the Visual Studio Command Prompt from the Start menu.  Use the `cd`
   command to set the working directory to the directory `projects\win32`,
   within the Reliance Edge source code folder.

6. Run `make`.

7. Run the following command to perform a quick test and ensure the file system
   works.

   `fsstress 0 --dev=img.bin -v -n 100`

   _Note: This command runs a stress test on volume zero using the file
   `img.bin` as a disk image (the file will be created if it does not exist).
   This command specifies verbose output (`-v`) and performs 100 file system
   operations (`-n 100`).  If it is successful, the last line it will print is
   "`fsstress end, return 0`"._

## Building the Win32 Host Tools

1. From the same Visual Studio command prompt, run `cd host` to set the working
   directory to `projects\win32\host`.

2. Run `make`.

3. Format a disk image using the following command: `redfmt 0 --dev=img.bin`

## What's Next?

This section introduces the steps needed to get Reliance Edge running on an
embedded device.  Further information about development for Reliance Edge can be
found in the documentation, available at
[tuxera.com/products/reliance-edge](https://www.tuxera.com/products/reliance-edge/).

The projects you just built use Reliance Edge as a file system on a Windows
computer using Windows drivers and file systems to access the storage media.
This proves the functionality of Reliance Edge but is a far cry from the IoT
devices that it targets.  Several steps are needed to set up a Reliance Edge
project for an embedded (or IoT) device.

Reliance Edge has an extensive set of compile time configuration options, which
include various storage media properties.  The configuration values are set by
the project in the files redconf.h and redconf.c.  The Reliance Edge
Configuration Utility provides a graphical user interface for easily setting up
these values.  The tool is available from Tuxera: contact <support@tuxera.com>
for details.  See the "Product Configuration" section of the documentation for
more information.

The interface between Reliance Edge and the underlying storage media drivers
must also be set up for a project.  Instructions for this procedure are given in
the "Porting Guide" section of the documentation.

If you have further questions about setting up your Reliance Edge project,
contact <support@tuxera.com>.
