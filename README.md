# Reliance Edge

Reliance Edge is a small, portable, highly reliable power-fail safe file system
for resource-constrained embedded systems like microcontrollers.  It is written
in C and provides a familiar POSIX-like file system API, making it easy to use
in your application; or an alternate minimalist API if your application has
simple storage needs.  Reliance Edge is highly configurable and can be tuned to
the precise needs of your application.

## Getting Help

You will find this section repeated below, but we wanted to tell you up front:
TUXERA WANTS TO HEAR FROM YOU!  Whether it's just to make comments about the
product, to suggest new features, or to tell us what you don't like—reach out!
All comments and inquiries can be directed to <support@tuxera.com>.

## Documentation

The complete documentation for Reliance Edge is distributed separately.  It
includes an API reference and detailed discussions of various aspects of using
Reliance Edge, including porting, building, configuring, and testing.  This
complete documentation, called the _Developer's Guide_, can be obtained for free
by contacting <support@tuxera.com>.

In addition to this README, see doc/release_notes.md for a list of updates
to Reliance Edge and a list of known issues.  There is also a quick-start
guide in the doc/ directory that describes step-by-step how to compile
and run Reliance Edge in a simulated Windows environment.

## Why Use Reliance Edge?

Reliance Edge is ideal for small embedded systems with data storage
requirements, especially if there is a chance of sudden power loss or other
system failures.  Compared to "raw" disk access, using a file system like
Reliance Edge removes the burden of tracking which sectors belong to which
objects, and allows data to be updated more reliably.  Compared to the FAT file
system, using Reliance Edge eliminates the possibility that file system data
will be left in an inconsistent state, corrupting the disk; Reliance Edge does
not need a fsck/CHKDSK utility.  Compared to journaling file systems, Reliance
Edge has less overhead and results in less storage media wear for longer device
lifetimes.

Reliance Edge uses a unique transactional model that not only prevents file
system corruption but also allows a set of changes to be made in an atomic "all
or nothing" fashion.  This is very useful for applications that make sets of
interrelated changes.  By using the features of Reliance Edge, a set of changes
can be incorporated into a single atomic transaction, which is committed in its
entirety or not at all even if interrupted by power loss; this means the
application does not need code to recover from partially-finished updates.

## Hardware

The typical hardware for Reliance Edge is a 32-bit microcontroller, but other
targets are possible.  In its typical configurations, Reliance Edge needs at
least 4 KB to 5 KB of RAM, 11 to 18 KB of code space (on the ROM or NOR flash),
and 500 to 700 bytes of stack.

Reliance Edge is not designed for high-end embedded systems that run complicated
operating systems like Linux or VxWorks.  Embedded systems of that variety are
better served by other file systems, like Tuxera's
[Reliance Nitro](https://www.tuxera.com/products/tuxera-nitrofs/).

## Getting Reliance Edge Working

Before you can use Reliance Edge, it must be ported and configured.  At a
minimum, porting includes filling-in functions so that Reliance Edge can issue
commands to your storage medium; depending on your needs, other functions may
need to be filled in as well.  These functions reside in a subdirectory in the
os/ directory; see os/stub/ for a blank set of functions.  Configuring includes
creating a project directory (start by copying projects/newproj) and creating
the two configuration files (redconf.h/redconf.c) using the Reliance Edge
Configuration Utility (which has source code in the repository and prebuilt
binaries available from <support@tuxera.com>).

These topics are covered in much greater detail in the _Developer's Guide_,
discussed above.

## Using Reliance Edge

Using Reliance Edge is a simple matter of including the primary Reliance Edge
application header in your application (either include/redposix.h or
include/redfse.h) and compiling and linking against Reliance Edge binaries.
The Reliance Edge driver must be initialized before it is used (via the
red\_init() or RedFseInit() functions) and then volumes can be mounted and file
and directory functions invoked.  The Reliance Edge API is documented in the
_Developer's Guide_ (see above) and also via comments in the source code.

## Licensing

Reliance Edge is an open-source project licensed under the GNU General Public
License v2 (GPLv2).  Businesses and individuals that for commercial or other
reasons cannot comply with the terms of the GPLv2 license must obtain a
commercial license before incorporating Reliance Edge into proprietary software
for distribution in any form.  Visit <https://www.tuxera.com/products/tuxera-edge-fs/>
for more information.  The commercial distribution also includes extra tests and
tools not distributed with the GPLv2 version.

See LICENSE.txt for the full license terms of this distribution of the product.

## Getting Help

If you need assistance using Reliance Edge, and you have already consulted the
_Developer's Guide_, contact <support@tuxera.com>.

[GitHub Discussions](https://docs.github.com/en/discussions) are enabled on the
Reliance Edge GitHub repository to provide a forum for discussion of Reliance
Edge and to allow users to get help from Tuxera and from each other.  To access
this forum, open the "Discussions" tab on the GitHub page for the repository or
navigate to the following URL: <https://github.com/tuxera/reliance-edge/discussions>

## Contributing

Contributions to Reliance Edge are welcome.  Our policy is that Tuxera US Inc
must own the copyright of all code incorporated into Reliance Edge; if
contributing a significant amount of code, you will be asked to file a copyright
assignment agreement.  See CONTRIBUTING.txt for further details and contribution
guidelines.

To report bugs, please create a GitHub issue or contact <support@tuxera.com>.
