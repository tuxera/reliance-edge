/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2021 Tuxera US Inc.
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
    comply with the terms of the GPLv2 license must obtain a commercial license
    before incorporating Reliance Edge into proprietary software for
    distribution in any form.  Visit http://www.datalight.com/reliance-edge for
    more information.
*/
#ifndef ALLSETTINGS_H
#define ALLSETTINGS_H

#include "settings/cbsetting.h"
#include "settings/cmbintsetting.h"
#include "settings/cmbstrsetting.h"
#include "settings/pathsepsetting.h"
#include "settings/rbtnsetting.h"
#include "settings/sbsetting.h"
#include "settings/lesetting.h"

///
/// \brief  Structure containing public settings pointers for global access.
///         Instantiated globally in ::allSettings
///
struct AllSettings
{
    ///
    /// \brief  Formats a string for output to a redconf.h file. Uses the values
    ///         in the global variables ::allSettings and ::volumeSettings to
    ///         fill in values. Assumes that all values are valid.
    ///
    static QString FormatHeaderOutput();

    ///
    /// \brief  Fills errors and warnings with any errors and warnings found in
    ///         current settings
    ///
    /// \param errors   List to which to add any error messages
    /// \param warnings List to which to add any warning messages
    ///
    static void GetErrors(QStringList &errors, QStringList &warnings);

    ///
    /// \brief  Wrapper function for VolumeSettings::FormatCodefileOutput
    ///
    static QString FormatCodefileOutput();

    ///
    /// \brief  Looks for settings in the given string \p text. The macro names
    ///         of any missing values are added to \p notFound and the macro
    ///         names of any unparseable values are added to \p notParsed.
    ///
    static void ParseHeaderToSettings(const QString &text,
                                      QStringList &notFound,
                                      QStringList &notParsed);

    ///
    /// \brief  Wrapper function for VolumeSettings::ParseCodefile
    ///
    static void ParseCodefileToSettings(const QString &text,
                                        QStringList &notFound,
                                        QStringList &notParsed);

    ///
    /// \brief  Checks for validity of \p setting, appending any message to
    ///         \p errors or \p warnings.
    ///
    static void CheckError(SettingBase *setting,
                           QStringList &errors, QStringList &warnings);

    ///
    /// \brief  Deletes all members of ::allSettings and sets them to NULL
    ///
    static void DeleteAll();

    // "General" tab
    CbSetting *cbsReadonly;
    CbSetting *cbsAutomaticDiscards;
    RbtnSetting *rbtnsUsePosix;
    RbtnSetting *rbtnsUseFse;
    CbSetting *cbsPosixFormat;
    CbSetting *cbsPosixLink;
    CbSetting *cbsPosixUnlink;
    CbSetting *cbsPosixMkdir;
    CbSetting *cbsPosixRmdir;
    CbSetting *cbsPosixRename;
    CbSetting *cbsPosixAtomicRename;
    CbSetting *cbsPosixFtruncate;
    CbSetting *cbsPosixDirOps;
    CbSetting *cbsPosixCwd;
    CbSetting *cbsPosixFstrim;
    SbSetting *sbsMaxNameLen;
    PathSepSetting *pssPathSepChar;
    SbSetting *sbsTaskCount;
    SbSetting *sbsHandleCount;
    CbSetting *cbsFseFormat;
    CbSetting *cbsFseTruncate;
    CbSetting *cbsFseGetMask;
    CbSetting *cbsFseSetMask;
    CbSetting *cbsDebugEnableOutput;
    CbSetting *cbsDebugProcesAsserts;

    // "Volumes" tab (Note: most settings handled by VolumeSettings)
    CmbIntSetting *cmisBlockSize;

    // "Data" tab
    CmbStrSetting *cmssByteOrder;
    CmbIntSetting *cmisNativeAlignment;
    CmbStrSetting *cmssCrc;
    CbSetting *cbsInodeBlockCount;
    CbSetting *cbsInodeTimestamps;
    CbSetting *cbsUpdateAtime;
    SbSetting *sbsDirectPtrs;
    SbSetting *sbsIndirectPtrs;

