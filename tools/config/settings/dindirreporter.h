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
#ifndef DINDIRREPORTER_H
#define DINDIRREPORTER_H

#include <QLabel>

#include "notifiable.h"

///
/// \brief  The DindirReporter class handles the UI label component that reports
///         the number of double indirect pointers at the current configuration.
///
///         This class is instantiated and deleted by the ::ConfigWindow.
///         Although not a descendent of Setting<T>, this class functions at a
///         similar level as the settings and is thus included in the settings
///         module.
///
class DindirReporter : public Notifiable
{
public:
    ///
    /// \brief  Constructor
    ///
    ///         Requires that ::allSettings be initialized.
    ///
    /// \param dindirLabel  The ::QLabel to which to print the number of double
    ///                     indirect pointers at current configuration.
    ///
    DindirReporter(QLabel *dindirLabel);

    void Notify() override;

private:
    QLabel *label;
};

#endif // DINDIRREPORTER_H
