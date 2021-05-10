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
#include "filedialog.h"
#include <debug.h>

QString FileDialog::defaultDir = QDir::homePath();

FileDialog::FileDialog(QWidget *parentWindow, AcceptMode amode, FileMode fmode)
    : QFileDialog(parentWindow),
      acceptMode(amode)
{
    codefileNameFilters += "Config code file (redconf.c)";
    codefileNameFilters += "C code files (*.c)";
    codefileNameFilters += "All files(*.*)";

    headerNameFilters += "Config header file (redconf.h)";
    headerNameFilters += "C header files (*.h)";
    headerNameFilters += "All files (*.*)";

    setFileMode(fmode);
    setAcceptMode(amode);

    connect(this, SIGNAL(windowTitleChanged(QString)),
            this, SLOT(on_windowTitleChanged(QString)));
}

QString FileDialog::ShowGetHeader(const QString &defaultPath)
{
    if(acceptMode == AcceptSave)
    {
        setWindowTitle("Save Configuration Header As (1 of 2)");
    }
    else
    {
        Q_ASSERT(acceptMode == AcceptOpen);
        setWindowTitle("Open Configuration Header (1 of 2)");
    }

    setDefaultSuffix("h");
    setNameFilters(headerNameFilters);

    if(!defaultPath.isNull())
    {
        selectFile(defaultPath);
    }
    else
    {
        selectFile("redconf.h");
    }

    return showFileDialog();
}

QString FileDialog::ShowGetCodefile(const QString &defaultPath)
{
    if(acceptMode == AcceptSave)
    {
        setWindowTitle("Save Configuration Code File As (2 of 2)");
    }
    else
    {
        Q_ASSERT(acceptMode == AcceptOpen);
        setWindowTitle("Open Configuration Code File (2 of 2)");
    }

    setDefaultSuffix("c");
    setNameFilters(codefileNameFilters);

    if(!defaultPath.isNull())
    {
        selectFile(defaultPath);
    }
    else
    {
        selectFile("redconf.c");
    }

    return showFileDialog();
}

// Shows fileDialog and returns the path to the file chosen by the user.
// Returns QString::null if no file is selected.
QString FileDialog::showFileDialog()
{
    setDirectory(defaultDir);

    if(!exec())
    {
        return QString::null;
    }

    defaultDir = directory().absolutePath();

    QStringList strList = selectedFiles();

    Q_ASSERT(strList.count() == 1); //Should give us one file
    if(strList.count() == 0) //But just in case it doesn't
    {
        return QString::null;
    }

    return strList.at(0);
}

// This is implemented as a workaround on Ubuntu, where setDirectory is
// ineffective unless called while the window is open.
//
// TODO: it's possible to open up a dialog without the window title changing.
// Use a different event instead.
void FileDialog::on_windowTitleChanged(const QString &title)
{
    (void) title;

    setDirectory(defaultDir);
}
