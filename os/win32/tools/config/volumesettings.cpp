/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                   Copyright (c) 2014-2015 Datalight, Inc.
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
    comply with the terms of the GPLv2 license may obtain a commercial license
    before incorporating Reliance Edge into proprietary software for
    distribution in any form.  Visit http://www.datalight.com/reliance-edge for
    more information.
*/
#include <stdexcept>

#include "volumesettings.h"
#include "validators.h"
#include "allsettings.h"

extern const char * const gpszAtomicWrTrue = "Supported";
extern const char * const gpszAtomicWrFalse = "Unsupported";

VolumeSettings::Volume::Volume(QString name,
                               WarningBtn *wbtnPathPrefix,
                               WarningBtn *wbtnSectorSize,
                               WarningBtn *wbtnVolSize,
                               WarningBtn *wbtnInodeCount,
                               WarningBtn *wbtnAtomicWrite)
    : stName("", name, validateVolName, wbtnPathPrefix),
      stSectorSize("", 512, validateVolSectorSize, wbtnSectorSize),
      stSectorCount("", 1024, validateVolSectorCount, wbtnVolSize),
      stInodeCount("", 100, validateVolInodeCount, wbtnInodeCount),
      stAtomicWrite("", gpszAtomicWrFalse, validateVolAtomicWrite, wbtnAtomicWrite)
{
    Q_ASSERT(allSettings.sbsAllocatedBuffers != NULL);
    stSectorCount.notifyList.append(allSettings.sbsAllocatedBuffers);
    stSectorSize.notifyList.append(&stSectorCount);
    stSectorCount.notifyList.append(&stInodeCount);
    stSectorSize.notifyList.append(&stInodeCount);
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

IntSetting *VolumeSettings::Volume::GetStInodeCount()
{
    return &stInodeCount;
}

StrSetting *VolumeSettings::Volume::GetStAtomicWrite()
{
    return &stAtomicWrite;
}

bool VolumeSettings::Volume::NeedsExternalImap()
{
    // Formulas taken from RedCoreInit

    Q_ASSERT(allSettings.rbtnsUsePosix != NULL);
    Q_ASSERT(allSettings.cmisBlockSize != NULL);

    unsigned long metarootHeaderSize = 16
            + (allSettings.rbtnsUsePosix->GetValue() ?
                   16 : 12);
    unsigned long metarootEntries =
            (allSettings.cmisBlockSize->GetValue()
             - metarootHeaderSize)
            * 8;

    unsigned volSectorShift = 0;
    while((stSectorSize.GetValue() << volSectorShift)
          < allSettings.cmisBlockSize->GetValue())
    {
        volSectorShift++;
    }

    unsigned long volBlockCount = stSectorCount.GetValue() >> volSectorShift;

    return (volBlockCount - 3 > metarootEntries);
}


VolumeSettings::VolumeSettings(QLineEdit *pathPrefixBox,
                               QComboBox *sectorSizeBox,
                               QSpinBox *volSizeBox,
                               QLabel *volSizeLabel,
                               QSpinBox *inodeCountBox,
                               QComboBox *atomicWriteBox,
                               QPushButton *addButton,
                               QPushButton *removeButton,
                               QListWidget *volumesList,
                               WarningBtn *volCountWarn,
                               WarningBtn *pathPrefixWarn,
                               WarningBtn *sectorSizeWarn,
                               WarningBtn *volSizeWarn,
                               WarningBtn *inodeCountWarn,
                               WarningBtn *atomicWriteWarn)
    : stVolumeCount(macroNameVolumeCount, 1, validateVolumeCount),
      lePathPrefix(pathPrefixBox),
      sbVolSize(volSizeBox),
      sbInodeCount(inodeCountBox),
      labelVolSizeBytes(volSizeLabel),
      cmbSectorSize(sectorSizeBox),
      cmbAtomicWrite(atomicWriteBox),
      btnAdd(addButton),
      btnRemSelected(removeButton),
      listVolumes(volumesList),
      wbtnVolCount(volCountWarn),
      wbtnPathPrefix(pathPrefixWarn),
      wbtnSectorSize(sectorSizeWarn),
      wbtnVolSize(volSizeWarn),
      wbtnInodeCount(inodeCountWarn),
      wbtnAtomicWrite(atomicWriteWarn),
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
    connect(sbInodeCount, SIGNAL(valueChanged(QString)),
                     this, SLOT(sbInodeCount_valueChanged(QString)));
    connect(cmbSectorSize, SIGNAL(currentIndexChanged(int)),
                     this, SLOT(cmbSectorSize_currentIndexChanged(int)));
    connect(cmbAtomicWrite, SIGNAL(currentIndexChanged(int)),
                     this, SLOT(cmbAtomicWrite_currentIndexChanged(int)));
    connect(listVolumes, SIGNAL(currentRowChanged(int)),
                     this, SLOT(listVolumes_currentRowChanged(int)));
    connect(btnAdd, SIGNAL(clicked()),
                     this, SLOT(btnAdd_clicked()));
    connect(btnRemSelected, SIGNAL(clicked()),
                     this, SLOT(btnRemSelected_clicked()));

    updateVolSizeBytes();
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

    // Important: set activeIndex before changing UI elements, since
    // some may try accessing volumes[activeIndex] on UI update
    int rememberIndex = activeIndex;
    activeIndex = index;

    // Update the UI fields to reflect the new active volume.
    //
    // Setting the UI values will trigger input processing, which
    // will automatically check validity and set any needed
    // warning icons.

    lePathPrefix->setText(volumes[index]->GetStName()->GetValue());

    sbVolSize->setValue(volumes[index]->GetStSectorCount()->GetValue());

    sbInodeCount->setValue(volumes[index]->GetStInodeCount()->GetValue());

    // Volume size bytes label set by sbVolSize_valueChanged

    // Use QLocale to add comma separators
    QLocale l(QLocale::English, QLocale::UnitedStates);
    cmbSectorSize->setCurrentText(l.toString(
            static_cast<qlonglong>(volumes[index]->GetStSectorSize()->GetValue())));

    cmbAtomicWrite->setCurrentText(volumes[index]->GetStAtomicWrite()->GetValue());

    listVolumes->setCurrentRow(index);

    if(rememberIndex >= 0 && rememberIndex < volumes.count())
    {
        deselectVolume(rememberIndex);
    }

    allSettings.cmisBlockSize->notifyList.append(volumes[index]->GetStSectorSize());
    allSettings.cmisBlockSize->notifyList.append(volumes[index]->GetStSectorCount());
    allSettings.cmisBlockSize->notifyList.append(volumes[index]->GetStInodeCount());
    allSettings.rbtnsUsePosix->notifyList.append(volumes[index]->GetStInodeCount());
}

