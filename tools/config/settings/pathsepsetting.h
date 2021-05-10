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
#ifndef PATHSEPSETTING_H
#define PATHSEPSETTING_H

#include <QString>
#include <QComboBox>
#include <QLineEdit>

#include "ui/warningbtn.h"
#include "strsetting.h"

///
/// \brief  The PathSepSetting class is a special case class that manages the
///         path separator character setting. It uses a QComboBox and a
///         QLineEdit for user input and holds a string value.
///
class PathSepSetting : public StrSetting
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
    /// \param le           The QLineEdit associated with this setting
    /// \param btnWarn      Optional: passed to the StrSetting constructor
    ///
    PathSepSetting(QString macroName, QString defaultValue,
                   std::function<Validity(QString, QString &)> validator,
                   QComboBox *cmb,
                   QLineEdit *le,
                   WarningBtn * btnWarn = 0);

    bool TryParse(const QString &toParse, QString &out) override;

protected:
    virtual void setUi() override;

private:
    // The index of the "Custom" entry in comboBox
    const static int optionCustomIndex = 2;
    QComboBox *comboBox;
    QLineEdit *lineEdit;

private slots:
    void comboBox_currentIndexChanged(int i);
    void lineEdit_textChanged(const QString & text);
};

#endif // PATHSEPSETTING_H
