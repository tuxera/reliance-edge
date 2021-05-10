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
#include <QToolTip>

#include "configwindow.h"
#include "warningbtn.h"
#include "ui_warningbtn.h"

// The icons used. These are set the first time the
// WarningBtn constructor is run
QIcon *WarningBtn::iconError = NULL;
QIcon *WarningBtn::iconWarn = NULL;

WarningBtn::WarningBtn(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WarningBtn)
{
    ui->setupUi(this);

    if(!iconError || !iconWarn)
    {
        iconError = new QIcon(":/icons/error.png");
        iconWarn = new QIcon(":/icons/warn.png");
    }

    // Forward the clicked() signal
    connect(ui->toolButton, SIGNAL(clicked()),
                     this, SIGNAL(clicked()));

    ui->toolButton->setVisible(false);
}

WarningBtn::~WarningBtn()
{
    delete ui;
}

void WarningBtn::SetWarn(QString msg)
{
    currMsg = msg;
    ui->toolButton->setIcon(*iconWarn);
    ui->toolButton->setVisible(true);
}

void WarningBtn::SetError(QString msg)
{
    currMsg = msg;
    ui->toolButton->setIcon(*iconError);
    ui->toolButton->setVisible(true);
}

void WarningBtn::Clear(void)
{
    currMsg = QString();
    ui->toolButton->setVisible(false);
}

void WarningBtn::Set(Validity v, QString msg)
{
    switch (v)
    {
        case Valid:
            Clear();
            break;

        case Warning:
            SetWarn(msg);
            break;

        case Invalid:
            SetError(msg);
            break;

        // Default: do nothing
    }
}

// Check for mouse-over events and show the current message
// in a tooltip when one arrises.
bool WarningBtn::event(QEvent *e)
{
    if(e->type() == QEvent::Enter)
    {
        if(!currMsg.isNull() && !currMsg.isEmpty())
        {
            // Initialize to absolute pos of this WarningBtn on the screen
            QPoint toolTipPoint = mapToGlobal(mapFromParent(pos()));

            // Put the tooltip a bit below the WarningBtn to avoid
            // mouse interferance.
            toolTipPoint.ry() += height() / 2;

            QToolTip::showText(toolTipPoint,
                               currMsg,
                               ui->toolButton);
        }
    }

    return QWidget::event(e);
}
