/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2024 Tuxera US Inc.
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
    comply with the terms of the GPLv2 license must obtain a commercial
    license before incorporating Reliance Edge into proprietary software
    for distribution in any form.

    Visit https://www.tuxera.com/products/reliance-edge/ for more information.
*/
/** @file
    @brief Interfaces for image builder and image copier.
*/
#ifndef REDTOOLS_H
#define REDTOOLS_H


#ifdef _WIN32
  #include <windows.h>
  #define HOST_PATH_MAX MAX_PATH
#elif defined(__linux__) || defined(unix) || defined(__unix__) || defined(__unix)
  #include <linux/limits.h>
  #define HOST_PATH_MAX PATH_MAX
#endif

#ifdef _WIN32
  /*  Account for the difference in base-points for the timestamps.  Windows
      time starts at January 1, 1601, and is specified in 100-nanosecond
      intervals.  Unix time starts at January 1, 1970.  The following constant
      values are used to convert Windows time to Unix time.
  */
  #define TIME_1601_TO_1970_DAYS  134774U
  #define TIME_100NANOS_PER_DAY   864000000000U
#endif

#ifdef st_atime
  #define POSIX_2008_STAT

  /*  These undefs are preset to remove the definitions for the fields of the
      same name in the POSIX time and stat code.  If these are not present, a
      very cryptic error message will occur if this is compiled on a Linux (or
      other POSIX-based) system.
  */
  #undef st_atime
  #undef st_mtime
  #undef st_ctime
#endif

/*  If redstat.h and sys/stat.h have both been included...
*/
#if defined(RED_S_IFREG) && defined(S_ISREG)
  /*  Reliance Edge uses the same permission bit values as Linux.  The code
      assumes that no translation needs to occur: make sure that this is the
      case.
  */
  #if (RED_S_IFREG != S_IFREG) || (RED_S_IFDIR != S_IFDIR) || (RED_S_ISUID != S_ISUID) || (RED_S_ISGID != S_ISGID) || \
      (RED_S_ISVTX != S_ISVTX) || (RED_S_IRWXU != S_IRWXU) || (RED_S_IRWXG != S_IRWXG) || (RED_S_IRWXO != S_IRWXO)
    #error "error: Reliance Edge permission bits don't match host OS permission bits!"
  #endif
#endif

/*  Whether Reliance Edge has settable attributes.
*/
#define HAVE_SETTABLE_ATTR \
    ((REDCONF_API_POSIX == 1) && ((REDCONF_INODE_TIMESTAMPS == 1) || (REDCONF_POSIX_OWNER_PERM == 1)))


#if REDCONF_IMAGE_BUILDER == 1

#include "redformat.h"

#define MACRO_NAME_MAX_LEN 32U

#ifdef _WIN32
  #define IB_ISPATHSEP(c) (((c) == '\\') || ((c) == '/'))
#else
  #define IB_ISPATHSEP(c) ((c) == '/')
#endif

typedef struct
{
    uint8_t     bVolNum;
    const char *pszInputDir;
    const char *pszOutputFile;
  #if REDCONF_API_POSIX == 1
    const char *pszVolName;
  #else
    const char *pszMapFile;
    const char *pszDefineFile;
    bool        fNoWarn;
  #endif
    REDFMTOPT   fmtopt;
} IMGBLDPARAM;


void ImgbldParseParams(int argc, char *argv[], IMGBLDPARAM *pParam);
int ImgbldStart(IMGBLDPARAM *pParam);


typedef struct
{
  #if REDCONF_API_POSIX == 1
    char     szOutFilePath[HOST_PATH_MAX];
  #else
    uint32_t ulOutFileIndex;
  #endif
    char     szInFilePath[HOST_PATH_MAX];
} FILEMAPPING;

/*  Subset of ::REDSTAT.
*/
typedef struct
{
    uint16_t uMode;
    uint32_t ulUid;
    uint32_t ulGid;
    uint64_t ullSize;
    uint32_t ulATime;
    uint32_t ulMTime;
} IBSTAT;


extern void *gpCopyBuffer;
extern uint32_t gulCopyBufferSize;


