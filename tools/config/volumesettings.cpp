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
#include <stdexcept>

#include "validators.h"
#include "allsettings.h"
#include "volumesettings.h"

extern const char * const gpszSupported = "Supported";
extern const char * const gpszUnsupported = "Unsupported";

VolumeSettings::Volume::Volume(QString name,
                               WarningBtn *wbtnPathPrefix,
                               WarningBtn *wbtnSectorSize,
                               WarningBtn *wbtnVolSize,
                               WarningBtn *wbtnVolOff,
                               WarningBtn *wbtnInodeCount,
                               WarningBtn *wbtnAtomicWrite,
                               WarningBtn *wbtnDiscardSupport,
                               WarningBtn *wbtnBlockIoRetries)
    : stName("", name, validateVolName, wbtnPathPrefix),
      stSectorSize("", 512, validateVolSectorSize, wbtnSectorSize),
      stSectorCount("", 1024, validateVolSectorCount, wbtnVolSize),
      stSectorOff("", 0, validateVolSectorOff, wbtnVolOff),
      stInodeCount("", 100, validateVolInodeCount, wbtnInodeCount),
      stAtomicWrite("", gpszUnsupported, validateSupportedUnsupported, wbtnAtomicWrite),
      stDiscardSupport("", gpszUnsupported, validateDiscardSupport, wbtnDiscardSupport),
      stBlockIoRetries("", 0, validateVolIoRetries, wbtnBlockIoRetries)
{
    Q_ASSERT(allSettings.sbsAllocatedBuffers != NULL);
    stSectorCount.notifyList.append(allSettings.sbsAllocatedBuffers);
    stSectorSize.notifyList.append(&stSectorCount);
    stSectorSize.notifyList.append(&stSectorOff);
    stSectorCount.notifyList.append(&stInodeCount);
    stSectorSize.notifyList.append(&stInodeCount);
    stDiscardSupport.notifyList.append(allSettings.cbsAutomaticDiscards);
    stDiscardSupport.notifyList.append(allSettings.cbsPosixFstrim);
}

StrSetting *VolumeSettings::Volume::GetStName()
{
    return &stName;
}

IntSetting *VolumeSettings::Volume::GetStSectorSize()
{
    return &stSectorSize;
}

IntSetting *VolumeSettings::Volume::GetStSectorCount()
{
    return &stSectorCount;
}

IntSetting *VolumeSettings::Volume::GetStSectorOff()
{
    return &stSectorOff;
}

IntSetting *VolumeSettings::Volume::GetStInodeCount()
{
    return &stInodeCount;
}

StrSetting *VolumeSettings::Volume::GetStAtomicWrite()
{
    return &stAtomicWrite;
}

StrSetting *VolumeSettings::Volume::GetStDiscardSupport()
{
    return &stDiscardSupport;
}

IntSetting *VolumeSettings::Volume::GetStBlockIoRetries()
{
    return &stBlockIoRetries;
}

bool VolumeSettings::Volume::NeedsExternalImap()
{
    // Formulas taken from RedCoreInit

    Q_ASSERT(allSettings.rbtnsUsePosix != NULL);
    Q_ASSERT(allSettings.cmisBlockSize != NULL);

    unsigned long sectorSize = stSectorSize.GetValue();
    unsigned long sectorCount = stSectorCount.GetValue();
    unsigned long blockSize = allSettings.cmisBlockSize->GetValue();

    if(IsAutoSectorCount())
    {
        // If the sector count is unknown, both imaps must be included.
        return true;
    }
    else
    {
        unsigned long metarootHeaderSize = 16
                + (allSettings.rbtnsUsePosix->GetValue() ?
                       16 : 12);
        unsigned long metarootEntries = (blockSize - metarootHeaderSize) * 8;

        unsigned volSectorShift = 0;

        // If the sector size isn't known, assume the sector and block sizes are
        // the same, which will result in the maximum number of blocks.  This is
        // pessimistic and may result in the unnecessary inclusion of the external
        // imap, but will not result in its erroneous exclusion.
        if(!IsAutoSectorSize())
        {
            while((sectorSize << volSectorShift) < blockSize)
            {
                volSectorShift++;
            }
        }

        unsigned long volBlockCount = sectorCount >> volSectorShift;

        return (volBlockCount > metarootEntries + 3);
    }
}


