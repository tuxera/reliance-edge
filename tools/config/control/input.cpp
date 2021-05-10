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
#include <QTextStream>

#include "input.h"
#include "allsettings.h"

Input::Input(QWidget *parentWin)
    : fileDialog(NULL),
      parentWindow(parentWin),
      messageBox(new QMessageBox(parentWin))
{
}

void Input::TryLoad()
{
    QString headerText, codefileText;

    if(fileDialog == NULL)
    {
        fileDialog = new FileDialog(parentWindow,
                                    QFileDialog::AcceptOpen,
                                    QFileDialog::ExistingFile);
    }

    QString headerPath = fileDialog->ShowGetHeader(QString::null);
    if(headerPath.isNull() || headerPath.isEmpty())
    {
        emit results(InResultUserCancelled, QString::null, QString::null);
        return;
    }

    QString codefilePath = fileDialog->ShowGetCodefile(QString::null);
    if(codefilePath.isNull() || codefilePath.isEmpty())
    {
        emit results(InResultUserCancelled, QString::null, QString::null);
        return;
    }

    if(!getFile(headerPath, headerText))
    {
        return; //results() already emitted by getFile
    }
    if(!getFile(codefilePath, codefileText))
    {
        return;
    }

    QStringList notFound;
    QStringList notParsed;
    AllSettings::ParseHeaderToSettings(headerText, notFound, notParsed);
    AllSettings::ParseCodefileToSettings(codefileText, notFound, notParsed);

    if(notFound.count() > 0 || notParsed.count() > 0)
    {
        QString report;

        if(notFound.count() > 0)
        {
            report += "The following settings were not found in the selected configuration files: \n\n";
            for(int i = 0; i < notFound.count(); i++)
            {
                report += QString(" - ") + notFound[i] + QString("\n");
            }
            if(notParsed.count() > 0)
            {
                report += "\n";
            }
        }
        if(notParsed.count() > 0)
        {
            report += "The following settings were located in the selected configuration files but could not be parsed: \n\n";
            for(int i = 0; i < notParsed.count(); i++)
            {
                report += QString(" - ") + notParsed[i] + QString("\n");
            }
        }

        messageBox->setText("Some settings could not be loaded.");
        messageBox->setInformativeText("Press \"Show Details\" to view which values were not loaded properly.");
        messageBox->setDetailedText(report);
        messageBox->setIcon(QMessageBox::Warning);
        messageBox->setStandardButtons(QMessageBox::Ok);

        messageBox->exec();
    }

    emit results(InResultSuccess, headerPath, codefilePath);
}

// Helper method for TryLoad: takes a path to a text file
// filePath and fills fileContents with its text. If an
// error is encountered, emits results() with the appropriate
// Result and returns false
bool Input::getFile(const QString &filePath, QString &fileContent)
{
    bool success = true;
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        success = false;
    }
    else
    {
        if(file.size() > 1024 * 1024)
        {
            // Don't bother hanging the computer trying to read
            // this file. It's way too big.
            emit results(InResultErrorHugeFile, QString::null, QString::null);
            return false;
        }

        QTextStream stmIn(&file);
        stmIn.setCodec("UTF-8");
        fileContent = stmIn.readAll();

        if(stmIn.status() != QTextStream::Ok
                || fileContent.isNull()
                || fileContent.isEmpty())
        {
            success = false;
        }
    }

    if(!success)
    {
        emit results(InResultFileError, QString::null, QString::null);
        return false;
    }
    else return true;
}
