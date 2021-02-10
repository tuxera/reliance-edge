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
#ifndef NOTIFIABLE_H
#define NOTIFIABLE_H

///
/// \brief  The Notifiable class is an interface for objects to be notified of
///         changes to settings. This is an alternative to the Qt signal/slot
///         system which is required because the ::Setting<T> class is not
///         allowed to inherit from QObject.
///
class Notifiable
{
public:
    ///
    /// \brief  Notify of an event that requires this to reprocess information.
    ///
    ///         In a normal ::Setting object, this redirects to
    ///         Setting::RecheckValid.
    ///
    virtual void Notify() = 0;

    ///
    /// \brief  Virtual inline destructor provided to ensure child destructors
    ///         are called. (And, more realistically, to calm GCC's
    ///         Wdelete-non-virtual-dtor warning.)
    ///
    inline virtual ~Notifiable() { }
};

#endif // NOTIFIABLE_H