bool VolumeSettings::Volume::NeedsInternalImap()
{
    // Formulas taken from RedCoreInit

    Q_ASSERT(allSettings.rbtnsUsePosix != NULL);
    Q_ASSERT(allSettings.cmisBlockSize != NULL);

    unsigned long sectorSize = stSectorSize.GetValue();
    unsigned long sectorCount = stSectorCount.GetValue();
    unsigned long blockSize = allSettings.cmisBlockSize->GetValue();

    if(IsAutoSectorCount())
    {
        // If the sector count is unknown, both imaps must be included.
        return true;
    }
    else
    {
        unsigned long metarootHeaderSize = 16
                + (allSettings.rbtnsUsePosix->GetValue() ?
                       16 : 12);
        unsigned long metarootEntries = (blockSize - metarootHeaderSize) * 8;

        unsigned volSectorShift = 0;

        // If the sector size isn't known, assume the sector will be the minimum
        // valid size.  This is pessimistic and may result in the unnecessary
        // inclusion of the internal imap, but will not result in its erroneous
        // exclusion.
        if(IsAutoSectorSize())
        {
            sectorSize = 128;
        }

        while((sectorSize << volSectorShift) < blockSize)
        {
            volSectorShift++;
        }

        unsigned long volBlockCount = sectorCount >> volSectorShift;

        return (volBlockCount <= metarootEntries + 3);
    }
}

bool VolumeSettings::Volume::IsAutoSectorSize()
{
	return fAutoSectorSize;
}

bool VolumeSettings::Volume::IsAutoSectorCount()
{
	return fAutoSectorCount;
}

void VolumeSettings::Volume::SetAutoSectorSize(bool value)
{
	fAutoSectorSize = value;
}

void VolumeSettings::Volume::SetAutoSectorCount(bool value)
{
	fAutoSectorCount = value;
}

VolumeSettings::VolumeSettings(QLineEdit *pathPrefixBox,
                               QComboBox *sectorSizeBox,
                               QCheckBox *sectorSizeAuto,
                               QSpinBox *volSizeBox,
                               QCheckBox *volSizeAuto,
                               QLabel *volSizeLabel,
                               QSpinBox *volOffBox,
                               QLabel *volOffLabel,
                               QSpinBox *inodeCountBox,
                               QComboBox *atomicWriteBox,
                               QComboBox *discardSupportBox,
                               QCheckBox *enableRetriesCheck,
                               QSpinBox *numRetriesBox,
                               QWidget *numRetriesWidget,
                               QPushButton *addButton,
                               QPushButton *removeButton,
                               QListWidget *volumesList,
                               WarningBtn *volCountWarn,
                               WarningBtn *pathPrefixWarn,
                               WarningBtn *sectorSizeWarn,
                               WarningBtn *volSizeWarn,
                               WarningBtn *volOffWarn,
                               WarningBtn *inodeCountWarn,
                               WarningBtn *atomicWriteWarn,
                               WarningBtn *discardSupportWarn,
                               WarningBtn *ioRetriesWarn)
    : stVolumeCount(macroNameVolumeCount, 1, validateVolumeCount),
      lePathPrefix(pathPrefixBox),
      sbVolSize(volSizeBox),
      cbVolSizeAuto(volSizeAuto),
      sbVolOff(volOffBox),
      sbInodeCount(inodeCountBox),
      labelVolSizeBytes(volSizeLabel),
      labelVolOffBytes(volOffLabel),
      cmbSectorSize(sectorSizeBox),
      cbSectorSizeAuto(sectorSizeAuto),
      cmbAtomicWrite(atomicWriteBox),
      cmbDiscardSupport(discardSupportBox),
      cbEnableRetries(enableRetriesCheck),
      sbNumRetries(numRetriesBox),
      widgetNumRetries(numRetriesWidget),
      btnAdd(addButton),
      btnRemSelected(removeButton),
      listVolumes(volumesList),
      wbtnVolCount(volCountWarn),
      wbtnPathPrefix(pathPrefixWarn),
      wbtnSectorSize(sectorSizeWarn),
      wbtnVolSize(volSizeWarn),
      wbtnVolOff(volOffWarn),
      wbtnInodeCount(inodeCountWarn),
      wbtnAtomicWrite(atomicWriteWarn),
      wbtnDiscardSupport(discardSupportWarn),
      wbtnIoRetries(ioRetriesWarn),
      volTick(0)
{
    Q_ASSERT(allSettings.rbtnsUsePosix != NULL);
    usePosix = allSettings.rbtnsUsePosix->GetValue();

    AddVolume();
    SetActiveVolume(volumes.count() - 1);

    connect(lePathPrefix, SIGNAL(textChanged(QString)),
            this, SLOT(lePathPrefix_textChanged(QString)));
    connect(sbVolSize, SIGNAL(valueChanged(QString)),
            this, SLOT(sbVolSize_valueChanged(QString)));
    connect(cbVolSizeAuto, SIGNAL(stateChanged(int)),
            this, SLOT(cbVolSizeAuto_stateChanged(int)));
    connect(sbVolOff, SIGNAL(valueChanged(QString)),
            this, SLOT(sbVolOff_valueChanged(QString)));
    connect(sbInodeCount, SIGNAL(valueChanged(QString)),
            this, SLOT(sbInodeCount_valueChanged(QString)));
    connect(cmbSectorSize, SIGNAL(currentIndexChanged(int)),
            this, SLOT(cmbSectorSize_currentIndexChanged(int)));
    connect(cbSectorSizeAuto, SIGNAL(stateChanged(int)),
            this, SLOT(cbSectorSizeAuto_stateChanged(int)));
    connect(cmbAtomicWrite, SIGNAL(currentIndexChanged(int)),
            this, SLOT(cmbAtomicWrite_currentIndexChanged(int)));
    connect(cmbDiscardSupport, SIGNAL(currentIndexChanged(int)),
            this, SLOT(cmbDiscardSupport_currentIndexChanged(int)));
    connect(cbEnableRetries, SIGNAL(stateChanged(int)),
            this, SLOT(cbEnableRetries_stateChanged(int)));
    connect(sbNumRetries, SIGNAL(valueChanged(QString)),
            this, SLOT(sbNumRetries_valueChanged(QString)));
    connect(listVolumes, SIGNAL(currentRowChanged(int)),
            this, SLOT(listVolumes_currentRowChanged(int)));
    connect(btnAdd, SIGNAL(clicked()),
            this, SLOT(btnAdd_clicked()));
    connect(btnRemSelected, SIGNAL(clicked()),
            this, SLOT(btnRemSelected_clicked()));

    updateVolSizeBytes();
    updateVolOffBytes();
}

