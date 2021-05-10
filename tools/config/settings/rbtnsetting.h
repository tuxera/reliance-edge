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
#ifndef RBTNSETTING_H
#define RBTNSETTING_H

#include <QRadioButton>

#include "boolsetting.h"

///
/// \brief  The RbtnSetting class manages settings that use a QRadioButton for
///         user input and hold a Boolean value.
///
class RbtnSetting : public BoolSetting
{
    Q_OBJECT
public:
    ///
    /// \brief Constructor
    ///
    /// \param macroName    Passed to Setting<T> constructor
    /// \param defaultValue Passed to Setting<T> constructor
    /// \param validator    Passed to Setting<T> constructor
    /// \param rbtn         The QRadioButton associated with this setting
    /// \param btnWarn      Optional: passed to the BoolSetting constructor
    ///
    RbtnSetting(QString macroName, bool defaultValue,
                std::function<Validity(bool, QString &)> validator,
                QRadioButton * rbtn,
                WarningBtn * btnWarn = 0);

protected:
    virtual void setUi() override;

private:
    QRadioButton *radioButton;

private slots:
    void radioButton_toggled(bool input);
};

#endif // RBTNSETTING_H
