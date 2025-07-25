/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2025 Tuxera US Inc.
                      All Rights Reserved Worldwide.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; use version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but "AS-IS," WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, see <https://www.gnu.org/licenses/>.
*/
/*  Businesses and individuals that for commercial or other reasons cannot
    comply with the terms of the GPLv2 license must obtain a commercial
    license before incorporating Reliance Edge into proprietary software
    for distribution in any form.

    Visit https://www.tuxera.com/products/tuxera-edge-fs/ for more information.
*/
#ifndef CHECKEDSPSETTING_H
#define CHECKEDSPSETTING_H

#include <QString>
#include <QSpinBox>
#include <QCheckBox>

#include "ui/warningbtn.h"
#include "intsetting.h"

///
/// \brief  The CheckedSbSetting class manages settings that use a QSpinBox
///         for user input and hold an unsigned integer value.
///
class CheckedSbSetting : public IntSetting
{
    Q_OBJECT
public:
    ///
    /// \brief Constructor
    ///
    /// \param macroName    Passed to Setting<T> constructor
    /// \param defaultValue Passed to Setting<T> constructor
    /// \param validator    Passed to Setting<T> constructor
    /// \param sb           The QSpinBox associated with this setting
    /// \param cb           The QCheckBox associated with this setting
    /// \param isChecked    Checkbox initial setting is checked
    /// \param enableWhenChecked  Object is enabled with checkbox checked
    /// \param btnWarn      Optional: passed to the IntSetting constructor
    ///
    CheckedSbSetting(QString macroName, unsigned long defaultValue,
              std::function<Validity(unsigned long, QString &)> validator,
              QSpinBox *sb,
              QCheckBox *cb,
              bool isChecked,
              bool enableWhenChecked = true,
              WarningBtn * btnWarn = 0);

protected:
    virtual bool TryParse(const QString &toParse, unsigned long &out);
    virtual void setUi() override;
    virtual unsigned long GetValue() override;
    virtual void SetValue(unsigned long arg, bool updateUi = true) override;
    virtual Validity checkValue(unsigned long value, QString &msg) override;

private:
    QSpinBox *spinbox;
    QCheckBox * checkBox;
    bool enableWhenChecked;
    bool isChecked;

private slots:
    void spinbox_valueChanged(const QString & text);
    void checkBox_stateChanged(int state);
};

#endif // #ifndef CHECKEDSPSETTING_H
