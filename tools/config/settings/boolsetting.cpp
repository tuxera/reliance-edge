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

#include <QString>

#include "boolsetting.h"

BoolSetting::BoolSetting(QString macroName, bool defaultValue,
          std::function<Validity(bool, QString &)> validator,
          WarningBtn * btnWarn)
    : Setting<bool>(macroName, defaultValue, validator),
      btnWarning(btnWarn)
{
}

void BoolSetting::ProcessInput(bool input)
{
    QString msg;
    checkValue(input, msg);
    SetValue(input, false);
}

Validity BoolSetting::checkValue(bool value, QString &msg)
{
    Validity v = CheckValid(value, msg);
    if(btnWarning)
    {
        btnWarning->Set(v, msg);
    }
    return v;
}

bool BoolSetting::TryParse(const QString &toParse, bool & out)
{
    // Currently 0 and 1 are the only values used for boolean
    // REDCONF settings, but support true and false for common
    // sense reasons.
    if(QString::compare(toParse, "1") == 0
       || QString::compare(toParse, "true", Qt::CaseInsensitive) == 0)
    {
        out = true;
        return true;
    }
    else if (QString::compare(toParse, "0") == 0
             || QString::compare(toParse, "false", Qt::CaseInsensitive) == 0)
    {
        out = false;
        return true;
    }
    else return false;
}
