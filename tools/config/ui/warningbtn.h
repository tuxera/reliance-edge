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
#ifndef WARNINGBTN_H
#define WARNINGBTN_H

#include <QWidget>
#include <QIcon>
#include <QMouseEvent>

#include "validity.h"
#include "ui_warningbtn.h"

namespace Ui {
class WarningBtn;
}

///
/// \brief  The WarningBtn is a UI element which shows an error or warning icon
///         to the user if a setting value is invalid or not recommended.
///
class WarningBtn : public QWidget
{
    Q_OBJECT

public:
    explicit WarningBtn(QWidget *parent = 0);
    ~WarningBtn();

    ///
    /// \brief  Sets this ::WarningBtn to display a yellow warning icon
    ///         representing the given message.
    ///
    /// \param msg  The message describing this warning.
    ///
    void SetWarn(QString msg);

    ///
    /// \brief  Sets this ::WarningBtn to display a red error icon representing
    ///         the given message.
    ///
    /// \param msg  The message describing this error.
    ///
    void SetError(QString msg);

    ///
    /// \brief  Sets this ::WarningBtn to display no icon and show no message.
    ///
    void Clear(void);

    ///
    /// \brief  Sets the behavior of this ::WarningBtn
    ///
    /// \param v    Specifies the state this ::WarningBtn is to report.
    /// \param msg  Specifies the message describing the validity reported. This
    ///             will not be used if \p v is equal to Validity::Valid.
    ///
    void Set(Validity v, QString msg);

protected:
    bool event(QEvent *e) override;

private:
    static QIcon *iconError;
    static QIcon *iconWarn;

    QString currMsg;
    Ui::WarningBtn *ui;

signals:
    void clicked();
};

#endif // WARNINGBTN_H
