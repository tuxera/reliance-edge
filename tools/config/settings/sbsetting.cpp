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
#include <stdexcept>

#include "sbsetting.h"

SbSetting::SbSetting(QString macroName, unsigned long defaultValue,
                     std::function<Validity(unsigned long, QString &)> validator,
                     QSpinBox *sb,
                     WarningBtn * btnWarn)
    : IntSetting(macroName, defaultValue, validator, btnWarn),
      spinbox(sb)
{
    if(!sb)
    {
        throw new std::invalid_argument("sb cannot be null");
    }

    setUi(); //important: before connections are made
    connect(sb, SIGNAL(valueChanged(QString)),
                     this, SLOT(spinbox_valueChanged(QString)));
}

bool SbSetting::TryParse(const QString &toParse, unsigned long &out)
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

void SbSetting::setUi()
{
    // Should be safe to cast to int since values greater
    // than INT_MAX cause parse failure; as long as default
    // value is not greater than INT_MAX.
    spinbox->setValue(static_cast<int>(value));
}

void SbSetting::spinbox_valueChanged(const QString & text)
{
    ProcessInput(text);
}
