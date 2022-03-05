/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2022 Tuxera US Inc.
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
    comply with the terms of the GPLv2 license must obtain a commercial
    license before incorporating Reliance Edge into proprietary software
    for distribution in any form.

    Visit https://www.tuxera.com/products/reliance-edge/ for more information.
*/
#include <stdexcept>

#include "checkedsbsetting.h"

CheckedSbSetting::CheckedSbSetting(QString macroName, unsigned long defaultValue,
                     std::function<Validity(unsigned long, QString &)> validator,
                     QSpinBox *sb,
                     QCheckBox *cb,
                     bool isChecked,
                     bool enableWhenChecked,
                     WarningBtn * btnWarn)
    : IntSetting(macroName, defaultValue, validator, btnWarn),
      spinbox(sb),
      checkBox(cb)
{
    if(!sb)
    {
        throw new std::invalid_argument("sb cannot be null");
    }
    if(!cb)
    {
        throw new std::invalid_argument("cb cannot be null");
    }

    this->enableWhenChecked = enableWhenChecked;
    this->isChecked = isChecked;

    /*  UI must be updated before the event handlers are attached
    */
    setUi();

    /*  Set the event handlers for UI controls associated with this object
    */
    connect(sb, SIGNAL(valueChanged(QString)), this, SLOT(spinbox_valueChanged(QString)));
    connect(cb, SIGNAL(stateChanged(int)), this, SLOT(checkBox_stateChanged(int)));
}

bool CheckedSbSetting::TryParse(const QString &toParse, unsigned long &out)
{
    unsigned long result;
    bool success = IntSetting::TryParse(toParse, result);

    // QSpinBox::setValue takes an int, so no values higher
    // than INT_MAX are permitted.
    if(success && result <= INT_MAX)
    {
        out = result;
        return true;
    }
    return false;
}

/*  Refresh the UI controls associated with this object
*/
void CheckedSbSetting::setUi()
{
    QString msg;
    spinbox->setEnabled(isChecked == enableWhenChecked);
    spinbox->setValue(static_cast<int>(value));
    checkBox->setCheckState(isChecked ? Qt::Checked : Qt::Unchecked);
    RecheckValid(msg);
}

/*  Spinbox value changed event handler
*/
void CheckedSbSetting::spinbox_valueChanged(const QString & text)
{
    ProcessInput(text);
}

/*  Checkbox state changed event handler
*/
void CheckedSbSetting::checkBox_stateChanged(int state)
{
    /*  Record the checked state and refresh the UI
    */
    Q_ASSERT((state == Qt::Checked) || (state == Qt::Unchecked));
    isChecked = state == Qt::Checked;
    setUi();
}

unsigned long CheckedSbSetting::GetValue(void)
{
    /*  Return the value if the object is enabled, otherwise return zero.
    */
    if(isChecked == enableWhenChecked)
    {
        return value;
    }
    else
    {
        return 0;
    }
}

void CheckedSbSetting::SetValue(unsigned long arg, bool updateUi)
{
    /*  When loading values, interpret a zero as disabling the value
    */
    if(arg == 0)
    {
        if(updateUi)
        {
            isChecked = !enableWhenChecked;
            value = defValue;
            setUi();
        }
    }
    else
    {
        if(updateUi)
        {
            isChecked = enableWhenChecked;
        }
        IntSetting::SetValue(arg, updateUi);
    }
}

Validity CheckedSbSetting::checkValue(unsigned long value, QString &msg)
{
    if(isChecked == enableWhenChecked)
    {
        return IntSetting::checkValue(value, msg);
    }
    else
    {
        if(btnWarning)
        {
            btnWarning->Set(Valid, msg);
        }
        return Valid;
    }
}
