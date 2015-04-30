/** @file
*/

/*  Inherit most settings from the target configuration.
*/
#include "../redconf.h"


#ifndef HOST_REDCONF_H
#define HOST_REDCONF_H

#if REDCONF_ENDIAN_BIG == 1
#define REDCONF_ENDIAN_SWAP
#endif

#undef  REDCONF_ENDIAN_BIG
#define REDCONF_ENDIAN_BIG 0

/*  Ignore the target system memory alignment.  For Windows, 4 bytes works well.
*/
#undef  REDCONF_ALIGNMENT_SIZE
#define REDCONF_ALIGNMENT_SIZE 4U

/*  Host tools always have output.
*/
#undef  REDCONF_OUTPUT
#define REDCONF_OUTPUT 1

/*  Read-only must be disabled for the image builder.
*/
#undef  REDCONF_READ_ONLY
#define REDCONF_READ_ONLY 0

/*  Enable the checker host tool.
*/
#undef  REDCONF_CHECKER
#define REDCONF_CHECKER 1

/*  Enable the formatter code in POSIX-like API configurations for the image
    builder and formatter host tools.
*/
#undef  REDCONF_API_POSIX_FORMAT
#define REDCONF_API_POSIX_FORMAT 1

/*  Enable the image builder host tool.
*/
#undef  REDCONF_IMAGE_BUILDER
#define REDCONF_IMAGE_BUILDER 1

/*  The image builder needs red_mkdir().
*/
#undef  REDCONF_API_POSIX_MKDIR
#define REDCONF_API_POSIX_MKDIR 1

/*  The image copier utility needs red_readdir().
*/
#undef  REDCONF_API_POSIX_READDIR
#define REDCONF_API_POSIX_READDIR 1

/*  The image copier utility needs a handle for every level of directory depth.
    While Reliance Edge has no maximum directory depth or path depth, Windows
    limits paths to 260 bytes, and each level of depth eats up at least two
    characters, 130 handles will be sufficient for all images that can be
    copied.
*/
#undef  REDCONF_HANDLE_COUNT
#define REDCONF_HANDLE_COUNT 130U

/*  The target redconf.h may have configured the memory and string functions to
    use custom implementations that are only available on the target system.  So
    for the host, we just use the C library versions.
*/
#include <string.h>

#undef  RedMemCpy
#define RedMemCpy   memcpy
#undef  RedMemMove
#define RedMemMove  memmove
#undef  RedMemSet
#define RedMemSet   memset
#undef  RedMemCmp
#define RedMemCmp   memcmp

#undef  RedStrLen
#define RedStrLen   (uint32_t)strlen
#undef  RedStrCmp
#define RedStrCmp   strcmp
#undef  RedStrNCmp
#define RedStrNCmp  strncmp
#undef  RedStrNCpy
#define RedStrNCpy  strncpy

#endif

