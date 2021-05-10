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
#include "dindirreporter.h"

DindirReporter::DindirReporter(QLabel *dindirLabel)
    : label(dindirLabel)
{
    // Assert this one, assume the others.
    Q_ASSERT(allSettings.cmisBlockSize != NULL);

    allSettings.cbsInodeBlockCount->notifyList.append(this);
    allSettings.cbsInodeTimestamps->notifyList.append(this);
    allSettings.rbtnsUsePosix->notifyList.append(this);
    allSettings.cmisBlockSize->notifyList.append(this);
    allSettings.sbsDirectPtrs->notifyList.append(this);
    allSettings.sbsIndirectPtrs->notifyList.append(this);

    // The DindirReporter is deleted after allSettings, so there is
    // no need to remove these on destruction.

    Notify();
}

void DindirReporter::Notify()
{
    Q_ASSERT(allSettings.sbsDirectPtrs != NULL);
    Q_ASSERT(allSettings.sbsIndirectPtrs != NULL);
    Q_ASSERT(allSettings.cmisBlockSize != NULL);

    unsigned long blockSize = allSettings.cmisBlockSize->GetValue();
    unsigned long dirPointers = allSettings.sbsDirectPtrs->GetValue();
    unsigned long indirPointers = allSettings.sbsIndirectPtrs->GetValue();
    long dindirPtrs = (long) getInodeEntries() - dirPointers - indirPointers;

    if(dindirPtrs < 0)
    {
        label->setText("--");
    }
    else
    {
        qlonglong indirEntries = (blockSize - 20) / 4;

        qlonglong dindirEntries = indirEntries * indirEntries;
        qlonglong dindirDataBlocks = static_cast<qlonglong>(dindirPtrs) * dindirEntries;
        qlonglong dindirDataBlocksMax = 0xFFFFFFFF - (dirPointers + (indirEntries * indirPointers));

        if(dindirDataBlocks > dindirDataBlocksMax)
        {
            dindirPtrs = (dindirDataBlocksMax + dindirEntries - 1) / dindirEntries;
        }

        label->setText(QString::number(dindirPtrs));
    }
}