VolumeSettings::~VolumeSettings()
{
    clearVolumes();
}

IntSetting *VolumeSettings::GetStVolumeCount()
{
    return &stVolumeCount;
}

QList<VolumeSettings::Volume *> *VolumeSettings::GetVolumes()
{
    return &volumes;
}

int VolumeSettings::GetCurrentIndex()
{
    checkCurrentIndex();
    return activeIndex;
}

void VolumeSettings::SetUsePosix(bool posix)
{
    usePosix = posix;

    if(posix)
    {
        lePathPrefix->setEnabled(true);
    }
    else
    {
        lePathPrefix->setEnabled(false);
    }

    // Refresh UI list, since the entry names will change
    // when usePosix changes.
    refreshVolumeList();
}

void VolumeSettings::SetActiveVolume(int index)
{
    if(index < 0 || index >= volumes.count())
    {
        throw new std::out_of_range(
             "invalid index given to VolumeSettings::SetActiveVolume");
    }

    if(activeIndex >= 0 && activeIndex < volumes.count())
    {
        deselectVolume(activeIndex);
    }

    // All of these must be deleted from the notify list any time a volume is
    // deselected or deleted, or else a memory fault will occur trying to notify
    // a null pointer.
    allSettings.cmisBlockSize->notifyList.append(volumes[index]->GetStSectorSize());
    allSettings.cmisBlockSize->notifyList.append(volumes[index]->GetStSectorCount());
    allSettings.cmisBlockSize->notifyList.append(volumes[index]->GetStSectorOff());
    allSettings.cmisBlockSize->notifyList.append(volumes[index]->GetStInodeCount());
    allSettings.rbtnsUsePosix->notifyList.append(volumes[index]->GetStInodeCount());
    allSettings.rbtnsUsePosix->notifyList.append(volumes[index]->GetStName());

    // Update the UI fields to reflect the new active volume.
    //
    // Setting the UI values will trigger input processing, which
    // will automatically check validity and set any needed
    // warning icons.

    activeIndex = index;

    lePathPrefix->setText(volumes[index]->GetStName()->GetValue());

	//  Update the auto sector count check box
	if(volumes[index]->IsAutoSectorCount())
	{
		cbVolSizeAuto->setCheckState(Qt::CheckState::Checked);
	}
	else
	{
		cbVolSizeAuto->setCheckState(Qt::CheckState::Unchecked);
	}

	//  Update the volume size spin box
    sbVolSize->setValue(volumes[index]->GetStSectorCount()->GetValue());

	//  Update the volume offset spin box
    sbVolOff->setValue(volumes[index]->GetStSectorOff()->GetValue());

    sbInodeCount->setValue(volumes[index]->GetStInodeCount()->GetValue());

    // Volume size bytes label set by sbVolSize_valueChanged
    // Volume offset bytes label set by sbVolOff_valueChanged

    // Use QLocale to add comma separators
    QLocale l(QLocale::English, QLocale::UnitedStates);

	//  Update the auto sector size check box
	if(volumes[index]->IsAutoSectorSize())
	{
		cbSectorSizeAuto->setCheckState(Qt::CheckState::Checked);
	}
	else
	{
		cbSectorSizeAuto->setCheckState(Qt::CheckState::Unchecked);
	}

	//  Update the sector size combo box
	cmbSectorSize->setCurrentText(l.toString(
			static_cast<qlonglong>(volumes[index]->GetStSectorSize()->GetValue())));

    cmbAtomicWrite->setCurrentText(volumes[index]->GetStAtomicWrite()->GetValue());

    cmbDiscardSupport->setCurrentText(volumes[index]->GetStDiscardSupport()->GetValue());

    unsigned long ioRetriesValue = volumes[index]->GetStBlockIoRetries()->GetValue();
    widgetNumRetries->setEnabled(ioRetriesValue != 0);
    cbEnableRetries->setChecked(ioRetriesValue != 0);
    if(ioRetriesValue != 0)
    {
        sbNumRetries->setValue(ioRetriesValue);
    }

    listVolumes->setCurrentRow(index);
}

