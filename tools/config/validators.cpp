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
///
/// \file validators.h
/// \brief  This header file defines all of the validation functions used for
///         the settings.
///

#include <QList>

#include "volumesettings.h"
#include "allsettings.h"
#include "validators.h"


static bool isPowerOfTwo(unsigned long value);


///
/// \brief  Returns Valid no matter what is passed in.
///
Validity emptyBoolValidator(bool value, QString &msg)
{
    (void)value;
    (void)msg;
    return Valid;
}

///
/// \brief  Returns Valid no matter what is passed in.
///
Validity emptyIntValidator(unsigned long value, QString &msg)
{
    (void)value;
    (void)msg;
    return Valid;
}

///
/// \brief  Returns Valid no matter what is passed in.
///
Validity emptyStringValidator(QString value, QString &msg)
{
    (void)value;
    (void)msg;
    return Valid;
}

///
/// \brief  Validator for allSettings::cbAutomaticDiscards.
///
///         Requires that ::allSettings and ::volumeSettings be initialized.
///
///         Side effect: calls ::volumeSettings->SetDiscardsEnabled.
///
Validity validateAutomaticDiscards(bool value, QString &msg)
{
    Q_ASSERT(allSettings.cbsAutomaticDiscards != NULL);

    if(!volumeSettings->GetDiscardsSupported() && value)
    {
        msg = QString("None of the defined volumes support discards.");
        return Invalid;
    }

    return Valid;
}

///
/// \brief  Validator for allSettings::cbPosixFstrim.
///
///         Requires that ::allSettings and ::volumeSettings be initialized.
///
///         Side effect: calls ::volumeSettings->SetDiscardsEnabled.
///
Validity validatePosixFstrim(bool value, QString &msg)
{
    Q_ASSERT(allSettings.cbsPosixFstrim != NULL);

    if(!volumeSettings->GetDiscardsSupported() && value)
    {
        msg = QString("None of the defined volumes support discards.");
        return Invalid;
    }

    return Valid;
}

///
/// \brief  Validator for allSettings::rbtnsUsePosix.
///
///         Requires that ::allSettings and ::volumeSettings be initialized.
///
///         Side effect: calls ::volumeSettings->SetUsePosix.
///
Validity validateUsePosixApi(bool value, QString &msg)
{
    Q_ASSERT(allSettings.rbtnsUseFse != NULL);
    if(allSettings.rbtnsUseFse->GetValue() == value)
    {
        // At least on Windows, this happens when the rbtnUseFse is clicked:
        // this value is changed to false before the Use FSE value is changed
        // to true. This may be inverted, but either way the first
        // validateUseXxxApi function will return Invalid and the second will
        // return Valid in normal circumstances.
        msg = "One API (POSIX or FSE) must be chosen but not both.";
        return Invalid;
    }

    Q_ASSERT(volumeSettings != NULL);

    // Following above comment, when the user selects a radio button this
    // function call will be reached either here or in validateUseFseApi but not
    // in both places, since the first called will return early above.
    volumeSettings->SetUsePosix(value);

    return Valid;
}

///
/// \brief  Validator for allSettings::rbtnsUseFse.
///
///         Requires that ::allSettings and ::volumeSettings be initialized.
///
///         Side effect: calls ::volumeSettings->SetUsePosix.
///
Validity validateUseFseApi(bool value, QString &msg)
{
    Q_ASSERT(allSettings.rbtnsUsePosix != NULL);
    if(allSettings.rbtnsUsePosix->GetValue() == value)
    {
        // See comments in validateUsePosixApi
        msg = "One API (POSIX or FSE) must be chosen but not both.";
        return Invalid;
    }

    Q_ASSERT(volumeSettings != NULL);
    volumeSettings->SetUsePosix(!value);

    return Valid;
}

