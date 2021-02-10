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
#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QFileDialog>

///
/// \brief  A child of QFileDialog designed for saving and loading configuration
///         files.
///
class FileDialog : protected QFileDialog
{
    Q_OBJECT
public:
    ///
    /// \brief  Constructs a ::FileDialog.
    ///
    /// \param parentWindow A pointer to the parent ::ConfigWindow
    /// \param amode        Must be one of AcceptMode::AcceptSave or
    ///                     AcceptMode::AcceptOpen. Specifies whether this
    ///                     ::FileDialog will select files to open or locations
    ///                     to save files.
    /// \param fmode        The FileMode to use for this ::FileDialog. Passed to
    ///                     QFileDialog::setFileMode.
    ///
    FileDialog(QWidget *parentWindow, AcceptMode amode, FileMode fmode);

    ///
    /// \brief  Shows this ::FileDialog, asking the user to choose a redconf.h
    ///         file.
    ///
    /// \param defaultPath  A path to a file to be selected as a default, or
    ///                     QString::null
    ///
    /// \return Returns the path to the file chosen by the user, or
    ///         QString::null if no file was selected.
    ///
    QString ShowGetHeader(const QString &defaultPath);

    ///
    /// \brief  Shows this ::FileDialog, asking the user to choose a redconf.c
    ///         file.
    ///
    /// \param defaultPath  A path to a file to be selected as a default, or
    ///                     QString::null
    ///
    /// \return Returns the path to the file chosen by the user, or
    ///         QString::null if no file was selected.
    ///
    QString ShowGetCodefile(const QString &defaultPath);

private:
    QString showFileDialog();

    QStringList codefileNameFilters;
    QStringList headerNameFilters;
    AcceptMode acceptMode;

    // Used to open every FileDialog to the last used location.
    // Initialized to the user's home directory.
    static QString defaultDir;

private slots:
    void on_windowTitleChanged(const QString &title);
};

#endif // FILEDIALOG_H
