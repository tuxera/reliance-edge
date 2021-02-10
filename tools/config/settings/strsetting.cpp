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

#include "strsetting.h"

StrSetting::StrSetting(QString macroName, QString defaultValue,
                         std::function<Validity(QString, QString &)> validator,
                         WarningBtn * btnWarn)
    : Setting<QString>(macroName, defaultValue, validator),
      btnWarning(btnWarn)
{
}

void StrSetting::ProcessInput(const QString & text)
{
    QString msg;
    checkValue(text, msg);
    SetValue(text, false);
}

Validity StrSetting::checkValue(QString value, QString &msg)
{
    Validity v = CheckValid(value, msg);
    if(btnWarning)
    {
        btnWarning->Set(v, msg);
    }
    return v;
}

bool StrSetting::TryParse(const QString &toParse, QString & out)
{
    if(toParse.isNull())
    {
        return false;
    }
    else
    {
        out = toParse;
        return true;
    }
}