void VolumeSettings::AddVolume()
{
    QString name = QString("VOL") + QString::number(volTick) + QString(":");

    volumes.append(new Volume(name, wbtnPathPrefix, wbtnSectorSize, wbtnVolSize, wbtnVolOff,
                              wbtnInodeCount, wbtnAtomicWrite, wbtnDiscardSupport,
                              wbtnIoRetries));
    volTick++;

    if(!usePosix)
    {
        name = QString("Volume ") + QString::number(volumes.count());
    }
    // else leave 'name' unmodified for item label

    listVolumes->addItem(name);

    checkSetVolumeCount();
}

void VolumeSettings::RemoveActiveVolume()
{
    // Asserts activeIndex valid
    if(!checkCurrentIndex()) return;

    Q_ASSERT(allSettings.cmisBlockSize != NULL);

    deselectVolume(activeIndex);

    delete volumes[activeIndex];
    volumes.removeAt(activeIndex);

    if(activeIndex >= volumes.count())
    {
        if(volumes.count() == 0)
        {
            Q_ASSERT(false); // This was the last volume: should not happen.

            AddVolume(); // Add another volume if it does happen
            return;
        }

        // Select the last volume in the list
        activeIndex = volumes.count() - 1;
    }

    refreshVolumeList();
    checkSetVolumeCount();
}

void VolumeSettings::GetErrors(QStringList &errors, QStringList &warnings)
{
    int rememberIndex = activeIndex;
    for(int i = 0; i < volumes.count(); i++)
    {
        // Artificially set activeIndex so that the volume name validator
        // does not check volumes[i]'s name against itself and find a
        // false duplicate.
        activeIndex = i;
        AllSettings::CheckError(volumes[i]->GetStName(), errors, warnings);
        AllSettings::CheckError(volumes[i]->GetStSectorCount(), errors, warnings);
        AllSettings::CheckError(volumes[i]->GetStSectorOff(), errors, warnings);
        AllSettings::CheckError(volumes[i]->GetStInodeCount(), errors, warnings);
        AllSettings::CheckError(volumes[i]->GetStSectorSize(), errors, warnings);
        AllSettings::CheckError(volumes[i]->GetStAtomicWrite(), errors, warnings);
        AllSettings::CheckError(volumes[i]->GetStDiscardSupport(), errors, warnings);
        AllSettings::CheckError(volumes[i]->GetStBlockIoRetries(), errors, warnings);
    }
    if(activeIndex != rememberIndex)
    {
        // Re-set warning buttons to the active volume

        activeIndex = rememberIndex;
        checkCurrentIndex();
        volumes[activeIndex]->GetStName()->Notify();
        volumes[activeIndex]->GetStSectorCount()->Notify();
        volumes[activeIndex]->GetStSectorOff()->Notify();
        volumes[activeIndex]->GetStInodeCount()->Notify();
        volumes[activeIndex]->GetStSectorSize()->Notify();
        volumes[activeIndex]->GetStAtomicWrite()->Notify();
        volumes[activeIndex]->GetStDiscardSupport()->Notify();
        volumes[activeIndex]->GetStBlockIoRetries()->Notify();
    }
}

void VolumeSettings::GetImapRequirements(bool &imapInline, bool &imapExternal)
{
    imapInline = false;
    imapExternal = false;

    for(int i = 0; i < volumes.count(); i++)
    {
        if(volumes[i]->NeedsExternalImap())
        {
            imapExternal = true;
        }

        if(volumes[i]->NeedsInternalImap())
        {
            imapInline = true;
        }

        if(imapInline && imapExternal)
        {
            break; //No need to keep testing.
        }
    }
}

bool VolumeSettings::GetDiscardsSupported()
{
    for(int i = 0; i < volumes.count(); i++)
    {
        if(QString::compare(volumes[i]->GetStDiscardSupport()->GetValue(),
                            gpszSupported, Qt::CaseInsensitive) == 0)
        {
            // If one volume supports discards, then the driver needs
            // to support them.
            return true;
        }
    }

    return false;
}