///
/// \brief  Validator for allSettings::sbsMaxNameLen.
///
///         Requires that ::allSettings be initialized.
///
Validity validateMaxNameLen(unsigned long value, QString &msg)
{
    if(value < 1)
    {
        msg = "Max name length must be greater than 0.";
        return Invalid;
    }

    Q_ASSERT(allSettings.cmisBlockSize != NULL);
    if(value > allSettings.cmisBlockSize->GetValue() - 4)
    {
        msg = "Max name length must be at least 4 lower than block size.";
        return Invalid;
    }

    if(value % 4 != 0)
    {
        msg = "Recommended: set name length maximum to a multiple of 4.";
        return Warning;
    }

    return Valid;
}

///
/// \brief  Validator for allSettings::pssPathSepChar.
///
Validity validatePathSepChar(QString value, QString &msg)
{
    if(value.isNull() || value.isEmpty())
    {
        msg = "Path separator character cannot be empty.";
        return Invalid;
    }

    if(value.length() == 1)
    {
        char c = value[0].toLatin1();
        if(c <= 0 || c >= 127)
        {
            msg = "Path separator charactor must be a standard ASCII character.";
            return Invalid;
        }
        return Valid;
    }

    if(value.startsWith('\\'))
    {
        if(value.length() == 2 &&
                (value[1] == '\\'
                || value[1] == 'a'
                || value[1] == 'b'
                || value[1] == 'f'
                || value[1] == 'n'
                || value[1] == 'r'
                || value[1] == 't'
                || value[1] == 'v'
                || value[1] == '"'
                || value[1] == '\''
                || value[1] == '?'))
        {
            return Valid; //Valid escape char
        }

        int convertBase = 8;
        int convertFrom = 1; //First expected numerical char
        if(value[1] == 'x')
        {
            convertBase = 16;
            convertFrom = 2;
        }
        bool success = false;
        QString escapeSeq = value.mid(convertFrom, value.length() - 1);
        unsigned short charCode = escapeSeq.toUShort(&success, convertBase);

        if(!success || charCode > 127)
        {
            msg = "Invalid escape sequence for path separator.";
            return Invalid;
        }

        if(charCode == 0)
        {
            msg = "Null character not valid for path separator.";
            return Invalid;
        }

        return Valid;
    }

    msg = "Invalid character sequence. Must be single character or valid escape sequence.";
    return Invalid;
}

///
/// \brief  Validator for allSettings::sbsTaskCount
///
///         Requires that ::allSettings be initialized.
///
Validity validateTaskCount(unsigned long value, QString &msg)
{
    if(value < 1)
    {
        msg = "Task count must be greater than 0.";
        return Invalid;
    }

    return Valid;
}
///
/// \brief  Validator for allSettings::sbsHandleCount
///
Validity validateHandleCount(unsigned long value, QString &msg)
{
    if(value < 1 || value > 4096)
    {
        msg = "Handle count must be between 1 and 4096.";
        return Invalid;
    }

    return Valid;
}
///
/// \brief  Validator for allSettings::cmisBlockSize
///
Validity validateBlockSize(unsigned long value, QString &msg)
{
    if(value < 128 || value > 65536)
    {
        msg = "Block size must be a power of 2 between 128 and 65536.";
        return Invalid;
    }

    if(!isPowerOfTwo(value))
    {
        msg = "Block size must be a power of 2.";
        return Invalid;
    }

    return Valid;
}

///
/// \brief  Validator for allSettings::sbsBlockIoRetries
///
Validity validateVolIoRetries(unsigned long value, QString &msg)
{
    if(value > 254)
    {
        msg = "Block I/O retries cannot be higher than 254.";
        return Invalid;
    }

    return Valid;
}

///
/// \brief  Validator for the number of volumes added in the <i>Volumes</i> tab
///
Validity validateVolumeCount(unsigned long value, QString &msg)
{
    if(value > 255)
    {
        msg = "No more than 255 volumes are allowed.";
        return Invalid;
    }

    if(value == 0)
    {
        msg = "At least one volume must be created.";
        return Invalid;
    }

    return Valid;
}

