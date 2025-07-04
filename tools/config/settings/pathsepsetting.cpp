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
#include "pathsepsetting.h"

PathSepSetting::PathSepSetting(QString macroName, QString defaultValue,
                               std::function<Validity(QString, QString &)> validator,
                               QComboBox *cmb,
                               QLineEdit *le,
                               WarningBtn *btnWarn)
    : StrSetting(macroName, defaultValue, validator, btnWarn),
      comboBox(cmb),
      lineEdit(le)
{
    if(!cmb || !le)
    {
        throw new std::invalid_argument("cmb and le cannot be null");
    }

    setUi();
    connect(cmb, SIGNAL(currentIndexChanged(int)),
                     this, SLOT(comboBox_currentIndexChanged(int)));
    connect(le, SIGNAL(textChanged(QString)),
                     this, SLOT(lineEdit_textChanged(QString)));
}

bool PathSepSetting::TryParse(const QString &toParse, QString &out)
{
    QString substr = toParse;
    if(toParse.startsWith('\'') && toParse.endsWith('\''))
    {
        substr = toParse.mid(1, toParse.length() - 2);
    }

    QString msg;
    Validity v = CheckValid(substr, msg);

    if(v == Invalid)
    {
        return false;
    }
    else
    {
        out = substr;
        return true;
    }
}

void PathSepSetting::comboBox_currentIndexChanged(int i)
{
    if(i == optionCustomIndex)
    {
        // Show line edit; don't do further processing
        lineEdit->setVisible(true);
        ProcessInput(lineEdit->text());
    }
    else
    {
        lineEdit->setVisible(false);
        ProcessInput(comboBox->currentText());
    }
}

void PathSepSetting::setUi()
{
    if(QString::compare(value, "/") == 0)
    {
        comboBox->setCurrentText("/");
        lineEdit->setVisible(false);
    }
    else if(QString::compare(value, "\\") == 0
            || QString::compare(value, "\\\\") == 0) //double-escaped backslash
    {
        comboBox->setCurrentText("\\");
        lineEdit->setVisible(false);
    }
    else
    {
        // Set lineEdit text before setting comboBox index so as to avoid resetting
        // value in comboBox_currentIndexChanged
        lineEdit->setText(value);
        comboBox->setCurrentIndex(optionCustomIndex);
        lineEdit->setVisible(true);
    }
}

void PathSepSetting::lineEdit_textChanged(const QString &text)
{
    ProcessInput(text);
}
