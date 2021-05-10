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
#ifndef STRSETTING_H
#define STRSETTING_H

#include <QString>

#include "ui/warningbtn.h"
#include "setting.h"

///
/// \brief  Class for settings that may be represented using a string type.
///         Inherits from QObject and Setting<T>, where `T` is set to `QString`
///
class StrSetting : public QObject, public Setting<QString>
{
    Q_OBJECT
public:
    ///
    /// \brief Constructor
    ///
    /// \param macroName    Passed to Setting<T> constructor
    /// \param defaultValue Passed to Setting<T> constructor
    /// \param validator    Passed to Setting<T> constructor
    /// \param btnWarn      Optional: the warning button associated with this
    ///                     setting
    ///
    StrSetting(QString macroName, QString defaultValue,
               std::function<Validity(QString, QString &)> validator,
               WarningBtn * btnWarn = 0);

    bool TryParse(const QString &toParse, QString & out) override;

    ///
    /// \brief  Sets the value of this setting, checking validity and setting
    ///         any associated warning button
    ///
    /// \param input    The value to set
    ///
    void ProcessInput(const QString & input);

protected:
    Validity checkValue(QString value, QString &msg);

    WarningBtn *btnWarning;
};

#endif // STRSETTING_H