///
/// \brief  Validator for a VolumeSettings::Volume::stName
///
///         Requires that ::allSettings be initialized.
///
///         Returns Valid regardless of \p value if ::volumeSettings is not
///         initialized. This is necessary because the volume name is checked
///         for validity during the initialization of ::volumeSettings.
///
///         This validator checks \p value against the names of other volumes.
///         It assumes that \p value is the name of the volume at the active
///         index in ::volumeSettings. If a volume name that is not at the
///         active index is checked with this validator, it will be checked
///         against itself and reported invalid because it appears to be a
///         duplicate. To avoid this, ensure volumeSettings::activeIndex refers
///         to the volume whose name is to be validated.
///
Validity validateVolName(QString value, QString &msg)
{
    Q_ASSERT(allSettings.rbtnsUsePosix != NULL);

    if(!allSettings.rbtnsUsePosix->GetValue())
    {
        return Valid; //Vol name doesn't matter in FSE
    }

    if(value.isNull())
    {
        Q_ASSERT(false); //Not expected
        msg = "Volume name cannot be null.";
        return Invalid;
    }

    for(int i = 0; i < value.count(); i++)
    {
        if(value[i] == '\n' || value[i] == '\r')
        {
            msg = "Unexpected new line in volume name. Try using an escape sequence instead.";
            return Invalid;
        }
    }

    // This evaluates to true when this function is called
    // during the instantiation of volumeSettings.
    if(volumeSettings == NULL)
    {
        PRINTDBG(QString("NULL volumeSettings; cannot validate: ") + value);
        return Valid; //Assume that initial value is valid
    }

    int ignoreIndex = volumeSettings->GetCurrentIndex();
    QList<VolumeSettings::Volume *> * const volumes
            = volumeSettings->GetVolumes();

    // Check for duplicates to this name
    for(int i = 0; i < volumes->count(); i++)
    {
        if(i == ignoreIndex) continue;
        if((*volumes)[i]->GetStName()->GetValue() == value)
        {
            msg = QString("Volume name must be unique. Duplicate volume name ")
                    + value
                    + QString(".");
            return Invalid;
        }
    }

    return Valid;
}

///
/// \brief  Validator for a VolumeSettings::Volume::stSectorSize
///
///         Requires that ::allSettings be initialized.
///
Validity validateVolSectorSize(unsigned long value, QString &msg)
{
    Q_ASSERT(allSettings.cmisBlockSize != NULL);
    if(!isPowerOfTwo(value)
            || value < 128
            || value > 65536) // 2^16. Same max in block size
    {
        msg = "Sector size must be a power of 2 between 128 and 65536.";
        return Invalid;
    }

    if(value > allSettings.cmisBlockSize->GetValue())
    {
        msg = "Sector size cannot be larger than block size.";
        return Invalid;
    }

    return Valid;
}

///
/// \brief  Validator for a VolumeSettings::Volume::stSectorCount.
///
///         Assumes the validator is being run on the volume at
///         volumeSettings::activeIndex. Requires ::allSettings and
///         ::volumeSettings be initialized.
///
Validity validateVolSectorCount(unsigned long value, QString &msg)
{
    VolumeSettings::Volume *currVolume = volumeSettings->GetVolumes()->at(volumeSettings->GetCurrentIndex());
	bool isAutoSize = currVolume->IsAutoSectorSize();
    unsigned long sectorSize = currVolume->GetStSectorSize()->GetValue();
    unsigned long blockSize = allSettings.cmisBlockSize->GetValue();

    // Avoid division by 0
    if(blockSize == 0 || (!isAutoSize && blockSize < sectorSize))
    {
        msg = "Invalid block or sector size; cannot validate volume size.";
        return Warning;
    }

    if(isAutoSize)
    {
        if(value < 5)
        {
            msg = "Volume must be the size of at least 5 sectors.";
            return Invalid;
        }
    }
    else
    {
        if((value / (blockSize / sectorSize)) < 5)
        {
            msg = "Volume must be the size of at least 5 blocks.";
            return Invalid;
        }

        Q_ASSERT(volumeSettings != NULL);
        qulonglong maxSectors = getVolSizeMaxBytes() / sectorSize;

        if(static_cast<qulonglong>(value) > maxSectors)
        {
            msg = "Volume size too large. Try selecting a higher block size. See Info tab for limits.";
            return Invalid;
        }
    }

    return Valid;
}

