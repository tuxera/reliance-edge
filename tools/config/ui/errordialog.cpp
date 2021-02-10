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
#include <QListWidget>
#include <QIcon>

#include "errordialog.h"
#include "ui_errordialog.h"

ErrorDialog::ErrorDialog(QWidget *parent) :
    QDialog(parent, Qt::WindowCloseButtonHint),
    ui(new Ui::ErrorDialog)
{
    ui->setupUi(this);

    connect(ui->btnOk, SIGNAL(clicked()),
                     this, SLOT(btnOk_clicked()));
    connect(ui->btnContinue, SIGNAL(clicked()),
                     this, SLOT(btnContinue_clicked()));
    connect(ui->btnCancel, SIGNAL(clicked()),
                     this, SLOT(btnCancel_clicked()));
}

ErrorDialog::~ErrorDialog()
{
    delete ui;

}

void ErrorDialog::SetErrorText(const QString &text)
{
    ui->label->setText(text);
}

void ErrorDialog::ShowErrorsInfo(QStringList errors, QStringList warnings)
{
    setModal(false); // Non-blocking UI

    ui->btnOk->setVisible(true);
    ui->btnCancel->setVisible(false);
    ui->btnContinue->setVisible(false);

    ui->btnOk->setDefault(true);

    showErrors(errors, warnings);
}

void ErrorDialog::ShowErrorsAction(QStringList errors, QStringList warnings)
{
    setModal(true); // UI blocks parent windows

    ui->btnContinue->setVisible(true);
    ui->btnCancel->setVisible(true);
    ui->btnOk->setVisible(false);

    ui->btnContinue->setDefault(true);

    showErrors(errors, warnings);
}

void ErrorDialog::showErrors(QStringList errors, QStringList warnings)
{
    QIcon iconError(":/icons/error.png");
    QIcon iconWarn(":/icons/warn.png");
    QListWidgetItem *lwi;

    // Clear and repopulate the list of error messages
    ui->listErrors->clear();
    for(int i = 0; i < errors.length(); i++)
    {
        lwi = new QListWidgetItem(iconError, errors[i]);
        ui->listErrors->addItem(lwi);
    }
    for(int i = 0; i < warnings.length(); i++)
    {
        lwi = new QListWidgetItem(iconWarn, warnings[i]);
        ui->listErrors->addItem(lwi);
    }

    show();
    activateWindow();
}

void ErrorDialog::btnOk_clicked()
{
    close();
    emit results(EdResultOk);
}

void ErrorDialog::btnContinue_clicked()
{
    close();
    emit results(EdResultContinue);
}

void ErrorDialog::btnCancel_clicked()
{
    close();
    emit results(EdResultCancel);
}
