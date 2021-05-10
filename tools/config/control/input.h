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
#ifndef INPUT_H
#define INPUT_H

#include <QWidget>
#include <QObject>
#include <QFileDialog>
#include <QMessageBox>

#include "ui/filedialog.h"

///
/// \brief  The Input class controls the process of loading configuration files.
///
class Input : public QObject
{
    Q_OBJECT
public:
    ///
    /// \brief  Represents the result of a call to Input::TryLoad.
    ///
    enum Result {
        /// Operation was successful
        InResultSuccess,

        /// Operation was cancelled by the user (i.e. file dialog was closed
        /// without selecting a file)
        InResultUserCancelled,

        /// Operation was cancelled to avoid hanging the system because an
        /// unreasonably large file was selected.
        InResultErrorHugeFile,

        /// There was an error reading the file given by the user
        InResultFileError
    };

    explicit Input(QWidget *parentWin);

    ///
    /// \brief  Prompts the user to select existing configuration files and
    ///         attempts to load them.
    ///
    ///         The user is shown a ::FileDialog twice, once to select an
    ///         existing redconf.h file and again to select a redconf.c file.
    ///         If the user selects valid files, they are loaded into the
    ///         ::ConfigWindow UI.
    ///
    ///         The result of this operation is emitted by the signal
    ///         Input::results.
    ///
    void TryLoad();

private:
    bool getFile(const QString &filePath, QString &fileContent);

    FileDialog *fileDialog;
    QWidget *parentWindow;
    QMessageBox *messageBox;

signals:
    void results(Input::Result r, const QString & headerPath, const QString & codefilePath);
};

#endif // INPUT_H
