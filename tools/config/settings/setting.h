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
#ifndef SETTINGS_H
#define SETTINGS_H

#include <functional>
#include <exception>
#include <stdexcept>

#include <QString>
#include <QList>

#include "ui/warningbtn.h"
#include "debug.h"
#include "validity.h"
#include "settingbase.h"


///
/// \brief  The Setting class is used to represent settings displayed by the UI.
///         This class controls validity checking and value storage; derrived
///         classes may also control user interface interaction.
///
/// \tparam T   The type which this setting holds
///
template<typename T>
class Setting : public SettingBase
{
public:
    ///
    /// \brief Constructor
    ///
    /// \param macroName    The C macro name associated with this setting. May
    ///                     be null or empty if there is no associated macro.
    /// \param defaultValue The initial value to which to set this setting
    /// \param validator    A function from validators.h to be used to validate
    ///                     this setting
    ///
    Setting(QString macroName, T defaultValue,
            std::function<Validity(T, QString &)> validator);

    ///
    /// \brief  CheckValid Checks whether the given value is valid for this
    ///         setting
    ///
    /// \param arg  Value to check
    /// \param msg  Set to a human readable error or warning message if the
    ///             given value is not valid
    ///
    /// \return Returns one of Validity::Valid, Validity::Warning, or
    ///         Validity::Invalid
    ///
    Validity CheckValid(T arg, QString &msg);

    Validity RecheckValid(QString &msg) override;

    void Notify() override;

    ///
    /// \brief  Sets the value of this Setting. Does not test the validity of
    ///         the given value.
    ///
    /// \param arg      Value to set
    ///
    /// \param updateUi If set to true or unspecified, any ui elements will be
    ///                 updated after the value is set
    ///
    void SetValue(T arg, bool updateUi = true);

    ///
    /// \brief  Returns the current value held by this Setting
    ///
    T GetValue(void)
    {
        return value;
    }

    ///
    /// \brief  Returns the macro name associated with this setting
    ///
    QString GetMacroName()
    {
        return name;
    }

    ///
    /// \brief  Tries to parse the given string into a valid value for this
    ///         setting.
    ///
    /// \param toParse  String data to parse
    /// \param out      Reference to be set to the parsed value. Indeterminate
    ///                 if false is returned.
    ///
    /// \return True if successful, false if not.
    ///
    virtual bool TryParse(const QString &toParse, T &out) = 0;

protected:
    ///
    /// \brief  Verifies that given value value is valid. Returns the validity
    ///         and sets any warning UI elements accordingly.
    ///
    /// \param value    Value to test
    /// \param msg      Will be set to a human readable message if the returned
    ///                 value is not Validity::Valid
    ///
    /// \return Returns the validity of the given value
    ///
    virtual Validity checkValue(T value, QString &msg) = 0;

    ///
    /// \brief setUi Sets any UI elements
    ///
    virtual void setUi();

    const QString name;
    const T defValue;
    T value;

private:
    // Expected to be set to one of the functions declared in validators.h
    // at initialization.
    const std::function<Validity(T, QString &)> validateFn;
};


template<typename T>
Setting<T>::Setting(QString macroName, T defaultValue,
                    std::function<Validity(T, QString &)> validator)
    : name(macroName),
      defValue(defaultValue),
      value(defaultValue),
      validateFn(validator)
{
    if(!validateFn)
    {
        throw new std::invalid_argument("Unassigned function pointer passed to Setting::Setting");
    }
}

template<typename T>
Validity Setting<T>::CheckValid(T arg, QString &msg)
{
    return validateFn(arg, msg);
}

template<typename T>
Validity Setting<T>::RecheckValid(QString &msg)
{
    return checkValue(value, msg);
}

template<typename T>
void Setting<T>::Notify()
{
    QString msg;
    checkValue(value, msg);
}

template<typename T>
void Setting<T>::SetValue(T arg, bool updateUi)
{
    value = arg;

    // Recalculate validity for any Settings that are dependent
    // on this one
    if(!notifyList.isEmpty())
    {
        for(QList<Notifiable *>::const_iterator i = notifyList.cbegin(); i < notifyList.cend(); ++i)
        {
            if(*i == NULL)
            {
                continue;
            }
            (*i)->Notify();
        }
    }

    if(updateUi)
    {
        setUi();
    }
}

// Blank method for Settings that have no UI components
template<typename T>
void Setting<T>::setUi()
{
    //Do nothing
}

#endif // SETTINGS_H
