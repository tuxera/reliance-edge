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
#ifndef SETTINGBASE_H
#define SETTINGBASE_H

#include <QList>

#include "validity.h"
#include "notifiable.h"

///
/// \brief  The SettingBase class is a base class for Setting<T>, allowing
///         instances of Setting<T> derived classes to be treated as inheriting
///         from the same type.
///
class SettingBase : public Notifiable
{
public:
    ///
    /// \brief  Checks whether the current value held by this Setting is valid;
    ///         sets any warnings in the user interface, and returns the
    ///         Validity.
    ///
    /// \param msg  Set to a human readable error or warning message if
    ///             Validity::Invalid or Validity::Warning is returned.
    ///
    virtual Validity RecheckValid(QString &msg) = 0;

    ///
    /// \brief  ::SettingBase objects added to this list will be notified when
    ///         any value held by this ::SettingBase changes. This allows
    ///         warning buttons to be updated to indicate invalid values the
    ///         moment they become invalid due to a dependency (e.g. the
    ///         buffer count is no longer valid due to POSIX "rename" being
    ///         checked).
    ///
    QList<Notifiable *> notifyList;
};

#endif // SETTINGBASE_H