void VolumeSettings::AddVolume()
{
    QString name = QString("VOL") + QString::number(volTick) + QString(":");

    volumes.append(new Volume(name, wbtnPathPrefix, wbtnSectorSize, wbtnVolSize,
                              wbtnInodeCount, wbtnAtomicWrite));
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
        AllSettings::CheckError(volumes[i]->GetStInodeCount(), errors, warnings);
        AllSettings::CheckError(volumes[i]->GetStSectorSize(), errors, warnings);
        AllSettings::CheckError(volumes[i]->GetStAtomicWrite(), errors, warnings);
    }
    if(activeIndex != rememberIndex)
    {
        // Re-set warning buttons to the active volume

        activeIndex = rememberIndex;
        checkCurrentIndex();
        volumes[activeIndex]->GetStName()->Notify();
        volumes[activeIndex]->GetStSectorCount()->Notify();
        volumes[activeIndex]->GetStInodeCount()->Notify();
        volumes[activeIndex]->GetStSectorSize()->Notify();
        volumes[activeIndex]->GetStAtomicWrite()->Notify();
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
        else
        {
            imapInline = true;
        }

        if(imapInline && imapExternal)
        {
            break; //No need to keep testing.
        }
    }
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
        toReturn += QString("    { ")
                + QString::number(volumes[i]->GetStSectorSize()->GetValue())
                + QString("U, ")
                + QString::number(volumes[i]->GetStSectorCount()->GetValue())
                + QString("U, ")
                + (QString::compare(volumes[i]->GetStAtomicWrite()->GetValue(),
                                    gpszAtomicWrTrue, Qt::CaseInsensitive) == 0
                   ? QString("true") : QString("false"))
                + QString(", ")
                + QString::number(volumes[i]->GetStInodeCount()->GetValue())
                + QString("U");

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
            toReturn += QString("}\n");
        }
        else
        {
            toReturn += QString("},\n");
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

    // Skip comment: (/\\*[\\s\\S]*?\\*/)?
    // Capture value: (\\w*)
    // Skip final whitespace: \\s*
    QRegularExpression valueExp = QRegularExpression("(/\\*[\\s\\S]*?\\*/)?\\s*(\\w*),?\\s*");

    // Same regex as valueExp, except value is enclosed in quotation marks
    QRegularExpression pathPrefixEpx = QRegularExpression("(/\\*[\\s\\S]*?\\*/)?\"(.*?)\",?\\s*");

    // The position in strVolumes to start looking for
    // the next volume
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

        for(int i = 0; i < 4; i++)
        {
            rem = valueExp.match(currStr, currVolPos);
            if(!rem.hasMatch() || rem.lastCapturedIndex() < 2)
            {
                failure = true;
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
                                     wbtnInodeCount,
                                     wbtnAtomicWrite);

        parseAndSet(newVol->GetStSectorSize(), strValues[0], notParsed,
                pathPrefix + QString(" sector size"));
        parseAndSet(newVol->GetStSectorCount(), strValues[1], notParsed,
                pathPrefix + QString(" sector count"));

        // Special case parse and set
        if(QString::compare(strValues[2], "true") == 0)
        {
            newVol->GetStAtomicWrite()->SetValue(gpszAtomicWrTrue);
        }
        else if(QString::compare(strValues[2], "false") == 0)
        {
            newVol->GetStAtomicWrite()->SetValue(gpszAtomicWrFalse);
        }
        else
        {
            notParsed += pathPrefix + QString(" atomic write supported");
        }

        parseAndSet(newVol->GetStInodeCount(), strValues[3], notParsed,
                pathPrefix + QString(" inode count"));

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
            .removeOne(volumes[index]->GetStInodeCount());
    allSettings.rbtnsUsePosix->notifyList
            .removeOne(volumes[index]->GetStInodeCount());
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
    labelVolSizeBytes->setText(FormatSize(
            static_cast<qulonglong>(volumes[activeIndex]->GetStSectorSize()->GetValue())
            * static_cast<qulonglong>(volumes[activeIndex]->GetStSectorCount()->GetValue())));
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

void VolumeSettings::listVolumes_currentRowChanged(int row)
{
    if(row < 0 || row >= volumes.count())
    {
        return;
    }

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
