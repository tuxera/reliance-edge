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
#include <stdexcept>

#include "cmbintsetting.h"

CmbIntSetting::CmbIntSetting(QString macroName, unsigned long defaultValue,
                             std::function<Validity(unsigned long, QString &)> validator,
                             QComboBox *cmb,
                             WarningBtn * btnWarn)
    : IntSetting(macroName, defaultValue, validator, btnWarn),
      comboBox(cmb)
{
    if(!cmb)
    {
        throw new std::invalid_argument("cmb cannot be null");
    }

    setUi();
    connect(cmb, SIGNAL(currentIndexChanged(QString)),
                     this, SLOT(combobox_currentIndexChanged(QString)));
}

void CmbIntSetting::setUi()
{
    // Use QLocale to add commas; e.g. 32768 -> 32,768
    static const QLocale l(QLocale::English, QLocale::UnitedStates);
    comboBox->setCurrentText(l.toString((qulonglong) value));
}

void CmbIntSetting::combobox_currentIndexChanged(const QString & text)
{
    ProcessInput(text);
}
