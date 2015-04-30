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
#ifndef VOLUMESETTINGS_H
#define VOLUMESETTINGS_H

#include <QString>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QList>

#include "intsetting.h"
#include "strsetting.h"
#include "WarningBtn.h"

extern const char * const gpszAtomicWrTrue;
extern const char * const gpszAtomicWrFalse;

///
/// \brief  The VolumeSettings class handles basically everything associated
///         with the
///
class VolumeSettings : QObject
{
    Q_OBJECT
public:
    ///
    /// \brief  The Volume class ontains the Setting objects associated with a
    ///         volume.
    ///
    class Volume
    {
    public:
        Volume(QString name,
               WarningBtn *wbtnPathPrefix,
               WarningBtn *wbtnSectorSize,
               WarningBtn *wbtnVolSize,
               WarningBtn *wbtnInodeCount,
               WarningBtn *wbtnAtomicWrite);
        StrSetting *GetStName();
        IntSetting *GetStSectorSize();
        IntSetting *GetStSectorCount();
        IntSetting *GetStInodeCount();
        StrSetting *GetStAtomicWrite();
        bool NeedsExternalImap();

    private:
        StrSetting stName;
        IntSetting stSectorCount;
        IntSetting stInodeCount;
        IntSetting stSectorSize;
        StrSetting stAtomicWrite;
    };

    ///
    /// \brief  Constructor
    ///
    ///         Requires that ::allSettings be initialized.
    ///
    VolumeSettings(QLineEdit *pathPrefixBox,
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
                   WarningBtn *atomicWriteWarn);
    ~VolumeSettings();

    ///
    /// \brief  Gets the ::Setting for the number of volumes created
    ///
    IntSetting * GetStVolumeCount();

    ///
    /// \brief  Gets the list of volumes created
    ///
    QList<Volume *> *GetVolumes();

    ///
    /// \brief  Gets the index of the current active volume in
    ///         VolumeSettings::volumes
    ///
    int GetCurrentIndex();

    ///
    /// \brief  Sets whether the configuration is for a POSIX or FSE API.
    ///
    ///         Path prefixes (volume names) are only applicable to the POSIX
    ///         API. When ::usePosix is set to false, this setting becomes
    ///         unavailable.
    ///
    /// \param posix    Specifies whether the POSIX API is enabled or not
    ///
    void SetUsePosix(bool posix);

    ///
    /// \brief  Sets the UI to edit the volume at the given index in volumes.
    ///
    ///         Throws std::out_of_range if the given index is invalid.
    ///
    void SetActiveVolume(int index);

    ///
    /// \brief  Creates a new ::Volume
    ///
    ///         Allocates a new volume and puts it in VolumeSettings::volumes.
    ///
    void AddVolume();

    ///
    /// \brief  Removes and deletes the ::Volume at ::activeIndex.
    ///
    ///         Requires that there be more than one volume and ::activeIndex
    ///         be valid.
    ///
    void RemoveActiveVolume();

    ///
    /// \brief  Get any errors or warnings associated with the volume settings.
    ///         Called by AllSettings::GetErrors.
    ///
    void GetErrors(QStringList &errors, QStringList &warnings);

    ///
    /// \brief  Checks what configuration of imaps is required.
    ///
    /// \param imapInline   Set to true if any volume requires an inline imap;
    ///                     false otherwise.
    /// \param imapExternal Set to true if any volume requires an external imap;
    ///                     false otherwise.
    ///
    void GetImapRequirements(bool &imapInline, bool &imapExternal);

    ///
    /// \brief  Formats the volume settings as valid C code.
    ///
    /// \return A string of C code for a redconf.c file.
    ///
    QString FormatCodefileOutput();

    ///
    /// \brief  Parse C code, loading volume settings.
    ///
    ///         This function is only required to correctly load settings from
    ///         text that has been created by ::FormatCodefileOutput. It will
    ///         attempt to load settings from any
    ///
    /// \param text         A string of C code from a redconf.c file.
    /// \param notFound     A list to which to append the name of any settings
    ///                     that were expected but are not found.
    /// \param notParsed    A list to which to append the name of any settings
    ///                     that were found but could not be parsed.
    ///
    void ParseCodefile(const QString &text,
                       QStringList &notFound,QStringList &notParsed);

    ///
    /// \brief  Produces a human readable string from a number of bytes. The
    ///         format of the returned string is "xxx.xx MB (xx,xxx,xxx bytes)",
    ///         where MB could also by KB, GB, TB, or PB depending on the size.
    ///         If \p sizeInBytes is less than 1024, then the format of the
    ///         returned string is "x,xxx bytes"
    ///
    static QString FormatSize(qulonglong sizeInBytes);

private:
    void clearVolumes();
    void deselectVolume(int index);
    template<typename T>
    void parseAndSet(Setting<T> *setting, const QString &value,
                     QStringList &notParsed, const QString &humanName);
    void refreshVolumeList();
    void checkSetVolumeCount();
    bool checkCurrentIndex();
    void updateVolSizeBytes();

    // Getter: GetStNumVolumes
    IntSetting stVolumeCount;
    bool usePosix;

    // Keeps a record of how many volumes have been added. Used to create
    // names of new volumes
    unsigned volTick;

    // Getter: GetVolumes
    QList<Volume *> volumes;
    int activeIndex;

    QLineEdit *lePathPrefix;
    QSpinBox *sbVolSize;
    QSpinBox *sbInodeCount;
    QLabel *labelVolSizeBytes;
    QComboBox *cmbSectorSize;
    QComboBox *cmbAtomicWrite;
    QPushButton *btnAdd;
    QPushButton *btnRemSelected;
    QListWidget *listVolumes;

    WarningBtn *wbtnVolCount;
    WarningBtn *wbtnPathPrefix;
    WarningBtn *wbtnVolSize;
    WarningBtn *wbtnInodeCount;
    WarningBtn *wbtnSectorSize;
    WarningBtn *wbtnAtomicWrite;

private slots:
    void lePathPrefix_textChanged(const QString &text);
    void cmbSectorSize_currentIndexChanged(int index);
    void sbVolSize_valueChanged(const QString &value);
    void sbInodeCount_valueChanged(const QString &value);
    void cmbAtomicWrite_currentIndexChanged(int index);
    void listVolumes_currentRowChanged(int row);
    void btnAdd_clicked();
    void btnRemSelected_clicked();
};

///
/// \brief  Global ::VolumeSettings object.
///
///         Accessed by validators, ::Input, ::Output, etc. Initialized in
///         ::ConfigWindow constructor after ::allSettings is initialized.
///
extern VolumeSettings * volumeSettings;

#endif // VOLUMESETTINGS_H