QString VolumeSettings::FormatCodefileOutput()
{

    QString toReturn = QString("\
/** @file\n\
*/\n\
#include <redconf.h>\n\
#include <redtypes.h>\n\
#include <redmacs.h>\n\
#include <redvolume.h>\n\
\n\n\
const VOLCONF gaRedVolConf[REDCONF_VOLUME_COUNT] =\n\
{\n");

    for(int i = 0; i < volumes.count(); i++)
    {
		QString sectorSize;
		QString sectorCount;

		if(volumes[i]->IsAutoSectorSize())
		{
			sectorSize = QString("SECTOR_SIZE_AUTO");
		}
		else
		{
			sectorSize = QString::number(volumes[i]->GetStSectorSize()->GetValue()) + QString("U");
		}
		if(volumes[i]->IsAutoSectorCount())
		{
			sectorCount = QString("SECTOR_COUNT_AUTO");
		}
		else
		{
			sectorCount = QString::number(volumes[i]->GetStSectorCount()->GetValue()) + QString("U");
		}

        toReturn += QString("    { ")
                + sectorSize
                + QString(", ")
                + sectorCount
                + QString(", ")
				+ QString::number(volumes[i]->GetStSectorOff()->GetValue())
				+ QString("U, ")
                + (QString::compare(volumes[i]->GetStAtomicWrite()->GetValue(),
                                    gpszSupported, Qt::CaseInsensitive) == 0
                   ? QString("true") : QString("false"))
                + QString(", ")
                + QString::number(volumes[i]->GetStInodeCount()->GetValue())
                + QString("U")
                + QString(", ")
                + QString::number(volumes[i]->GetStBlockIoRetries()->GetValue())
                + QString("U");

        // Discards are only listed if REDCONF_DISCARDS is true.
        if(allSettings.cbsAutomaticDiscards->GetValue() || allSettings.cbsPosixFstrim->GetValue())
        {
            toReturn += (QString::compare(volumes[i]->GetStDiscardSupport()->GetValue(),
                                gpszSupported, Qt::CaseInsensitive) == 0
                         ? QString(", true") : QString(", false"));
        }

        Q_ASSERT(allSettings.rbtnsUsePosix != NULL);
        Q_ASSERT(allSettings.rbtnsUsePosix->GetValue() == usePosix);

        if(allSettings.rbtnsUsePosix->GetValue())
        {
            QString volName = volumes[i]->GetStName()->GetValue();

            // Add escape chars where necessary
            volName.replace('\\', "\\\\");
            volName.replace('"', "\\\"");

            // Add as new entry, enclosed in quotation marks
            toReturn += QString(", \"")
                    + volName
                    + QString("\"");
        }

        if(i == volumes.count() - 1)
        {
            toReturn += QString(" }\n");
        }
        else
        {
            toReturn += QString(" },\n");
        }
    }

    toReturn += QString("};\n");
    return toReturn;
}

