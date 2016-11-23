/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                   Copyright (c) 2014-2015 Datalight, Inc.
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
    comply with the terms of the GPLv2 license may obtain a commercial license
    before incorporating Reliance Edge into proprietary software for
    distribution in any form.  Visit http://www.datalight.com/reliance-edge for
    more information.
*/
#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QErrorMessage>
#include <QMessageBox>

#include "ui/configwindow.h"
#include "output.h"
#include "input.h"

///
/// \brief  The Application class is a child class of QApplication. It runs the
///         program and owns instances of the ConfigWindow, Output, and Input
///         classes.
///
class Application : protected QApplication
{
    Q_OBJECT
public:
    ///
    /// \brief  Instantiates an Application
    ///
    /// \param argc Passed to QApplication constructor
    /// \param argv Passed to QApplication constructor
    ///
    Application(int &argc, char *argv[]);

    ~Application();

    ///
    /// \brief  Runs the configuration utility
    ///
    /// \return Returns 0 if success or non-zero if error
    ///
    int Run();

private:
    ConfigWindow *configWindow;
    Output *output;
    Input *input;
    QMessageBox *messageBox;

public slots:
    ///
    /// \brief  Calls Output::TrySave and handles results
    ///
    void TrySave();

    ///
    /// \brief  Calls Output::TryLoad and handles results
    ///
    void TryLoad();

    ///
    /// \brief  Calls Output::ShowErrors
    ///
    void ShowErrors();

private slots:
    void output_results(Output::Result r);
    void input_results(Input::Result r);
};

#endif // APPLICATION_H