/*  Implemented in ibposix.c
*/
#if REDCONF_API_POSIX == 1
REDSTATUS IbPosixCopyDir(const char *pszVolName, const char *pszInDir);
int IbPosixCreateDir(const char *pszVolName, const char *pszFullPath, const char *pszBasePath);
int IbConvertPath(const char *pszVolName, const char *pszFullPath, const char *pszBasePath, char *pszOutPath);
#if HAVE_SETTABLE_ATTR
int IbCopyAttr(const char *pszHostPath, const char *pszRedPath);
#endif
#endif


/*  Implemented in ibfse.c
*/
#if REDCONF_API_FSE == 1
typedef struct sFILELISTENTRY FILELISTENTRY;
struct sFILELISTENTRY
{
    FILEMAPPING     fileMapping;
    FILELISTENTRY  *pNext;
};


void FreeFileList(FILELISTENTRY **ppFileList);

int IbFseGetFileList(const char *pszPath, const char *pszIndirPath, FILELISTENTRY **ppFileListHead);
int IbFseOutputDefines(FILELISTENTRY *pFileList, const IMGBLDPARAM *pOptions);
int IbFseCopyFiles(uint8_t bVolNum, const FILELISTENTRY *pFileList);
#endif


/*  Implemented in OS-specific space (imgbldwin.c and imgbldlinux.c)
*/
#if REDCONF_API_POSIX == 1
int IbPosixCopyDirRecursive(const char *pszVolName, const char *pszInDir);
#endif
#if REDCONF_API_FSE == 1
int IbFseBuildFileList(const char *pszDirPath, FILELISTENTRY **ppFileListHead);
int IbSetRelativePath(char *pszPath, const char *pszParentPath);
#endif
bool IsRegularFile(const char *pszPath);
int IbStat(const char *pszPath, IBSTAT *pStat);


/*  Implemented separately in ibfse.c and ibposix.c
*/
int IbApiInit(void);
int IbApiUninit(void);
int IbCopyFile(uint8_t bVolNum, const FILEMAPPING *pFileMapping);

#endif /* REDCONF_IMAGE_BUILDER == 1 */


/*  For image copier tool
*/

#ifdef _WIN32
  #define HOST_PSEP '\\'
  #if !__STDC__
    #define snprintf _snprintf
    #define stat _stat
    #define S_IFDIR _S_IFDIR
    #define rmdir _rmdir
  #endif
#else
  #define HOST_PSEP '/'
#endif

typedef struct
{
    uint8_t     bVolNum;
    const char *pszOutputDir;
    const char *pszBDevSpec;
  #if REDCONF_API_POSIX == 1
    const char *pszVolName;
  #endif
    bool        fNoWarn;
} IMGCOPYPARAM;

typedef struct
{
  #if REDCONF_API_POSIX == 1
    const char *pszVolume;      /* Volume path prefix. */
    uint32_t    ulVolPrefixLen; /* strlen(COPIER::pszVolume) */
  #else
    uint8_t     bVolNum;        /* Volume number. */
  #endif
    const char *pszOutputDir;   /* Output directory path. */
    bool        fNoWarn;        /* If true, no warning to overwrite. */
    uint8_t    *pbCopyBuffer;   /* Buffer for copying file data. */
} COPIER;


void ImgcopyParseParams(int argc, char *argv[], IMGCOPYPARAM *pParam);
int ImgcopyStart(IMGCOPYPARAM *pParam);

/*  Implemented separately in imgcopywin.c and imgcopylinux.c.  These functions
    print an error message and abort on failure.
*/
void ImgcopyMkdir(const char *pszDir);
void ImgcopyRecursiveRmdir(const char *pszDir);
#if (REDCONF_API_POSIX == 1) && (REDCONF_POSIX_OWNER_PERM == 1)
void ImgcopyChown(const char *pszPath, uint32_t ulUid, uint32_t ulGid);
void ImgcopyChmod(const char *pszPath, uint16_t uMode);
#endif
#if REDCONF_API_POSIX_SYMLINK == 1
void ImgcopySymlink(const char *pszPath, const char *pszSymlink);
#endif
#if (REDCONF_API_POSIX == 1) && (REDCONF_INODE_TIMESTAMPS == 1)
void ImgcopyUtimes(const char *pszPath, uint32_t *pulTimes);
#endif

#endif /* REDTOOLS_H */