///
/// \brief  Validator for a VolumeSettings::Volume::stSectorOff.
///
///         Assumes the validator is being run on the volume at
///         volumeSettings::activeIndex. Requires ::allSettings and
///         ::volumeSettings be initialized.
///
Validity validateVolSectorOff(unsigned long value, QString &msg)
{
    (void)value;
    VolumeSettings::Volume *currVolume = volumeSettings->GetVolumes()->at(volumeSettings->GetCurrentIndex());
	bool isAutoSize = currVolume->IsAutoSectorSize();
    unsigned long sectorSize = currVolume->GetStSectorSize()->GetValue();
    unsigned long blockSize = allSettings.cmisBlockSize->GetValue();

    // Avoid division by 0
    if(blockSize == 0 || (!isAutoSize && blockSize < sectorSize))
    {
        msg = "Invalid block or sector size; cannot validate volume offset.";
        return Warning;
    }
	else
	{
		return Valid;
	}
}

///
/// \brief  Validator for a VolumeSettings::Volume::stInodeCount
///
///         Assumes the validator is being run on the volume at
///         volumeSettings::activeIndex. Requires ::allSettings and
///         ::volumeSettings be initialized.
///
Validity validateVolInodeCount(unsigned long value, QString &msg)
{
    if(value < 1)
    {
        msg = "Inode count must be 1 or above.";
        return Invalid;
    }

    VolumeSettings::Volume *currVolume = volumeSettings->GetVolumes()->at(volumeSettings->GetCurrentIndex());
	bool isAutoSize = currVolume->IsAutoSectorSize();
	bool isAutoCount = currVolume->IsAutoSectorCount();

    bool imapExternal = currVolume->NeedsExternalImap();
    unsigned long sectorSize = currVolume->GetStSectorSize()->GetValue();
    unsigned long sectorCount = currVolume->GetStSectorCount()->GetValue();
    unsigned long blockSize = allSettings.cmisBlockSize->GetValue();

    // Avoid division by 0
    if(blockSize == 0 || (!isAutoSize && blockSize < sectorSize))
    {
        msg = "Invalid block or sector size; cannot validate inode count.";
        return Warning;
    }

    if(!isAutoCount && !isAutoSize)
    {
        unsigned long blockCount = sectorCount / (blockSize / sectorSize);

        // Number of bits in a block after subtracting the node header size.
        unsigned long imapnodeEntries = (blockSize - 16) * 8;

        unsigned long inodeTableStartBN; // BN = Block Number

        if(imapExternal)
        {
            int imapNodeCount = 1;
            while(((blockCount - 3) - (imapNodeCount * 2)) > (imapnodeEntries * imapNodeCount))
            {
                imapNodeCount++;
            }

            // Imap start block number + number of imap blocks
            inodeTableStartBN = 3 + (imapNodeCount * 2);
        }
        else
        {
            inodeTableStartBN = 3;
        }

        // Number of inode blocks past starting point must fit within block count
        if((inodeTableStartBN + value * 2) > blockCount)
        {
            unsigned long currMax = ((long)blockCount - (long)inodeTableStartBN > 0 ?
                                     (blockCount - inodeTableStartBN) / 2 : 0);

            msg = QString("Inode count too high; limited by sector count. Current max: ")
                    + QString::number(currMax)
                    + QString(".");
            return Invalid;
        }
    }

    return Valid;
}