void VolumeSettings::ParseCodefile(const QString &text,
                                   QStringList &notFound,
                                   QStringList &notParsed)
{
    QRegularExpression exp("gaRedVolConf\\[.+?\\]\\s*=\\s*\\{([\\s\\S]*?)\\} *;");
    QRegularExpressionMatch rem = exp.match(text);

    if(!rem.hasMatch() || rem.lastCapturedIndex() < 1)
    {
        notFound += "Volume settings (gaRedVolConf)";
        return;
    }
    QString strVolumes = rem.captured(1);

    exp = QRegularExpression("\\{\\s*([\\s\\S]*?)\\s*\\}\\s*,?");

    // Skip comment (capture index 1): (/\\*[\\s\\S]*?\\*/)?
    // Capture value (capture index 2): (\\w+)
    // Ensure we caught the whole argument, skip final whitespace: \\s*(,\\s*|$)
    QRegularExpression valueExp = QRegularExpression("(/\\*[\\s\\S]*?\\*/)?\\s*(\\w+)\\s*(,\\s*|$)");

    // Same regex as valueExp, except value is enclosed in
    // quotation marks and may be empty.
    QRegularExpression pathPrefixEpx = QRegularExpression("(/\\*[\\s\\S]*?\\*/)?\"(.*?)\",?\\s*");

    // The position in strVolumes to start looking for the next volume
    int currPos = 0;
    int currVolIndex = 0;
    QList<Volume *> newVolumes;
    bool failure = false;

    while(!failure)
    {
        rem = exp.match(strVolumes, currPos);
        if(!rem.hasMatch() || rem.lastCapturedIndex() < 1)
        {
            break;
        }

        // The initialization block of the current volume in gaRedVolConf
        QString currStr = rem.captured(1);
        currPos = rem.capturedEnd(0);

        // The position in currStr to start looking for the next value
        int currVolPos = 0;

        // List of unparsed values of the settings of the current volume
        QStringList strValues;

        for(int i = 0; i < 7; i++)
        {
            rem = valueExp.match(currStr, currVolPos);
            if(!rem.hasMatch() || rem.lastCapturedIndex() < 2)
            {
                // All valid configuration files (from all versions of the
                // configuration tool) should have at least 4 arguments.
                if(i < 4)
                {
                    failure = true;
                }
                break;
            }
            strValues += rem.captured(2);
            currVolPos = rem.capturedEnd(0);
        }
        if(failure)
        {
            break;
        }

        QString pathPrefix;

        rem = pathPrefixEpx.match(currStr, currVolPos);
        if(!rem.hasMatch() || rem.lastCapturedIndex() < 2)
        {
            // It's normal for this to be missing if the file
            // was not exported in POSIX mode. Use a default
            // name if not found.
            pathPrefix = QString("VOL")
                    + QString::number(currVolIndex)
                    + QString(":");
        }
        else
        {
            pathPrefix = rem.captured(2);
        }

        Volume * newVol = new Volume(pathPrefix,
                                     wbtnPathPrefix,
                                     wbtnSectorSize,
                                     wbtnVolSize,
                                     wbtnVolOff,
                                     wbtnInodeCount,
                                     wbtnAtomicWrite,
                                     wbtnDiscardSupport,
                                     wbtnIoRetries);

		if((QString::compare(strValues[0], "SECTOR_SIZE_AUTO") == 0) ||
			(QString::compare(strValues[0], "0U") == 0) ||
			(QString::compare(strValues[0], "0") == 0))
		{
			newVol->SetAutoSectorSize(true);
		}
		else
		{
			parseAndSet(newVol->GetStSectorSize(), strValues[0], notParsed,
					pathPrefix + QString(" sector size"));
		}
        if((QString::compare(strValues[1], "SECTOR_COUNT_AUTO") == 0) ||
            (QString::compare(strValues[1], "0U") == 0) ||
            (QString::compare(strValues[1], "0") == 0))
		{
			newVol->SetAutoSectorCount(true);
		}
		else
		{
			parseAndSet(newVol->GetStSectorCount(), strValues[1], notParsed,
					pathPrefix + QString(" sector count"));
		}
        parseAndSet(newVol->GetStSectorOff(), strValues[2], notParsed,
                pathPrefix + QString(" sector offset"));

        // Special case parse and set
        if(QString::compare(strValues[3], "true") == 0)
        {
            newVol->GetStAtomicWrite()->SetValue(gpszSupported);
        }
        else if(QString::compare(strValues[3], "false") == 0)
        {
            newVol->GetStAtomicWrite()->SetValue(gpszUnsupported);
        }
        else
        {
            notParsed += pathPrefix + QString(" atomic write supported");
        }

        parseAndSet(newVol->GetStInodeCount(), strValues[4], notParsed,
                pathPrefix + QString(" inode count"));

        // Don't complain if I/O retries setting isn't found (see
        // comments above).
        if(strValues.count() > 5)
        {
            parseAndSet(newVol->GetStBlockIoRetries(), strValues[5], notParsed,
                    pathPrefix + QString(" block I/O retries"));
        }

        // The discard supported setting is set in Reliance Edge v1.1
        // and above only if REDCONF_DISCARDS is enabled.
        // For now, it's safe to check strValues.count(), but in the
        // future we may need to check whether discards are globally
        // supported before parsing per-volume.
        if(strValues.count() > 6)
        {
            // Special case parse and set
            if(QString::compare(strValues[6], "true") == 0)
            {
                newVol->GetStDiscardSupport()->SetValue(gpszSupported);
            }
            else if(QString::compare(strValues[6], "false") == 0)
            {
                newVol->GetStDiscardSupport()->SetValue(gpszUnsupported);
            }
            else
            {
                notParsed += pathPrefix + QString(" discards supported");
            }
        }

        newVolumes.append(newVol);
        currVolIndex++;
    }

    if(failure || currVolIndex == 0)
    {
        if(failure)
        {
            for(int i = 0; i < newVolumes.count(); i++)
            {
                delete newVolumes[i];
            }
        }
        notParsed += "Volume settings (gaRedVolConf)";
    }
    else
    {
        clearVolumes();

        activeIndex = 0;
        volumes = newVolumes;
        Q_ASSERT(volumes.count() > 0);

        refreshVolumeList();
        checkSetVolumeCount();
    }
}

QString VolumeSettings::FormatSize(qulonglong sizeInBytes)
{
    // Use QLocale to add comma separators
    QLocale l(QLocale::English, QLocale::UnitedStates);
    QString strBytes = l.toString((qulonglong) sizeInBytes) + QString(" bytes");

    if(sizeInBytes < 1024ull)
    {
        return strBytes;
    }

    double dbSize;
    QString strSize;

    if(sizeInBytes < 1024ull * 1024ull)
    {
        dbSize = (double) sizeInBytes / 1024.0l;
        strSize = QString::number(dbSize, 'f', 2)
                + QString(" KB");
    }
    else if(sizeInBytes < 1024ull * 1024ull * 1024ull)
    {
        dbSize = (double) sizeInBytes / 1024.0l / 1024.0l;
        strSize = QString::number(dbSize, 'f', 2)
                + QString(" MB");
    }
    else if(sizeInBytes < 1024ull * 1024ull * 1024ull * 1024ull)
    {
        dbSize = (double) sizeInBytes / 1024.0l / 1024.0l / 1024.0l;
        strSize = QString::number(dbSize, 'f', 2)
                + QString(" GB");
    }
    else if(sizeInBytes < 1024ull * 1024ull * 1024ull * 1024ull * 1024ull)
    {
        dbSize = (double) sizeInBytes / 1024.0l / 1024.0l / 1024.0l / 1024.0l;
        strSize = QString::number(dbSize, 'f', 2)
                + QString(" TB");
    }
    else
    {
        dbSize = (double) sizeInBytes / 1024.0l / 1024.0l / 1024.0l / 1024.0l / 1024.0l;
        strSize = QString::number(dbSize, 'f', 2)
                + QString(" PB");
    }

    return strSize + QString(" (") + strBytes + QString(")");
}

