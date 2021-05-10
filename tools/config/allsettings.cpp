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
#include "volumesettings.h"
#include "allsettings.h"
#include "version.h"


// Private helpers
static QString outputLine(const QString &macroName, const QString &value,
                          const QString &comment = QString::null);
static QString outputIfNotBlank(const QString &macroName, const QString &value,
                                const QString &comment = QString::null);
static qint32 getMinCompatVer();
template<typename T>
static void parseToSetting(const QString &text, Setting<T> *setting,
                          QStringList &notFound, QStringList &notParsed,
                          const QString &humanName = QString::null);
static void parseMemSetting(const QString &text, StrSetting *setting);
static void parseToEnabledDisabledSetting(const QString &text,
                                          StrSetting *setting,
                                          const QString &strTrue,
                                          const QString &strFalse,
                                          QStringList &notFound,
                                          QStringList &notParsed,
                                          const QString &humanName = QString::null);
static void parseToTrSetting(const QString &text, BoolSetting *setting);
static QString findValue(const QString &text, const QString &macroName, bool &found);

// Constant strings to save some malloc's and code duplicaiton
const QString str1 = QString("1"),
              str0 = QString("0"),
              strU = QString("U");

// Private inline helpers for FormatHeaderOutput. These process
// a given setting and append C preprocessor code to outputString
// as specified by the setting.

static inline void addBoolSetting(QString &outputString, BoolSetting *boolSetting)
{
    outputString += outputLine(boolSetting->GetMacroName(),
                               (boolSetting->GetValue() ? str1 : str0));
}

static inline void addIntSetting(QString &outputString, IntSetting *intSetting)
{
    outputString += outputLine(intSetting->GetMacroName(),
                               QString::number(intSetting->GetValue()) + strU);
}

// Add a transaction point to the mask at the end of outputString
// if trSetting is set to true.
// Trasnaction point will be exluded without evaluating trSetting
// if override is set to false. Evaluated normally if override
// is true or unspecified.
static inline void addTrIfChecked(QString &outputString, BoolSetting *trSetting,
                                  bool addAnyway = true)
{
    if(addAnyway && trSetting->GetValue())
    {
        // Essential to end with the pipe char; overwritten
        // after final trSetting is evaluated.
        outputString += QString(" ") + trSetting->GetMacroName() + QString(" |");
    }
}

// Takes a pointer to a pointer; deletes the *item
// and sets it to NULL
template<typename T>
static inline void deleteAndNullify(T **item)
{
    delete *item;
    *item = NULL;
}