///
/// \brief  Validator for settings which should be either gpszSupported
///         or gpszUnsupported.
///
/// Used for VolumeSettings::Volume::stAtomicWrite
///
Validity validateSupportedUnsupported(QString value, QString &msg)
{
    if(QString::compare(value, gpszSupported, Qt::CaseInsensitive) == 0
            || QString::compare(value, gpszUnsupported, Qt::CaseInsensitive) == 0)
    {
        return Valid;
    }
    else
    {
        Q_ASSERT(false); // The associated QComboBox should not allow this.
        msg = "Expected setting to be either \"Supported\" or \"Unsupported\".";
        return Invalid;
    }
}

///
/// \brief  Validator for VolumeSettings::Volume::stDiscardSupport
///
Validity validateDiscardSupport(QString value, QString &msg)
{
    if(QString::compare(value, gpszSupported, Qt::CaseInsensitive) == 0)
    {
        msg = "Discards are only supported by the commercial version of Reliance Edge.";
        return Warning;
    }
    else if(QString::compare(value, gpszUnsupported, Qt::CaseInsensitive) == 0)
    {
        return Valid;
    }
    else
    {
        Q_ASSERT(false); // The associated QComboBox should not allow this.
        msg = "Expected setting to be either \"Supported\" or \"Unsupported\".";
        return Invalid;
    }
}

///
/// \brief  Validator for allSettings::cmssByteOrder
///
Validity validateByteOrder(QString value, QString &msg)
{
    if(value.startsWith("big", Qt::CaseInsensitive)
        || value.startsWith("little", Qt::CaseInsensitive))
    {
        return Valid;
    }
    else
    {
        Q_ASSERT(false); // The associated QComboBox should not allow this.
        msg = "Byte order must be either big endian or little endian.";
        return Invalid;
    }

}

///
/// \brief  Validator for allSettings::sbsNativeAlignment
///
Validity validateAlignmentSize(unsigned long value, QString &msg)
{
    if(value != 1 && value != 2 && value != 4 && value != 8)
    {
        msg = "Alignment size must be power of two between 1 and 8.";
        return Invalid;
    }

    return Valid;
}

///
/// \brief  Validator for allSettings::cmssCrc
///
Validity validateCrc(QString value, QString &msg)
{
    if(QString::compare(value, crcBitwise) == 0
            || QString::compare(value, crcSarwate) == 0
            || QString::compare(value, crcSlice) == 0)
    {
        //Actual values
        return Valid;
    }
    else if(value.startsWith("bitwise", Qt::CaseInsensitive)
            || value.startsWith("sarwate", Qt::CaseInsensitive)
            || value.startsWith("slice by 8", Qt::CaseInsensitive))
    {
        //UI values
        return Valid;
    }
    else
    {
        Q_ASSERT(false); // The associated QComboBox should not allow this.
        msg = "CRC must be one of CRC_BITWISE, CRC_SARWATE, or CRC_SLICEBY8.";
        return Invalid;
    }
}

///
/// \brief  Validator for allSettings::sbsDirectPtrs
///
///         Requires that ::allSettings be initialized.
///
Validity validateInodeBlockCount(bool value, QString &msg)
{
    Q_ASSERT(allSettings.rbtnsUsePosix != NULL);

    if(value && !allSettings.rbtnsUsePosix->GetValue())
    {
        msg = "The inode block count is not accessible from the File System Essentials API.";
        return Warning;
    }

    return Valid;
}

///
/// \brief  Validator for allSettings::sbsDirectPtrs
///
///         Requires that ::allSettings be initialized.
///
Validity validateInodeTimestamps(bool value, QString &msg)
{
    Q_ASSERT(allSettings.rbtnsUsePosix != NULL);

    if(value && !allSettings.rbtnsUsePosix->GetValue())
    {
        msg = "Timestamps are not accessible from the File System Essentials API.";
        return Warning;
    }

    return Valid;
}

