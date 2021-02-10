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
#ifndef ERRORDIALOG_H
#define ERRORDIALOG_H

#include <QDialog>
#include <QList>
#include <QString>

namespace Ui {
class ErrorDialog;
}

///
/// \brief  An ::ErrorDialog is a dialog with a list of errors and/or warnings
///         to be shown to the user.
///
///         The dialog may block user action and require a decision to be made
///         (see ErrorDialog::ShowErrorsAction) or it may allow a user to
///         continue working with the parent window (see
///         ErrorDialog::ShowErrorsInfo)
///
class ErrorDialog : public QDialog
{
    Q_OBJECT
public:
    ///
    /// \brief  Represents the user choice that terminates an ErrorDialog.
    ///
    enum Result {
        /// User has chosen to continue the operation.
        EdResultContinue = 0,

        /// User has chosen to cancel the operation.
        EdResultCancel = 1,

        /// User has chosen the only available option
        EdResultOk = 2
    };

    explicit ErrorDialog(QWidget *parent = 0);

    ~ErrorDialog();

    ///
    /// \brief  Sets the message that is displayed above the list of errors and
    ///         warnings.
    ///
    /// \param text The text to use for the message.
    ///
    void SetErrorText(const QString &text);

    ///
    /// \brief  Opens this ::ErrorDialog and shows the given errors and
    ///         warnings. Does not block user from continuing to interact with
    ///         the open ::ConfigWindow. Only the "Continue" button is made
    ///         available to the user.
    ///
    /// \param errors   The list of error messages to show. Shown at the top,
    ///                 with a red icon beside each entry.
    /// \param warnings The list of warning messages to show. Shown below any
    ///                 error messages, with a yellow icon beside each entry.
    ///
    void ShowErrorsInfo(QStringList errors, QStringList warnings = QStringList());


    ///
    /// \brief  Opens this ::ErrorDialog and shows the given errors and
    ///         warnings. The user is given the option to continue or to cancel.
    ///         The open ::ConfigWindow is blocked until the user clicks one of
    ///         the options.
    ///
    /// \param errors   The list of error messages to show. Shown at the top,
    ///                 with a red icon beside each entry.
    /// \param warnings The list of warning messages to show. Shown below any
    ///                 error messages, with a yellow icon beside each entry.
    ///
    void ShowErrorsAction(QStringList errors, QStringList warnings);

signals:
    ///
    /// \brief  Called when the user presses the button and the dialog box
    ///         closes
    /// \param r    Describes the user selection
    ///
    void results(ErrorDialog::Result r);

private:
    void showErrors(QStringList errors, QStringList warnings);

    Ui::ErrorDialog *ui;

private slots:
    void btnOk_clicked();
    void btnContinue_clicked();
    void btnCancel_clicked();
};

#endif // ERRORDIALOG_H
