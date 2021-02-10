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
#ifndef CMBINTSETTING_H
#define CMBINTSETTING_H

#include <QString>
#include <QComboBox>

#include "ui/warningbtn.h"
#include "intsetting.h"

///
/// \brief  The CmbStrSetting class manages settings that use a QComboBox for
///         user input and hold an unsigned integer value.
///
class CmbIntSetting : public IntSetting
{
    Q_OBJECT
public:
    ///
    /// \brief Constructor
    ///
    /// \param macroName    Passed to Setting<T> constructor
    /// \param defaultValue Passed to Setting<T> constructor
    /// \param validator    Passed to Setting<T> constructor
    /// \param cmb          The QComboBox associated with this setting
    /// \param btnWarn      Optional: passed to the IntSetting constructor
    ///
    CmbIntSetting(QString macroName, unsigned long defaultValue,
               std::function<Validity(unsigned long, QString &)> validator,
               QComboBox *cmb,
               WarningBtn * btnWarn = 0);

protected:
    virtual void setUi() override;

private:
    QComboBox *comboBox;

private slots:
    void combobox_currentIndexChanged(const QString & text);
};
#endif // CMBINTSETTING_H