///
/// \brief  Validator for allSettings::sbsDirectPtrs
///
///         Requires that ::allSettings be initialized.
///
Validity validateDirectPointers(unsigned long value, QString &msg)
{
    Q_ASSERT(allSettings.sbsIndirectPtrs != NULL);
    unsigned long inodeEntries = getInodeEntries();

    if(allSettings.sbsIndirectPtrs->GetValue() > inodeEntries)
    {
        msg = "Too many direct and indirect pointers.";
        return Invalid;
    }

    if(value > inodeEntries - allSettings.sbsIndirectPtrs->GetValue())
    {
        msg = QString("Too many direct pointers. Current maximum based on other settings: ")
                + QString::number(inodeEntries - allSettings.sbsIndirectPtrs->GetValue())
                + QString(".");
        return Invalid;
    }

    return Valid;
}

///
/// \brief  Validator for allSettings::sbsIndirectPtrs
///
///         Requires that ::allSettings be initialized.
///
Validity validateIndirectPointers(unsigned long value, QString &msg)
{
    Q_ASSERT(allSettings.sbsDirectPtrs != NULL);
    unsigned long inodeEntries = getInodeEntries();

    if(allSettings.sbsDirectPtrs->GetValue() > inodeEntries)
    {
        msg = "Too many direct and indirect pointers.";
        return Invalid;
    }

    if(value > inodeEntries - allSettings.sbsDirectPtrs->GetValue())
    {
        msg = QString("Too many indirect pointers. Current maximum based on other settings: ")
                + QString::number(inodeEntries - allSettings.sbsDirectPtrs->GetValue())
                + QString(".");
        return Invalid;
    }

    return Valid;
}

///
/// \brief  Validator for allSettings::sbsAllocatedBuffers
///
///         Requires that ::allSettings and ::volumeSettings be initialized.
///
Validity validateAllocatedBuffers(unsigned long value, QString &msg)
{
    if(value > 255)
    {
        msg = "Buffer count must be less than 256.";
        return Invalid;
    }

    // Min buffer algorithm derrived from preprocessor logic in buffer.c

    ulong dindirPointers = (getInodeEntries()
                            - allSettings.sbsDirectPtrs->GetValue())
                           - allSettings.sbsIndirectPtrs->GetValue();

    ulong inodeMetaBuffers;

    if(dindirPointers > 0)
    {
        inodeMetaBuffers = 3;
    }
    else if(allSettings.sbsIndirectPtrs->GetValue() > 0)
    {
        inodeMetaBuffers = 2;
    }
    else
    {
        Q_ASSERT(allSettings.sbsDirectPtrs->GetValue() == getInodeEntries());
        inodeMetaBuffers = 1;
    }

    ulong inodeBuffers = inodeMetaBuffers + 1;

    bool imapInline, imapExternal;
    volumeSettings->GetImapRequirements(imapInline, imapExternal);

    ulong imapBuffers = (imapExternal ? 1 : 0);

    ulong minimumBufferCount;

    if(allSettings.cbsReadonly->GetValue()
            || !allSettings.rbtnsUsePosix->GetValue())
    {
        minimumBufferCount = inodeBuffers + imapBuffers;
    }
    else if(allSettings.cbsPosixRename->GetValue())
    {
        if(allSettings.cbsPosixAtomicRename->GetValue())
        {
            minimumBufferCount = inodeBuffers*2 + 3 + imapBuffers;
        }
        else
        {
            minimumBufferCount = inodeBuffers*2 + 2 + imapBuffers;
        }
    }
    else //POSIX, but no rename
    {
        minimumBufferCount = inodeBuffers + 1 + imapBuffers;
    }

    if(value < minimumBufferCount)
    {
        if(allSettings.cbsPosixRename->GetValue())
        {
            msg = QString("Too few allocated buffers. Try disabling POSIX rename or increasing buffer count.");
        }
        else
        {
            msg = QString("Too few allocated buffers. Current minimum based on other settings: ")
                    + QString::number(minimumBufferCount)
                    + QString(".");
        }
        return Invalid;
    }

    return Valid;
}

