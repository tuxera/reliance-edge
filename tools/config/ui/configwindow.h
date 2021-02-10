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
#ifndef CONFIGWINDOW_H
#define CONFIGWINDOW_H

#include <QMainWindow>
#include <QList>

#include "settings/limitreporter.h"
#include "settings/dindirreporter.h"
#include "volumesettings.h"

namespace Ui {
class ConfigWindow;
}

class ConfigWindow : public QMainWindow
{
    Q_OBJECT
public:

    ///
    /// \brief  Represents the radio button options under Memory Management
    ///         Methods
    ///
    enum MemFnSet
    {
        UseCStd,
        UseReliance,
        Customize
    };

    ///
    /// \brief  Constructs a ConfigWindow, Also initializing allSettings and
    ///         volumeSettings
    ///
    explicit ConfigWindow(QWidget *parent = 0);

    ~ConfigWindow();

    ///
    /// \brief  Sets the current selection for memory management methods. Used
    ///         to select "Customize" after loading settings.
    ///
    /// \param mfs  The option to set
    ///
    void SetMemRbtnSelection(MemFnSet mfs);

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void updateLimits();

    Ui::ConfigWindow *ui;
    QList<WarningBtn *> wbtns;
    LimitReporter *limitReporter;
    DindirReporter *dindirReporter;
    int timerId;
signals:
    ///
    /// \brief  Invoked when <i>File -> Save</i> is selected
    ///
    void saveClicked();

    ///
    /// \brief  Invoked when <i>File -> Save As</i> is selected
    ///
    void saveAsClicked();

    ///
    /// \brief  Invoked when <i>File -> Load</i> is selected
    ///
    void loadClicked();

    ///
    /// \brief  Invoked when a warning button is clicked
    ///
    void warningBtnClicked();

private slots:
    void cbReadonly_toggled(bool selected);
    void cbAutomaticDiscards_toggled(bool selected);
    void rbtnUsePosix_toggled(bool selected);
    void cbPosixRename_toggled(bool selected);
    void cbPosixMkdir_toggled(bool selected);
    void cbPosixLink_toggled(bool selected);
    void cbPosixUnlink_toggled(bool selected);
    void cbPosixFtruncate_toggled(bool selected);
    void cbPosixFstrim_toggled(bool selected);
    void cbFseTruncate_toggled(bool selected);
    void cbInodeTimestamps_toggled(bool selected);
    void rbtnMemUseCStd_toggled(bool selected);
    void rbtnMemUseReliance_toggled(bool selected);
    void rbtnMemCustomize_toggled(bool selected);
    void cbTransactManual_toggled(bool selected);

    void actionAbout_clicked();
};

#endif // CONFIGWINDOW_H