    // "Memory" tab
    SbSetting *sbsAllocatedBuffers;
    LeSetting *lesMemcpy;
    LeSetting *lesMemmov;
    LeSetting *lesMemset;
    LeSetting *lesMemcmp;
    LeSetting *lesStrlen;
    LeSetting *lesStrcmp;
    LeSetting *lesStrncmp;
    LeSetting *lesStrncpy;
    LeSetting *lesInclude;

    // "Transactions" tab
    CbSetting *cbsTrManual;
    CbSetting *cbsTrFileCreat;
    CbSetting *cbsTrDirCreat;
    CbSetting *cbsTrRename;
    CbSetting *cbsTrLink;
    CbSetting *cbsTrUnlink;
    CbSetting *cbsTrWrite;
    CbSetting *cbsTrTruncate;
    CbSetting *cbsTrFSync;
    CbSetting *cbsTrClose;
    CbSetting *cbsTrVolFull;
    CbSetting *cbsTrUmount;
    CbSetting *cbsTrSync;
};

extern const QString macroNameReadonly;
extern const QString macroNameAutomaticDiscards;
extern const QString macroNameUsePosix;
extern const QString macroNameUseFse;
extern const QString macroNamePosixFormat;
extern const QString macroNamePosixLink;
extern const QString macroNamePosixUnlink;
extern const QString macroNamePosixMkdir;
extern const QString macroNamePosixRmdir;
extern const QString macroNamePosixRename;
extern const QString macroNamePosixRenameAtomic;
extern const QString macroNamePosixFtruncate;
extern const QString macroNamePosixDirOps;
extern const QString macroNamePosixCwd;
extern const QString macroNamePosixFstrim;
extern const QString macroNameMaxNameLen;
extern const QString macroNamePathSepChar;
extern const QString macroNameTaskCount;
extern const QString macroNameHandleCount;
extern const QString macroNameFseFormat;
extern const QString macroNameFseTruncate;
extern const QString macroNameFseGetMask;
extern const QString macroNameFseSetMask;
extern const QString macroNameDebugEnableOutput;
extern const QString macroNameDebugProcesAsserts;

// "Volumes" tab
extern const QString macroNameBlockSize;
extern const QString macroNameVolumeCount; //Not in UI

// "Data Storage" tab
extern const QString macroNameByteOrder;
extern const QString macroNameNativeAlignment;
extern const QString macroNameCrc;
extern const QString macroNameInodeCount;
extern const QString macroNameInodeTimestamps;
extern const QString macroNameUpdateAtime;
extern const QString macroNameDirectPtrs;
extern const QString macroNameIndirectPtrs;

// Not in UI:
extern const QString macroNameInlineImap;
extern const QString macroNameExternalImap;

// "Memory" tab
extern const QString macroNameAllocatedBuffers;
extern const QString macroNameMemcpy;
extern const QString macroNameMemmov;
extern const QString macroNameMemset;
extern const QString macroNameMemcmp;
extern const QString macroNameStrlen;
extern const QString macroNameStrcmp;
extern const QString macroNameStrncmp;
extern const QString macroNameStrncpy;

// "Transaction Points" tab
extern const QString macroNameTrDefault; //Not in UI
extern const QString macroNameTrManual;
extern const QString macroNameTrFileCreat;
extern const QString macroNameTrDirCreat;
extern const QString macroNameTrRename;
extern const QString macroNameTrLink;
extern const QString macroNameTrUnlink;
extern const QString macroNameTrWrite;
extern const QString macroNameTrTruncate;
extern const QString macroNameTrFSync;
extern const QString macroNameTrClose;
extern const QString macroNameTrVolFull;
extern const QString macroNameTrUmount;
extern const QString macroNameTrSync;

// Mem & str management function names
extern const QString cstdMemcpy;
extern const QString cstdMemmov;
extern const QString cstdMemset;
extern const QString cstdMemcmp;
extern const QString cstdStrlen;
extern const QString cstdStrcmp;
extern const QString cstdStrncmp;
extern const QString cstdStrncpy;
extern const QString cstdStringH;

// Enum-like macro values
extern const QString crcBitwise;
extern const QString crcSarwate;
extern const QString crcSlice;

///
/// \brief  Global ::AllSettings object.
///
///         Accessed by validators, ::Input, ::Output, etc. Initialized in
///         ::ConfigWindow constructor.
///
extern AllSettings allSettings;

#endif // ALLSETTINGS_H
