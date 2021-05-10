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
/// \brief  This header file declares all of the validation functions used for
///         the settings.
///
#ifndef VALIDATORS_H
#define VALIDATORS_H

#include <QString>

#include "validity.h"

Validity emptyBoolValidator(bool value, QString &msg);
Validity emptyIntValidator(unsigned long value, QString &msg);
Validity emptyStringValidator(QString value, QString &msg);

Validity validateUsePosixApi(bool value, QString &msg);
Validity validateUseFseApi(bool value, QString &msg);

Validity validateAutomaticDiscards(bool value, QString &msg);
Validity validatePosixFstrim(bool value, QString &msg);

Validity validateMaxNameLen(unsigned long value, QString &msg);
Validity validatePathSepChar(QString value, QString &msg);
Validity validateTaskCount(unsigned long value, QString &msg);
Validity validateHandleCount(unsigned long value, QString &msg);

Validity validateBlockSize(unsigned long value, QString &msg);
Validity validateVolIoRetries(unsigned long value, QString &msg);
Validity validateVolumeCount(unsigned long value, QString &msg);

Validity validateVolName(QString value, QString &msg);
Validity validateVolSectorSize(unsigned long value, QString &msg);
Validity validateVolSectorCount(unsigned long value, QString &msg);
Validity validateVolSectorOff(unsigned long value, QString &msg);
Validity validateVolInodeCount(unsigned long value, QString &msg);
Validity validateSupportedUnsupported(QString value, QString &msg);
Validity validateDiscardSupport(QString value, QString &msg);

Validity validateByteOrder(QString value, QString &msg);
Validity validateAlignmentSize(unsigned long value, QString &msg);
Validity validateCrc(QString value, QString &msg);
Validity validateInodeBlockCount(bool value, QString &msg);
Validity validateInodeTimestamps(bool value, QString &msg);
Validity validateDirectPointers(unsigned long value, QString &msg);
Validity validateIndirectPointers(unsigned long value, QString &msg);

Validity validateAllocatedBuffers(unsigned long value, QString &msg);

Validity validateMemInclude(QString value, QString &msg);

Validity validateTransactManual(bool value, QString &msg);
Validity validateTransactVolFull(bool value, QString &msg);

///
/// \brief  Gets the value of INODE_ENTRIES. Used in some validators and in
///         updating the max file size.
///
unsigned long getInodeEntries();

///
/// \brief  Gets the maximum supported volume size in bytes based on current
///         settings.
///
qulonglong getVolSizeMaxBytes();

#endif // VALIDATORS_H
