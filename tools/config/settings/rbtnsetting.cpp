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

#include "rbtnsetting.h"

RbtnSetting::RbtnSetting(QString macroName, bool defaultValue,
          std::function<Validity(bool, QString &)> validator,
          QRadioButton * rbtn,
          WarningBtn * btnWarn)
    : BoolSetting(macroName, defaultValue, validator, btnWarn),
      radioButton(rbtn)
{
    if(!rbtn)
    {
        throw new std::invalid_argument("rbtn cannot be null");
    }

    setUi();
    connect(rbtn, SIGNAL(toggled(bool)),
                     this, SLOT(radioButton_toggled(bool)));
}

void RbtnSetting::setUi()
{
    radioButton->setChecked(value);
}

void RbtnSetting::radioButton_toggled(bool input)
{
    ProcessInput(input);
}