QString AllSettings::FormatHeaderOutput()
{
    QString toReturn = QString("/** @file\n*/\n#ifndef REDCONF_H\n#define REDCONF_H\n\n\n");
    QString currValue;

    // Assert one setting. Assume the others.
    Q_ASSERT(allSettings.cbsReadonly != NULL);
    Q_ASSERT(volumeSettings != NULL);

    // Add include statement at the top
    currValue = allSettings.lesInclude->GetValue();
    if(!currValue.isNull() && !currValue.isEmpty())
    {
        toReturn += QString("#include ") + currValue + QString("\n\n");
    }

    addBoolSetting(toReturn, allSettings.cbsReadonly);

    bool posix = allSettings.rbtnsUsePosix->GetValue();

    addBoolSetting(toReturn, allSettings.rbtnsUsePosix);
    addBoolSetting(toReturn, allSettings.rbtnsUseFse);

    // Set POSIX calls to true if selected and POSIX is enabled
    toReturn += outputLine(allSettings.cbsPosixFormat->GetMacroName(),
                           (posix && allSettings.cbsPosixFormat->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsPosixLink->GetMacroName(),
                           (posix && allSettings.cbsPosixLink->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsPosixUnlink->GetMacroName(),
                           (posix && allSettings.cbsPosixUnlink->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsPosixMkdir->GetMacroName(),
                           (posix && allSettings.cbsPosixMkdir->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsPosixRmdir->GetMacroName(),
                           (posix && allSettings.cbsPosixRmdir->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsPosixRename->GetMacroName(),
                           (posix && allSettings.cbsPosixRename->GetValue() ? str1 : str0));
    // Greyed out if cbPosixRename not selected
    toReturn += outputLine(allSettings.cbsPosixAtomicRename->GetMacroName(),
                           (posix && allSettings.cbsPosixRename->GetValue()
                            && allSettings.cbsPosixAtomicRename->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsPosixFtruncate->GetMacroName(),
                           (posix && allSettings.cbsPosixFtruncate->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsPosixDirOps->GetMacroName(),
                           (posix && allSettings.cbsPosixDirOps->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsPosixCwd->GetMacroName(),
                           (posix && allSettings.cbsPosixCwd->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsPosixFstrim->GetMacroName(),
                           (posix && allSettings.cbsPosixFstrim->GetValue() ? str1 : str0));

    addIntSetting(toReturn, allSettings.sbsMaxNameLen);

    QString pathSepChar = allSettings.pssPathSepChar->GetValue();

    // Handle special case characters here
    if(QString::compare(pathSepChar, "\\") == 0)
    {
        pathSepChar = "\\\\"; //Double-escaped backslash
    }
    else if (QString::compare(pathSepChar, "'") == 0)
    {
        pathSepChar = "\\'";
    }
    else if (QString::compare(pathSepChar, "\t") == 0)
    {
        pathSepChar = "\\t";
    }

    currValue = QString("'") + pathSepChar + QString("'");
    toReturn += outputLine(macroNamePathSepChar, currValue);

    addIntSetting(toReturn, allSettings.sbsTaskCount);
    addIntSetting(toReturn, allSettings.sbsHandleCount);

    // Set FSE calls to true if selected and POSIX is disabled
    toReturn += outputLine(allSettings.cbsFseFormat->GetMacroName(),
                           (!posix && allSettings.cbsFseFormat->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsFseTruncate->GetMacroName(),
                           (!posix && allSettings.cbsFseTruncate->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsFseGetMask->GetMacroName(),
                           (!posix && allSettings.cbsFseGetMask->GetValue() ? str1 : str0));
    toReturn += outputLine(allSettings.cbsFseSetMask->GetMacroName(),
                           (!posix && allSettings.cbsFseSetMask->GetValue() ? str1 : str0));

    addBoolSetting(toReturn, allSettings.cbsDebugEnableOutput);
    addBoolSetting(toReturn, allSettings.cbsDebugProcesAsserts);

    // "Volumes" tab

    addIntSetting(toReturn, allSettings.cmisBlockSize);

    addIntSetting(toReturn, volumeSettings->GetStVolumeCount());

    // "Data" tab

    currValue = (allSettings.cmssByteOrder->GetValue().startsWith("big", Qt::CaseInsensitive)
                 ? "1" : "0");
    toReturn += outputLine(macroNameByteOrder, currValue);

    addIntSetting(toReturn, allSettings.cmisNativeAlignment);

    currValue = (allSettings.cmssCrc->GetValue().startsWith("bitwise", Qt::CaseInsensitive)
                 ? crcBitwise : (allSettings.cmssCrc->GetValue().startsWith("sarwate", Qt::CaseInsensitive)
                                 ? crcSarwate : crcSlice));
    toReturn += outputLine(macroNameCrc, currValue);

    addBoolSetting(toReturn, allSettings.cbsInodeBlockCount);
    addBoolSetting(toReturn, allSettings.cbsInodeTimestamps);

    // Greyed out if cbInodeTimestamps is not selected
    toReturn += outputLine(allSettings.cbsUpdateAtime->GetMacroName(),
                           ((allSettings.cbsInodeTimestamps->GetValue()
                             && allSettings.cbsUpdateAtime->GetValue())
                            ? str1 : str0));

    addIntSetting(toReturn, allSettings.sbsDirectPtrs);
    addIntSetting(toReturn, allSettings.sbsIndirectPtrs);

    addIntSetting(toReturn, allSettings.sbsAllocatedBuffers);

    toReturn += outputIfNotBlank(macroNameMemcpy, allSettings.lesMemcpy->GetValue());
    toReturn += outputIfNotBlank(macroNameMemmov, allSettings.lesMemmov->GetValue());
    toReturn += outputIfNotBlank(macroNameMemset, allSettings.lesMemset->GetValue());
    toReturn += outputIfNotBlank(macroNameMemcmp, allSettings.lesMemcmp->GetValue());
    toReturn += outputIfNotBlank(macroNameStrlen, allSettings.lesStrlen->GetValue());
    toReturn += outputIfNotBlank(macroNameStrcmp, allSettings.lesStrcmp->GetValue());
    toReturn += outputIfNotBlank(macroNameStrncmp, allSettings.lesStrncmp->GetValue());
    toReturn += outputIfNotBlank(macroNameStrncpy, allSettings.lesStrncpy->GetValue());

    // Transaction points
    if(allSettings.cbsTrManual->GetValue())
    {
        currValue = QString("(") + macroNameTrManual + QString(")");
    }
    else
    {
        currValue = QString("((");
        QString rememberCurrVal = currValue;

        // Add each transaction point if checked and if corresponding
        // API call is enabled

        addTrIfChecked(currValue, allSettings.cbsTrFileCreat, posix);
        addTrIfChecked(currValue, allSettings.cbsTrDirCreat,
                       posix && allSettings.cbsPosixMkdir->GetValue());
        addTrIfChecked(currValue, allSettings.cbsTrRename,
                       posix && allSettings.cbsPosixRename->GetValue());
        addTrIfChecked(currValue, allSettings.cbsTrLink,
                       posix && allSettings.cbsPosixLink->GetValue());
        addTrIfChecked(currValue, allSettings.cbsTrUnlink,
                       posix && allSettings.cbsPosixUnlink->GetValue());
        addTrIfChecked(currValue, allSettings.cbsTrWrite);
        addTrIfChecked(currValue, allSettings.cbsTrTruncate,
                       (posix && allSettings.cbsPosixFtruncate->GetValue())
                       || (!posix && allSettings.cbsFseTruncate->GetValue()));
        addTrIfChecked(currValue, allSettings.cbsTrFSync, posix);
        addTrIfChecked(currValue, allSettings.cbsTrClose, posix);
        addTrIfChecked(currValue, allSettings.cbsTrVolFull);
        addTrIfChecked(currValue, allSettings.cbsTrUmount);
        addTrIfChecked(currValue, allSettings.cbsTrSync, posix);

        // Ensure some flags were added (currValue was changed)
        if(currValue != rememberCurrVal)
        {
            // Overwrite the final pipe character
            currValue[currValue.length() - 1] = ')';
            currValue += " & RED_TRANSACT_MASK)";
        }
        else
        {
            currValue = QString("(") + macroNameTrManual + QString(")");
        }
    }
    toReturn += outputLine(macroNameTrDefault, currValue);

    bool imapInline;
    bool imapExternal;
    volumeSettings->GetImapRequirements(imapInline, imapExternal);

    toReturn += QString("#define ") + macroNameInlineImap
            + (imapInline ? QString(" 1\n\n") : QString(" 0\n\n"))
            + QString("#define ") + macroNameExternalImap
            + (imapExternal ? QString(" 1\n\n") : QString(" 0\n\n"));

    addBoolSetting(toReturn, allSettings.cbsAutomaticDiscards);

    toReturn += QString("#define REDCONF_IMAGE_BUILDER 0\n\n");
    toReturn += QString("#define REDCONF_CHECKER 0\n\n");

    toReturn += QString("#define RED_CONFIG_UTILITY_VERSION 0x")
            + QString::number(CONFIG_VERSION_VAL, 16)
            + QString("U\n\n");

    toReturn += QString("#define RED_CONFIG_MINCOMPAT_VER 0x")
            + QString::number(getMinCompatVer(), 16)
            + QString("U\n\n");

    toReturn += QString("#endif\n"); //close ifndef REDCONF_H

    return toReturn;
}

// Formats the given arguments and returns a C #define statement
// based on them
QString outputLine(const QString &macroName, const QString &value,
                   const QString &comment)
{
    return QString("#define ")
            + macroName
            + QString(" ")
            + value
            + (comment.isNull() ? QString("") : QString(" /* ")
                                                + comment
                                                + QString(" */"))
            + QString("\n\n");
}

// Wraps outputLine, but returns an empty string if the given
// string value is null, empty, or whitespace.
QString outputIfNotBlank(const QString &macroName, const QString &value,
                         const QString &comment)
{
    if(value.isNull() || value.isEmpty() || value.trimmed().isEmpty())
    {
        return "";
    }
    else
    {
        return outputLine(macroName, value, comment);
    }
}

// Get the minimum compatible version of Reliance Edge. If an earlier version
// of Reliance Edge tries to use this configuration, it should fail with a
// helpful error message.
static qint32 getMinCompatVer()
{
    if(allSettings.rbtnsUsePosix->GetValue() && allSettings.cbsTrSync->GetValue())
    {
        // sync support added in v2.3; breaks backwards compatibility only if
        // enabled.
        return 0x02030000;
    }
    else
    {
        // Volume sector offset added in v2.2, which adds a member to the
        // volume configuration, thus breaking backward compatibility.
        return 0x02020000;
    }
}

QString AllSettings::FormatCodefileOutput()
{
    Q_ASSERT(volumeSettings != NULL);
    return volumeSettings->FormatCodefileOutput();
}

void AllSettings::GetErrors(QStringList &errors, QStringList &warnings)
{
    // All CheckError calls in this function are parallel.

    // "General" tab
    AllSettings::CheckError(allSettings.cbsReadonly, errors, warnings);
    AllSettings::CheckError(allSettings.cbsAutomaticDiscards, errors, warnings);
    AllSettings::CheckError(allSettings.rbtnsUsePosix, errors, warnings);
    AllSettings::CheckError(allSettings.rbtnsUseFse, errors, warnings);
    AllSettings::CheckError(allSettings.cbsPosixFormat, errors, warnings);
    AllSettings::CheckError(allSettings.cbsPosixLink, errors, warnings);
    AllSettings::CheckError(allSettings.cbsPosixUnlink, errors, warnings);
    AllSettings::CheckError(allSettings.cbsPosixMkdir, errors, warnings);
    AllSettings::CheckError(allSettings.cbsPosixRmdir, errors, warnings);
    AllSettings::CheckError(allSettings.cbsPosixRename, errors, warnings);
    AllSettings::CheckError(allSettings.cbsPosixAtomicRename, errors, warnings);
    AllSettings::CheckError(allSettings.cbsPosixFtruncate, errors, warnings);
    AllSettings::CheckError(allSettings.cbsPosixDirOps, errors, warnings);
    AllSettings::CheckError(allSettings.cbsPosixCwd, errors, warnings);
    AllSettings::CheckError(allSettings.cbsPosixFstrim, errors, warnings);
    AllSettings::CheckError(allSettings.sbsMaxNameLen, errors, warnings);
    AllSettings::CheckError(allSettings.pssPathSepChar, errors, warnings);
    AllSettings::CheckError(allSettings.sbsTaskCount, errors, warnings);
    AllSettings::CheckError(allSettings.sbsHandleCount, errors, warnings);
    AllSettings::CheckError(allSettings.cbsFseFormat, errors, warnings);
    AllSettings::CheckError(allSettings.cbsFseTruncate, errors, warnings);
    AllSettings::CheckError(allSettings.cbsFseGetMask, errors, warnings);
    AllSettings::CheckError(allSettings.cbsFseSetMask, errors, warnings);
    AllSettings::CheckError(allSettings.cbsDebugEnableOutput, errors, warnings);
    AllSettings::CheckError(allSettings.cbsDebugProcesAsserts, errors, warnings);

    // "Volumes" tab
    AllSettings::CheckError(allSettings.cmisBlockSize, errors, warnings);

    // "Data" tab
    AllSettings::CheckError(allSettings.cmssByteOrder, errors, warnings);
    AllSettings::CheckError(allSettings.cmisNativeAlignment, errors, warnings);
    AllSettings::CheckError(allSettings.cmssCrc, errors, warnings);
    AllSettings::CheckError(allSettings.cbsInodeBlockCount, errors, warnings);
    AllSettings::CheckError(allSettings.cbsInodeTimestamps, errors, warnings);
    AllSettings::CheckError(allSettings.cbsUpdateAtime, errors, warnings);
    AllSettings::CheckError(allSettings.sbsDirectPtrs, errors, warnings);
    AllSettings::CheckError(allSettings.sbsIndirectPtrs, errors, warnings);

    // "Memory" tab
    AllSettings::CheckError(allSettings.sbsAllocatedBuffers, errors, warnings);
    AllSettings::CheckError(allSettings.lesMemcpy, errors, warnings);
    AllSettings::CheckError(allSettings.lesMemmov, errors, warnings);
    AllSettings::CheckError(allSettings.lesMemset, errors, warnings);
    AllSettings::CheckError(allSettings.lesMemcmp, errors, warnings);
    AllSettings::CheckError(allSettings.lesStrlen, errors, warnings);
    AllSettings::CheckError(allSettings.lesStrcmp, errors, warnings);
    AllSettings::CheckError(allSettings.lesStrncmp, errors, warnings);
    AllSettings::CheckError(allSettings.lesStrncpy, errors, warnings);
    AllSettings::CheckError(allSettings.lesInclude, errors, warnings);

    // "Transactions" tab
    AllSettings::CheckError(allSettings.cbsTrManual, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrFileCreat, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrDirCreat, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrRename, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrLink, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrUnlink, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrWrite, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrTruncate, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrFSync, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrClose, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrVolFull, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrUmount, errors, warnings);
    AllSettings::CheckError(allSettings.cbsTrSync, errors, warnings);

    Q_ASSERT(volumeSettings != NULL);
    volumeSettings->GetErrors(errors, warnings);
}

void AllSettings::CheckError(SettingBase *setting,
                QStringList &errors, QStringList &warnings)
{
    Q_ASSERT(setting != NULL);
    QString msg;
    Validity v = setting->RecheckValid(msg);
    switch(v)
    {
        case Invalid:
            errors.append(msg);
            break;
        case Warning:
            warnings.append(msg);
            break;
        default:
            // Nothing to append.
            // Default clause included to supress GCC's Wswitch.
            break;
    }
}

void AllSettings::ParseHeaderToSettings(const QString &text,
                                        QStringList &notFound,QStringList &notParsed)
{
    QRegularExpression inclExp("#include ((\".*\")|(\\<.*\\>))\\s*?\\n");
    QRegularExpressionMatch inclMatch = inclExp.match(text);
    if(inclMatch.hasMatch())
    {
        if(inclMatch.lastCapturedIndex() < 1)
        {
            notParsed += "Included header";
        }
        else
        {
            allSettings.lesInclude->SetValue(inclMatch.captured(1));
        }
    }
    else
    {
        allSettings.lesInclude->SetValue("");
    }

    parseToSetting(text, allSettings.cbsReadonly, notFound, notParsed);
    parseToSetting(text, allSettings.cbsAutomaticDiscards, notFound, notParsed);
    parseToSetting(text, allSettings.rbtnsUsePosix, notFound, notParsed);
    parseToSetting(text, allSettings.rbtnsUseFse, notFound, notParsed);
    parseToSetting(text, allSettings.cbsPosixFormat, notFound, notParsed);
    parseToSetting(text, allSettings.cbsPosixLink, notFound, notParsed);
    parseToSetting(text, allSettings.cbsPosixUnlink, notFound, notParsed);
    parseToSetting(text, allSettings.cbsPosixMkdir, notFound, notParsed);
    parseToSetting(text, allSettings.cbsPosixRmdir, notFound, notParsed);
    parseToSetting(text, allSettings.cbsPosixRename, notFound, notParsed);
    parseToSetting(text, allSettings.cbsPosixAtomicRename, notFound, notParsed);
    parseToSetting(text, allSettings.cbsPosixFtruncate, notFound, notParsed);
    parseToSetting(text, allSettings.cbsPosixDirOps, notFound, notParsed);
    parseToSetting(text, allSettings.cbsPosixCwd, notFound, notParsed);
    parseToSetting(text, allSettings.cbsPosixFstrim, notFound, notParsed);
    parseToSetting(text, allSettings.sbsMaxNameLen, notFound, notParsed);
    parseToSetting(text, allSettings.pssPathSepChar, notFound, notParsed);
    parseToSetting(text, allSettings.sbsTaskCount, notFound, notParsed);
    parseToSetting(text, allSettings.sbsHandleCount, notFound, notParsed);
    parseToSetting(text, allSettings.cbsFseFormat, notFound, notParsed);
    parseToSetting(text, allSettings.cbsFseTruncate, notFound, notParsed);
    parseToSetting(text, allSettings.cbsFseGetMask, notFound, notParsed);
    parseToSetting(text, allSettings.cbsFseSetMask, notFound, notParsed);
    parseToSetting(text, allSettings.cbsDebugEnableOutput, notFound, notParsed);
    parseToSetting(text, allSettings.cbsDebugProcesAsserts, notFound, notParsed);

    // "Volumes" tab
    parseToSetting(text, allSettings.cmisBlockSize, notFound, notParsed);

    // "Data Storage" tab
    parseToEnabledDisabledSetting(text, allSettings.cmssByteOrder, "Big endian", "Little endian",
                                  notFound, notParsed);

    parseToSetting(text, allSettings.cmisNativeAlignment, notFound, notParsed);

    //special case: CRC name must be translated into expected UI string
    bool crcFound;
    QString crcStrValue = findValue(text, allSettings.cmssCrc->GetMacroName(), crcFound);
    if(crcFound && !crcStrValue.isNull())
    {
        if(QString::compare(crcStrValue, crcBitwise) == 0)
        {
            allSettings.cmssCrc->SetValue("Bitwise - smallest, slowest");
        }
        else if(QString::compare(crcStrValue, crcSarwate) == 0)
        {
            allSettings.cmssCrc->SetValue("Sarwate - midsized, fast");
        }
        else if(QString::compare(crcStrValue, crcSlice) == 0)
        {
            allSettings.cmssCrc->SetValue("Slice by 8 - largest, fastest");
        }
        else
        {
            notParsed += macroNameCrc;
        }
    }
    else
    {
        notFound += macroNameCrc;
    }

    parseToSetting(text, allSettings.cbsInodeBlockCount, notFound, notParsed);
    parseToSetting(text, allSettings.cbsInodeTimestamps, notFound, notParsed);
    parseToSetting(text, allSettings.cbsUpdateAtime, notFound, notParsed);
    parseToSetting(text, allSettings.sbsDirectPtrs, notFound, notParsed);
    parseToSetting(text, allSettings.sbsIndirectPtrs, notFound, notParsed);

    // "Memory" tab
    parseToSetting(text, allSettings.sbsAllocatedBuffers, notFound, notParsed);

    // Don't warn on these if not found; they will not be found
    // if use Reliance memory mngmnt fns was selected.
    parseMemSetting(text, allSettings.lesMemcpy);
    parseMemSetting(text, allSettings.lesMemmov);
    parseMemSetting(text, allSettings.lesMemset);
    parseMemSetting(text, allSettings.lesMemcmp);
    parseMemSetting(text, allSettings.lesStrlen);
    parseMemSetting(text, allSettings.lesStrcmp);
    parseMemSetting(text, allSettings.lesStrncmp);
    parseMemSetting(text, allSettings.lesStrncpy);

    // Transaction point settings. Special case: if the transaction
    // name is found within the #define statement, then it is set true.
    // Otherwise it is set false.

    QRegularExpression trSearchExp(QString("#define[ \\t]+")
                                + macroNameTrDefault
                                   //Capture all lines of value
                                + QString("([^\\\\]*(\\\\\\s*[^\\\\]*)*)"));
    QRegularExpressionMatch rem = trSearchExp.match(text);
    if(rem.hasMatch() && rem.lastCapturedIndex() >= 1)
    {
        QString trText = rem.captured(1);

        parseToTrSetting(trText, allSettings.cbsTrManual);
        parseToTrSetting(trText, allSettings.cbsTrFileCreat);
        parseToTrSetting(trText, allSettings.cbsTrDirCreat);
        parseToTrSetting(trText, allSettings.cbsTrRename);
        parseToTrSetting(trText, allSettings.cbsTrLink);
        parseToTrSetting(trText, allSettings.cbsTrUnlink);
        parseToTrSetting(trText, allSettings.cbsTrWrite);
        parseToTrSetting(trText, allSettings.cbsTrTruncate);
        parseToTrSetting(trText, allSettings.cbsTrFSync);
        parseToTrSetting(trText, allSettings.cbsTrClose);
        parseToTrSetting(trText, allSettings.cbsTrVolFull);
        parseToTrSetting(trText, allSettings.cbsTrUmount);
        parseToTrSetting(trText, allSettings.cbsTrSync);
    }
    else //no matches
    {
        notFound += macroNameTrDefault;
    }
}

// Searches for the given setting in the given text. Parses the
// value and loads it into setting. Appends humanName to notFound
// or to notParsed if the setting was not found or could not be
// parsed. Appends the setting's macro name if humanName is not
// specified.
template<typename T>
void parseToSetting(const QString &text, Setting<T> *setting,
                    QStringList &notFound, QStringList &notParsed,
                    const QString &humanName)
{
    Q_ASSERT(setting != NULL);

    QString label = (humanName.isNull() ? setting->GetMacroName() : humanName);
    bool found = false, parseSuccess = false;

    QString strValue = findValue(text, setting->GetMacroName(), found);
    if(found && !strValue.isNull())
    {
        T value;
        parseSuccess = setting->TryParse(strValue, value);
        if(parseSuccess)
        {
            setting->SetValue(value);
        }
        else
        {
            notParsed += label;
        }
    }
    else
    {
        notFound += label;
    }
}

// Searches for the given setting in the given text. If the macro
// is found, its value is parsed and loads it into setting.
// Otherwise the given setting is set to an empty string.
void parseMemSetting(const QString &text, StrSetting *setting)
{
    Q_ASSERT(setting != NULL);
    bool found = false;
    QString strValue = findValue(text, setting->GetMacroName(), found);
    if(found && !strValue.isNull())
    {
        setting->SetValue(strValue);
    }
    else
    {
        setting->SetValue("");
    }
}

// For settings that represent boolean entities with strings.
// Sets setting to strTrue or strFalse if the macro is is found
// in text and can be parsed; otherwise appends humanName to
// notFound or to notParsed. If humanName is not specified, the
// setting's macro name is appended.
void parseToEnabledDisabledSetting(const QString &text, StrSetting *setting,
                                   const QString &strTrue, const QString &strFalse,
                                   QStringList &notFound, QStringList &notParsed,
                                   const QString &humanName)
{
    QString label = (humanName.isNull() ? setting->GetMacroName() : humanName);
    bool found = false;

    QString strValue = findValue(text, setting->GetMacroName(), found);
    if(found && !strValue.isNull())
    {
        if(QString::compare(strValue, "0") == 0
                || QString::compare(strValue, "false", Qt::CaseInsensitive) == 0)
        {
            setting->SetValue(strFalse);
        }
        else if(QString::compare(strValue, "1") == 0
                || QString::compare(strValue, "true", Qt::CaseInsensitive) == 0)
        {
            setting->SetValue(strTrue);
        }
        else
        {
            notParsed += label;
        }
    }
    else
    {
        notFound += label;
    }
}

// Searches for the given setting's macro name in the given text.
// Sets the setting to true if it is found, false otherwise.
void parseToTrSetting(const QString &text, BoolSetting *setting)
{
    Q_ASSERT(setting != NULL);
    QRegularExpression searchExp(setting->GetMacroName());
    QRegularExpressionMatch rem = searchExp.match(text);
    setting->SetValue(rem.hasMatch());
}

// Locates the given macroName in the given text. Sets found to
// true and returns the macro's value if it is found; otherwise
// sets found to false and returns QString::null.
QString findValue(const QString &text, const QString &macroName, bool &found)
{
    QRegularExpression searchExp(QString("#define[ \\t]+")
                                + macroName
                                 // Capture value. Capture group 1 may contain
                                 // more than one word, but will not start or
                                 // end with whitespace.
                                + QString("[ \\t]+(\\S([ \\t]*\\S)*)"));
    QRegularExpressionMatch rem = searchExp.match(text);
    found = rem.hasMatch() && rem.lastCapturedIndex() > 0;
    if(!found)
    {
        return QString::null;
    }
    return rem.captured(1);
}

void AllSettings::ParseCodefileToSettings(const QString &text,
                                          QStringList &notFound,
                                          QStringList &notParsed)
{
    Q_ASSERT(volumeSettings != NULL);
    volumeSettings->ParseCodefile(text, notFound, notParsed);
}

void AllSettings::DeleteAll()
{
    deleteAndNullify(&allSettings.cbsReadonly);
    deleteAndNullify(&allSettings.cbsAutomaticDiscards);
    deleteAndNullify(&allSettings.rbtnsUsePosix);
    deleteAndNullify(&allSettings.rbtnsUseFse);
    deleteAndNullify(&allSettings.cbsPosixFormat);
    deleteAndNullify(&allSettings.cbsPosixLink);
    deleteAndNullify(&allSettings.cbsPosixUnlink);
    deleteAndNullify(&allSettings.cbsPosixMkdir);
    deleteAndNullify(&allSettings.cbsPosixRmdir);
    deleteAndNullify(&allSettings.cbsPosixRename);
    deleteAndNullify(&allSettings.cbsPosixAtomicRename);
    deleteAndNullify(&allSettings.cbsPosixFtruncate);
    deleteAndNullify(&allSettings.cbsPosixDirOps);
    deleteAndNullify(&allSettings.cbsPosixCwd);
    deleteAndNullify(&allSettings.cbsPosixFstrim);
    deleteAndNullify(&allSettings.sbsMaxNameLen);
    deleteAndNullify(&allSettings.pssPathSepChar);
    deleteAndNullify(&allSettings.sbsTaskCount);
    deleteAndNullify(&allSettings.sbsHandleCount);
    deleteAndNullify(&allSettings.cbsFseFormat);
    deleteAndNullify(&allSettings.cbsFseTruncate);
    deleteAndNullify(&allSettings.cbsFseGetMask);
    deleteAndNullify(&allSettings.cbsFseSetMask);
    deleteAndNullify(&allSettings.cbsDebugEnableOutput);
    deleteAndNullify(&allSettings.cbsDebugProcesAsserts);

    // "Volumes" tab (Note: most settings handled by VolumeSettings)
    deleteAndNullify(&allSettings.cmisBlockSize);

    // "Data" tab
    deleteAndNullify(&allSettings.cmssByteOrder);
    deleteAndNullify(&allSettings.cmisNativeAlignment);
    deleteAndNullify(&allSettings.cmssCrc);
    deleteAndNullify(&allSettings.cbsInodeBlockCount);
    deleteAndNullify(&allSettings.cbsInodeTimestamps);
    deleteAndNullify(&allSettings.cbsUpdateAtime);
    deleteAndNullify(&allSettings.sbsDirectPtrs);
    deleteAndNullify(&allSettings.sbsIndirectPtrs);

    // "Memory" tab
    deleteAndNullify(&allSettings.sbsAllocatedBuffers);
    deleteAndNullify(&allSettings.lesMemcpy);
    deleteAndNullify(&allSettings.lesMemmov);
    deleteAndNullify(&allSettings.lesMemset);
    deleteAndNullify(&allSettings.lesMemcmp);
    deleteAndNullify(&allSettings.lesStrlen);
    deleteAndNullify(&allSettings.lesStrcmp);
    deleteAndNullify(&allSettings.lesStrncmp);
    deleteAndNullify(&allSettings.lesStrncpy);
    deleteAndNullify(&allSettings.lesInclude);

    // "Transactions" tab
    deleteAndNullify(&allSettings.cbsTrManual);
    deleteAndNullify(&allSettings.cbsTrFileCreat);
    deleteAndNullify(&allSettings.cbsTrDirCreat);
    deleteAndNullify(&allSettings.cbsTrRename);
    deleteAndNullify(&allSettings.cbsTrLink);
    deleteAndNullify(&allSettings.cbsTrUnlink);
    deleteAndNullify(&allSettings.cbsTrWrite);
    deleteAndNullify(&allSettings.cbsTrTruncate);
    deleteAndNullify(&allSettings.cbsTrFSync);
    deleteAndNullify(&allSettings.cbsTrClose);
    deleteAndNullify(&allSettings.cbsTrVolFull);
    deleteAndNullify(&allSettings.cbsTrUmount);
    deleteAndNullify(&allSettings.cbsTrSync);
}


const QString macroNameReadonly = "REDCONF_READ_ONLY";
const QString macroNameAutomaticDiscards = "REDCONF_DISCARDS";
const QString macroNameUsePosix = "REDCONF_API_POSIX";
const QString macroNameUseFse = "REDCONF_API_FSE";
const QString macroNamePosixFormat = "REDCONF_API_POSIX_FORMAT";
const QString macroNamePosixLink = "REDCONF_API_POSIX_LINK";
const QString macroNamePosixUnlink = "REDCONF_API_POSIX_UNLINK";
const QString macroNamePosixMkdir = "REDCONF_API_POSIX_MKDIR";
const QString macroNamePosixRmdir = "REDCONF_API_POSIX_RMDIR";
const QString macroNamePosixRename = "REDCONF_API_POSIX_RENAME";
const QString macroNamePosixRenameAtomic = "REDCONF_RENAME_ATOMIC";
const QString macroNamePosixFtruncate = "REDCONF_API_POSIX_FTRUNCATE";
const QString macroNamePosixDirOps = "REDCONF_API_POSIX_READDIR";
const QString macroNamePosixCwd = "REDCONF_API_POSIX_CWD";
const QString macroNamePosixFstrim = "REDCONF_API_POSIX_FSTRIM";
const QString macroNameMaxNameLen = "REDCONF_NAME_MAX";
const QString macroNamePathSepChar = "REDCONF_PATH_SEPARATOR";
const QString macroNameTaskCount = "REDCONF_TASK_COUNT";
const QString macroNameHandleCount = "REDCONF_HANDLE_COUNT";
const QString macroNameFseFormat = "REDCONF_API_FSE_FORMAT";
const QString macroNameFseTruncate = "REDCONF_API_FSE_TRUNCATE";
const QString macroNameFseGetMask = "REDCONF_API_FSE_TRANSMASKGET";
const QString macroNameFseSetMask = "REDCONF_API_FSE_TRANSMASKSET";
const QString macroNameDebugEnableOutput = "REDCONF_OUTPUT";
const QString macroNameDebugProcesAsserts = "REDCONF_ASSERTS";

// "Volumes" tab
const QString macroNameBlockSize = "REDCONF_BLOCK_SIZE";
const QString macroNameVolumeCount = "REDCONF_VOLUME_COUNT";

// "Data" tab
const QString macroNameByteOrder = "REDCONF_ENDIAN_BIG";
const QString macroNameNativeAlignment = "REDCONF_ALIGNMENT_SIZE";
const QString macroNameCrc = "REDCONF_CRC_ALGORITHM";
const QString macroNameInodeCount = "REDCONF_INODE_BLOCKS";
const QString macroNameInodeTimestamps = "REDCONF_INODE_TIMESTAMPS";
const QString macroNameUpdateAtime = "REDCONF_ATIME";
const QString macroNameDirectPtrs = "REDCONF_DIRECT_POINTERS";
const QString macroNameIndirectPtrs = "REDCONF_INDIRECT_POINTERS";

// Not in UI
const QString macroNameInlineImap = "REDCONF_IMAP_INLINE";
const QString macroNameExternalImap = "REDCONF_IMAP_EXTERNAL";

// "Memory" tab
const QString macroNameAllocatedBuffers = "REDCONF_BUFFER_COUNT";
const QString macroNameMemcpy = "RedMemCpyUnchecked";
const QString macroNameMemmov = "RedMemMoveUnchecked";
const QString macroNameMemset = "RedMemSetUnchecked";
const QString macroNameMemcmp = "RedMemCmpUnchecked";
const QString macroNameStrlen = "RedStrLenUnchecked";
const QString macroNameStrcmp = "RedStrCmpUnchecked";
const QString macroNameStrncmp = "RedStrNCmpUnchecked";
const QString macroNameStrncpy = "RedStrNCpyUnchecked";

// "Transactions" tab
const QString macroNameTrDefault = "REDCONF_TRANSACT_DEFAULT"; //Not in UI
const QString macroNameTrManual = "RED_TRANSACT_MANUAL";
const QString macroNameTrFileCreat = "RED_TRANSACT_CREAT";
const QString macroNameTrDirCreat = "RED_TRANSACT_MKDIR";
const QString macroNameTrRename = "RED_TRANSACT_RENAME";
const QString macroNameTrLink = "RED_TRANSACT_LINK";
const QString macroNameTrUnlink = "RED_TRANSACT_UNLINK";
const QString macroNameTrWrite = "RED_TRANSACT_WRITE";
const QString macroNameTrTruncate = "RED_TRANSACT_TRUNCATE";
const QString macroNameTrFSync = "RED_TRANSACT_FSYNC";
const QString macroNameTrClose = "RED_TRANSACT_CLOSE";
const QString macroNameTrVolFull = "RED_TRANSACT_VOLFULL";
const QString macroNameTrUmount = "RED_TRANSACT_UMOUNT";
const QString macroNameTrSync = "RED_TRANSACT_SYNC";

// Mem & str management function names
const QString cstdMemcpy = "memcpy";
const QString cstdMemmov = "memmove";
const QString cstdMemset = "memset";
const QString cstdMemcmp = "memcmp";
const QString cstdStrlen = "strlen";
const QString cstdStrcmp = "strcmp";
const QString cstdStrncmp = "strncmp";
const QString cstdStrncpy = "strncpy";
const QString cstdStringH = "<string.h>";

const QString crcBitwise = "CRC_BITWISE";
const QString crcSarwate = "CRC_SARWATE";
const QString crcSlice = "CRC_SLICEBY8";

// Extern global allSettings instance
AllSettings allSettings;
