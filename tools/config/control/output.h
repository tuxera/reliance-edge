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
#ifndef OUTPUT_H
#define OUTPUT_H

#include <QWidget>
#include <QObject>
#include <QList>
#include <QString>

#include "ui/errordialog.h"
#include "ui/filedialog.h"
#include "settings/settingbase.h"

///
/// \brief  The Output class controls the processes of reporting invalid values
///         to the user and saving configuration files
///
class Output : public QObject
{
    Q_OBJECT
public:
    ///
    /// \brief  Represents the result of a call to Output::TrySave or
    ///         Output::ShowErrors
    ///
    enum Result {
        /// Operation was successful
        OutResultSuccess,

        /// Operation cancelled because one or more values is invalid.
        OutResultInvalid,

        /// Operation cancelled by the user
        OutResultUserCancelled,

        /// The dialog shown by Output::ShowErrors has been dismissed
        OutResultInfoDismissed,

        /// Operation cancelled because a save operation is already in progress.
        /// This result is not expected to occur and may indicate an internal
        /// error.
        OutResultErrorBusy,

        /// Failure opening selected file or writing out information
        OutResultFileError
    };

    Output(QWidget *parentWin);

    ///
    /// \brief  Checks for invalid values, prompts user to select a location to
    ///         save configuration files, and attempts to save them.
    ///
    ///         The save operation first checks for invalid valid values. If any
    ///         invalid values are found, an ::ErrorDialog is displayed and then
    ///         the operation is cancelled. If warning values are found but no
    ///         errors, an ::ErrorDialog is shown prompting the user to continue
    ///         or cancel.
    ///
    ///         The user is then shown a ::FileDialog twice to save the
    ///         relconf.h and relconf.c files. If valid file paths are selected,
    ///         then the settings are outputted to those files for use in
    ///         compiling the Reliance Edge source.
    ///
    ///         The result of this operation is emitted by the signal
    ///         Output::results.
    ///
    /// \param codefilePath The path at which to save the .c configuration file.
    ///                     If this is null or if an attempt to save the file
    ///                     with this name fails, save dialogs will be shown.
    /// \param headerPath   The path at which to save the .h configuration file.
    ///                     If this is null or if an attempt to save the file
    ///                     with this name fails, save dialogs will be shown.
    ///                     This must not be null if codefilePath is not null,
    ///                     and vice versa.
    ///
    void TrySave(const QString & headerPath, const QString & codefilePath);

    ///
    /// \brief  Checks for invalid and shows them to the user in a non-blocking
    ///         dialog.
    ///
    /// \param showIfNoErrors   If specified and set to true, the dialog will be
    ///                         shown even if no errors were found, reporting
    ///                         that no errors were found. Otherwise the
    ///                         function returns without showing the dialog if
    ///                         no errors are found.
    ///
    void ShowErrors(bool showIfNoErrors = false);

private:
    // Called once settings validity is verified
    void doOutput();

    QWidget *parentWindow;
    ErrorDialog *errorDialog;
    FileDialog *fileDialog;

    // Set to true while the save operation is active. The ConfigWindow UI
    // should be blocked while isSaving is true so that overlapping save
    // calls are not possible.
    bool isSaving;

    // Used to save parameters from TrySave() for doOutput to use. We do this
    // instead of passing parameters to doOutput() because doOutput() is also
    // called by errorDialog_results().
    QString currHeaderPath;
    QString currCodefilePath;

signals:
    void results(Output::Result r, const QString & headerPath, const QString & codefilePath);

private slots:
    void errorDialog_results(ErrorDialog::Result r);
};

#endif // OUTPUT_H