// Deletes all entries from volumes
void VolumeSettings::clearVolumes()
{
    if(activeIndex >= 0 && activeIndex < volumes.count())
    {
        // Remove references from other settings to avoid
        // memory access errors
        deselectVolume(activeIndex);
    }

    for(int i = 0; i < volumes.count(); i++)
    {
        delete volumes[i];
    }
}

// Remove references from allSettings members in order to avoid
// automatic re-checking (which would incorrectly set warning
// icons) and memory access violations if the volume is deleted.
void VolumeSettings::deselectVolume(int index)
{
    Q_ASSERT(allSettings.cmisBlockSize != NULL);
    Q_ASSERT(allSettings.rbtnsUsePosix != NULL);

    allSettings.cmisBlockSize->notifyList
            .removeOne(volumes[index]->GetStSectorSize());
    allSettings.cmisBlockSize->notifyList
            .removeOne(volumes[index]->GetStSectorCount());
    allSettings.cmisBlockSize->notifyList
            .removeOne(volumes[index]->GetStSectorOff());
    allSettings.cmisBlockSize->notifyList
            .removeOne(volumes[index]->GetStInodeCount());
    allSettings.rbtnsUsePosix->notifyList
            .removeOne(volumes[index]->GetStInodeCount());
    allSettings.rbtnsUsePosix->notifyList
            .removeOne(volumes[index]->GetStName());
}

// Helper function for ParseCodeFile
template<typename T>
void VolumeSettings::parseAndSet(Setting<T> *setting,
                                 const QString &strValue,
                                 QStringList &notParsed,
                                 const QString &humanName)
{
    T value;
    bool success = setting->TryParse(strValue, value);
    if(!success)
    {
        notParsed += humanName;
    }
    else
    {
        setting->SetValue(value);
    }
}

// Checks in the current number of volumes, setting any UI warnings
void VolumeSettings::checkSetVolumeCount()
{
    Q_ASSERT(listVolumes->count() == volumes.count());

    // Can't use stVolumeCount.ProcessInput because we
    // have an int already and not a QString
    QString msg;
    Validity v = stVolumeCount.CheckValid(
                (unsigned long) volumes.count(), msg);
    wbtnVolCount->Set(v, msg);

    stVolumeCount.SetValue((unsigned long) volumes.count());

    if(volumes.count() <= 1)
    {
        btnRemSelected->setEnabled(false);
    }
    else
    {
        btnRemSelected->setEnabled(true);
    }
}

// Clears the volume list in the UI and repopulates it
void VolumeSettings::refreshVolumeList()
{
    listVolumes->clear();

    if(usePosix)
    {
        for(int i = 0; i < volumes.count(); i++)
        {
            listVolumes->addItem(volumes[i]->GetStName()->GetValue());
        }
    }
    else
    {
        for(int i = 0; i < volumes.count(); i++)
        {
            listVolumes->addItem(QString("Volume ") + QString::number(i));
        }
    }

    Q_ASSERT(activeIndex >= 0 && activeIndex < volumes.count());
    SetActiveVolume(activeIndex);
}

// Asserts that activeIndex is valid. Takes steps to recover
// if it is not.
bool VolumeSettings::checkCurrentIndex()
{
    if(volumes.count() == 0)
    {
        Q_ASSERT(false); //Should never be 0
        AddVolume();
        return false;
    }
    else if(activeIndex < 0 || activeIndex >= volumes.count())
    {
        Q_ASSERT(false); //Shouldn't happen
        SetActiveVolume(0);
        return false;
    }

    return true;
}

// Updates the label that reports the volume size in bytes.
void VolumeSettings::updateVolSizeBytes()
{
    // If either the size or count of sectors is automatically-detected, the
    // volume size cannot be computed.
    if(volumes[activeIndex]->IsAutoSectorCount() || volumes[activeIndex]->IsAutoSectorSize())
    {
        labelVolSizeBytes->setText(QString("Auto Detect"));
    }
    else
    {
        labelVolSizeBytes->setText(FormatSize(
                static_cast<qulonglong>(volumes[activeIndex]->GetStSectorSize()->GetValue())
                * static_cast<qulonglong>(volumes[activeIndex]->GetStSectorCount()->GetValue())));
    }
}

// Updates the label that reports the volume offset in bytes.
void VolumeSettings::updateVolOffBytes()
{
    labelVolOffBytes->setText(FormatSize(
            static_cast<qulonglong>(volumes[activeIndex]->GetStSectorSize()->GetValue())
            * static_cast<qulonglong>(volumes[activeIndex]->GetStSectorOff()->GetValue())));
}

