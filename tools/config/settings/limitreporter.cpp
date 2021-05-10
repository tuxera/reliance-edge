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
#include "allsettings.h"
#include "validators.h"
#include "volumesettings.h"
#include "limitreporter.h"

LimitReporter::LimitReporter(QLabel *fsizeMaxLabel, QLabel *vsizeMaxLabel)
    : labelMaxFsize(fsizeMaxLabel),
      labelMaxVsize(vsizeMaxLabel)
{
    // Assert this one, assume the others.
    Q_ASSERT(allSettings.cmisBlockSize != NULL);

    allSettings.cbsInodeBlockCount->notifyList.append(this);
    allSettings.cbsInodeTimestamps->notifyList.append(this);
    allSettings.rbtnsUsePosix->notifyList.append(this);
    allSettings.cmisBlockSize->notifyList.append(this);
    allSettings.sbsDirectPtrs->notifyList.append(this);
    allSettings.sbsIndirectPtrs->notifyList.append(this);

    // LimitReporter is deleted after allSettings, so there is
    // no need to remove these on destruction.

    updateLimits();
}

void LimitReporter::Notify()
{
    updateLimits();
}

// Calculates upper limits of file size and volume size. Equations extracted
// from Reliance_Edge_Limits.xlsx.
void LimitReporter::updateLimits()
{
    Q_ASSERT(allSettings.cmisBlockSize != NULL);

    unsigned long dirPointers = allSettings.sbsDirectPtrs->GetValue();
    unsigned long indirPointers = allSettings.sbsIndirectPtrs->GetValue();
    unsigned long blockSize = allSettings.cmisBlockSize->GetValue();
    unsigned long inodeEntries = getInodeEntries();

    qlonglong doubleIndirs = inodeEntries - dirPointers - indirPointers;
    qlonglong indirEntries = (blockSize - 20) / 4;

    // A negative value indicates an invalid setting. The settings will not
    // be exportable until it is dealt with. In the mean time, do our best
    // to show a reasonable value if a negative value is encountered.
    if(indirEntries < 0)
    {
        indirEntries = 0;
    }
    if(doubleIndirs < 0)
    {
        doubleIndirs = 0;
    }

    qlonglong indirBlocks = static_cast<qulonglong>(indirPointers) * indirEntries;
    qlonglong dindirEntries = indirEntries * indirEntries;
    qlonglong dindirDataBlocksMax = 0xFFFFFFFF - (dirPointers + indirBlocks);
    qlonglong dindirDataBlocks = doubleIndirs * dindirEntries;

    if(dindirDataBlocks > dindirDataBlocksMax)
    {
        dindirDataBlocks = dindirDataBlocksMax;
    }

    qulonglong inodeDataBlocks = static_cast<qulonglong>(dirPointers) + indirBlocks + dindirDataBlocks;

    // Inode size is restricted such that the block count will fit into an
    // unsigned 32-bit integer.
    if (inodeDataBlocks > 0xFFFFFFFF)
        inodeDataBlocks = 0xFFFFFFFF;

    qlonglong inodeSizeMax = inodeDataBlocks * static_cast<qlonglong>(blockSize);

    labelMaxFsize->setText(VolumeSettings::FormatSize(inodeSizeMax));

    labelMaxVsize->setText(VolumeSettings::FormatSize(getVolSizeMaxBytes()));

}
