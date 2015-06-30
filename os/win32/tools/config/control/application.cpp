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
#include <QErrorMessage>
#include <QRegularExpressionMatch>

#include "application.h"

Application::Application(int &argc, char *argv[])
    : QApplication(argc, argv),
      configWindow(NULL),
      output(NULL),
      input(NULL),
      messageBox(NULL)
{
}

Application::~Application()
{
    if(configWindow != NULL)
    {
        delete configWindow;
    }
    if(output != NULL)
    {
        delete output;
    }
    if(input != NULL)
    {
        delete input;
    }
    // MessageBox deleted by configWindow
}

int Application::Run()
{
    if(configWindow != NULL)
    {
        Q_ASSERT(false);
        return 1; //Error: already running
    }

    configWindow = new ConfigWindow();
    output = new Output(configWindow);
    input = new Input(configWindow);
    messageBox = new QMessageBox(configWindow);

    messageBox->setText("Error");
    messageBox->setIcon(QMessageBox::Critical);
    messageBox->setStandardButtons(QMessageBox::Ok);

    connect(configWindow, SIGNAL(saveClicked()),
                     this, SLOT(TrySave()));
    connect(output, SIGNAL(results(Output::Result)),
                     this, SLOT(output_results(Output::Result)));
    connect(configWindow, SIGNAL(loadClicked()),
                     this, SLOT(TryLoad()));
    connect(input, SIGNAL(results(Input::Result)),
                     this, SLOT(input_results(Input::Result)));
    connect(configWindow, SIGNAL(warningBtnClicked()),
                     this, SLOT(ShowErrors()));

    // Pass control to configWindow.
    configWindow->show();
    return exec();
}

void Application::TrySave()
{
    output->TrySave();
    // Result handled by output_results
}

void Application::TryLoad()
{
    input->TryLoad();
    // Result handled by input_results
}

void Application::ShowErrors()
{
    // Show error dialog, even if there are no errors.
    output->ShowErrors(true);
}

void Application::output_results(Output::Result r)
{
    if(r == Output::OutResultFileError)
    {
        messageBox->setInformativeText("Error saving configuration files. Try saving to a different directory.");
        messageBox->exec();
    }
    configWindow->activateWindow();

}

void Application::input_results(Input::Result r)
{
    if(r == Input::InResultFileError)
    {
        messageBox->setInformativeText("Error loading selected configuration files.");
        messageBox->exec();
    }
    else if (r == Input::InResultErrorHugeFile)
    {
        messageBox->setInformativeText("Unreasonably large file. Please select valid configuration files.");
        messageBox->exec();
    }
    else if (r == Input::InResultSuccess)
    {
        configWindow->SetMemRbtnSelection(ConfigWindow::Customize);
        output->ShowErrors(false);
    }
    configWindow->activateWindow();
}