///
/// \brief  Validator for allSettings::lesInclude
///
Validity validateMemInclude(QString value, QString &msg)
{
    if(value.isEmpty())
    {
        return Valid;
    }
    if((value[0] == '<' && value[value.length() - 1] == '>')
        || (value[0] == '"' && value[value.length() - 1] == '"'))
    {
        return Valid;
    }
    else
    {
        msg = "Invalid include file format: must be enclosed in quotation marks or angle brackets.";
        return Invalid;
    }
}

///
/// \brief  Validator for allSettings::cbsTrManual
///
///         Requires that ::allSettings be initialized.
///
Validity validateTransactManual(bool value, QString &msg)
{
    Q_ASSERT(allSettings.cbsReadonly != NULL);
    if(value && !allSettings.cbsReadonly->GetValue())
    {
        msg = "Automatic transaction on volume full recommended except in special cases.";
        return Warning;
    }
    return Valid;
}

///
/// \brief  Validator for allSettings::cbsTrVolFull
///
///         Requires that ::allSettings be initialized.
///
Validity validateTransactVolFull(bool value, QString &msg)
{
    Q_ASSERT(allSettings.cbsTrManual != NULL);
    Q_ASSERT(allSettings.cbsReadonly != NULL);
    if(!value && !allSettings.cbsTrManual->GetValue()
            && !allSettings.cbsReadonly->GetValue())
    {
        msg = "Automatic transaction on volume full recommended except in special cases.";
        return Warning;
    }
    return Valid;
}


//Private method used to validate [in]direct pointer count
unsigned long getInodeEntries()
{
    Q_ASSERT(allSettings.cbsInodeBlockCount != NULL);
    Q_ASSERT(allSettings.cbsInodeTimestamps != NULL);
    Q_ASSERT(allSettings.rbtnsUsePosix != NULL);
    Q_ASSERT(allSettings.cmisBlockSize != NULL);

    unsigned long inodeHeaderSize = 16 + 8
            + (allSettings.cbsInodeBlockCount->GetValue() ? 4 : 0)
            + (allSettings.cbsInodeTimestamps->GetValue() ? 12 : 0)
            + 4
            + (allSettings.rbtnsUsePosix->GetValue() ? 4 : 0);

    return (allSettings.cmisBlockSize->GetValue() - inodeHeaderSize) / 4;
}

qulonglong getVolSizeMaxBytes()
{
    Q_ASSERT(allSettings.cmisBlockSize != NULL);

    qlonglong blockSize = allSettings.cmisBlockSize->GetValue();
    bool posix = allSettings.rbtnsUsePosix->GetValue();

    qlonglong mrHeader = 28 + (posix ? 4 : 0);
    qlonglong mrImapBits = (static_cast<qlonglong>(blockSize) - mrHeader) * 8;
    qlonglong imapBits = (static_cast<qlonglong>(blockSize) - 16) * 8;

    qulonglong imapMax = mrImapBits * imapBits * static_cast<qulonglong>(blockSize);

    if(mrImapBits < 0 || imapBits < 0)
    {
        imapMax = 0;
    }

    qulonglong blockMax_32bit = 0xFFFFFFFFULL * static_cast<qlonglong>(blockSize);

    return (blockMax_32bit < imapMax) ? blockMax_32bit : imapMax;
}

// Checks whether given value is a power of 2.
// Returns true for all powers of 2; false otherwise.
// Returns false for 0 as 0 is not considered to be
// a power of two.
//
// Code based on Stack Overflow: http://stackoverflow.com/questions/600293
static bool isPowerOfTwo(unsigned long value)
{
    return (value != 0) && ((value & (value - 1)) == 0);
}
