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

#include "intsetting.h"

IntSetting::IntSetting(QString macroName, unsigned long defaultValue,
                             std::function<Validity(unsigned long, QString &)> validator,
                             WarningBtn * btnWarn)
    : Setting<unsigned long>(macroName, defaultValue, validator),
      btnWarning(btnWarn)
{
}

void IntSetting::ProcessInput(const QString & text)
{
    unsigned long value;
    bool success = TryParse(text, value);
    if(!success)
    {
        if(btnWarning)
        {
            btnWarning->SetError("Error parsing selected value");
        }
        throw new std::invalid_argument("Error parsing value");
    }

    QString msg;
    checkValue(value, msg);
    SetValue(value, false);
}

Validity IntSetting::checkValue(unsigned long value, QString &msg)
{
    Validity v = CheckValid(value, msg);
    if(btnWarning)
    {
        btnWarning->Set(v, msg);
    }
    return v;
}

bool IntSetting::TryParse(const QString &toParse, unsigned long & out)
{
    if(toParse.isNull() || toParse.isEmpty())
    {
        return false;
    }

    QString strParsing = toParse;
    strParsing.remove(','); //Allow comma-divided nums (e.g. 1,232,600)
    strParsing.remove('U'); //Allow explicit unsigned notation

    bool success;
    unsigned long result;

    // Second argument 0 -> "C language convention is used"
    // according to qt-project docs. e.g. 0x___ read as hex
    result = strParsing.toULong(&success, 0);

    if(success)
    {
        out = result;
    }
    return success;
}