void VolumeSettings::lePathPrefix_textChanged(const QString &text)
{
    // Asserts that the vol index is ok
    if(!checkCurrentIndex()) return;

    volumes[activeIndex]->GetStName()->ProcessInput(text);

    if(usePosix)
    {
        listVolumes->item(activeIndex)->setText(text);
    }
    else
    {
        listVolumes->item(activeIndex)->setText(QString("Volume ")
                                                + QString::number(activeIndex));
    }
}

void VolumeSettings::cbSectorSizeAuto_stateChanged(int state)
{
	volumes[activeIndex]->SetAutoSectorSize(state == Qt::Checked);
    cmbSectorSize->setEnabled(state == Qt::Unchecked);

    updateVolSizeBytes();
    updateVolOffBytes();
}

void VolumeSettings::cmbSectorSize_currentIndexChanged(int index)
{
    // Asserts that the vol index is ok
    if(!checkCurrentIndex()) return;

    try {
        volumes[activeIndex]->GetStSectorSize()
                ->ProcessInput(cmbSectorSize->itemText(index));
    }
    catch(std::invalid_argument)
    {
        Q_ASSERT(false);
        return;
    }

    updateVolSizeBytes();
    updateVolOffBytes();
}

void VolumeSettings::cbVolSizeAuto_stateChanged(int state)
{
	volumes[activeIndex]->SetAutoSectorCount(state == Qt::Checked);
    sbVolSize->setEnabled(state == Qt::Unchecked);

    updateVolSizeBytes();
    updateVolOffBytes();
}

void VolumeSettings::sbVolSize_valueChanged(const QString &value)
{
    // Asserts that the vol index is ok
    if(!checkCurrentIndex()) return;

    try
    {
        volumes[activeIndex]->GetStSectorCount()->ProcessInput(value);
    }
    catch(std::invalid_argument)
    {
        Q_ASSERT(false);
        return;
    }

    updateVolSizeBytes();
}

void VolumeSettings::sbVolOff_valueChanged(const QString &value)
{
    // Asserts that the vol index is ok
    if(!checkCurrentIndex()) return;

    try
    {
        volumes[activeIndex]->GetStSectorOff()->ProcessInput(value);
    }
    catch(std::invalid_argument)
    {
        Q_ASSERT(false);
        return;
    }

    updateVolOffBytes();
}

void VolumeSettings::sbInodeCount_valueChanged(const QString &value)
{
    // Asserts that the vol index is ok
    if(!checkCurrentIndex()) return;

    try {
        volumes[activeIndex]->GetStInodeCount()->ProcessInput(value);
    }
    catch(std::invalid_argument)
    {
        Q_ASSERT(false);
        return;
    }
}

void VolumeSettings::cmbAtomicWrite_currentIndexChanged(int index)
{
    // Asserts that the vol index is ok
    if(!checkCurrentIndex()) return;

    try {
        volumes[activeIndex]->GetStAtomicWrite()
                ->ProcessInput(cmbAtomicWrite->itemText(index));
    }
    catch(std::invalid_argument)
    {
        Q_ASSERT(false);
        return;
    }
}

void VolumeSettings::cmbDiscardSupport_currentIndexChanged(int index)
{
    // Asserts that the vol index is ok
    if(!checkCurrentIndex()) return;

    try {
        volumes[activeIndex]->GetStDiscardSupport()
                ->ProcessInput(cmbDiscardSupport->itemText(index));
    }
    catch(std::invalid_argument)
    {
        Q_ASSERT(false);
        return;
    }
}

void VolumeSettings::cbEnableRetries_stateChanged(int state)
{
    widgetNumRetries->setEnabled(state == Qt::Checked);

    if(state == Qt::Checked)
    {
        volumes[activeIndex]->GetStBlockIoRetries()
                ->ProcessInput(sbNumRetries->text());
    }
    else
    {
        // Unchecked -> disable retries -> max 0 retries.
        volumes[activeIndex]->GetStBlockIoRetries()
                ->ProcessInput("0");
    }
}

void VolumeSettings::sbNumRetries_valueChanged(const QString & text)
{
    if(cbEnableRetries->isChecked())
    {
        volumes[activeIndex]->GetStBlockIoRetries()
                ->ProcessInput(text);
    }
}

void VolumeSettings::listVolumes_currentRowChanged(int row)
{
    if(row < 0 || row == activeIndex)
    {
        // `row` will equal `activeIndex` when the row is
        // changed programatically. `row` will equal -1
        // when the listVolumes is cleared.
        return;
    }

    Q_ASSERT(row < volumes.count());

    SetActiveVolume(row);
}

void VolumeSettings::btnAdd_clicked()
{
    AddVolume();
    Q_ASSERT(volumes.count() > 0);
    SetActiveVolume(volumes.count() - 1);
}

void VolumeSettings::btnRemSelected_clicked()
{
    RemoveActiveVolume();
}

VolumeSettings * volumeSettings = 0;
