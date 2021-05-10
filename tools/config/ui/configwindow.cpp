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
#include <QMessageBox>
#include <QDesktopWidget>

#include "control/output.h"
#include "version.h"
#include "allsettings.h"
#include "validators.h"
#include "configwindow.h"
#include "ui_configwindow.h"

ConfigWindow::ConfigWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ConfigWindow)
{
    ui->setupUi(this);

    // Instantiate allSettings

    // "General" tab
    allSettings.cbsReadonly = new CbSetting(macroNameReadonly, false, emptyBoolValidator, ui->cbReadonly);
    allSettings.cbsAutomaticDiscards = new CbSetting(macroNameAutomaticDiscards, false, validateAutomaticDiscards, ui->cbAutomaticDiscards, ui->wbtnAutomaticDiscards);
    allSettings.rbtnsUsePosix = new RbtnSetting(macroNameUsePosix, true, validateUsePosixApi, ui->rbtnUsePosix, ui->wbtnApiRbtns);
    allSettings.rbtnsUseFse = new RbtnSetting(macroNameUseFse, false, validateUseFseApi, ui->rbtnUseFse, ui->wbtnApiRbtns);
    allSettings.cbsPosixFormat = new CbSetting(macroNamePosixFormat, true, emptyBoolValidator, ui->cbPosixFormat);
    allSettings.cbsPosixLink = new CbSetting(macroNamePosixLink, true, emptyBoolValidator, ui->cbPosixLink);
    allSettings.cbsPosixUnlink = new CbSetting(macroNamePosixUnlink, true, emptyBoolValidator, ui->cbPosixUnlink);
    allSettings.cbsPosixMkdir = new CbSetting(macroNamePosixMkdir, true, emptyBoolValidator, ui->cbPosixMkdir);
    allSettings.cbsPosixRmdir = new CbSetting(macroNamePosixRmdir, true, emptyBoolValidator, ui->cbPosixRmDir);
    allSettings.cbsPosixRename = new CbSetting(macroNamePosixRename, false, emptyBoolValidator, ui->cbPosixRename);
    allSettings.cbsPosixAtomicRename = new CbSetting(macroNamePosixRenameAtomic, false, emptyBoolValidator, ui->cbPosixAtomicRename);
    allSettings.cbsPosixFtruncate = new CbSetting(macroNamePosixFtruncate, true, emptyBoolValidator, ui->cbPosixFtruncate);
    allSettings.cbsPosixDirOps = new CbSetting(macroNamePosixDirOps, true, emptyBoolValidator, ui->cbPosixDirOps);
    allSettings.cbsPosixCwd = new CbSetting(macroNamePosixCwd, false, emptyBoolValidator, ui->cbPosixCwd);
    allSettings.cbsPosixFstrim = new CbSetting(macroNamePosixFstrim, false, validatePosixFstrim, ui->cbPosixFstrim, ui->wbtnFstrim);
    allSettings.sbsMaxNameLen = new SbSetting(macroNameMaxNameLen, 12, validateMaxNameLen, ui->sbFileNameLen, ui->wbtnFileNameLen);
    allSettings.pssPathSepChar = new PathSepSetting(macroNamePathSepChar, "/", validatePathSepChar, ui->cmbPathChar, ui->lePathCharCustom, ui->wbtnPathChar);
    allSettings.cbsFseFormat = new CbSetting(macroNameFseFormat, false, emptyBoolValidator, ui->cbFseFormat);
    allSettings.cbsFseTruncate = new CbSetting(macroNameFseTruncate, true, emptyBoolValidator, ui->cbFseTruncate);
    allSettings.cbsFseGetMask = new CbSetting(macroNameFseGetMask, true, emptyBoolValidator, ui->cbFseGetMask);
    allSettings.cbsFseSetMask = new CbSetting(macroNameFseSetMask, true, emptyBoolValidator, ui->cbFseSetMask);
    allSettings.sbsTaskCount = new SbSetting(macroNameTaskCount, 10, validateTaskCount, ui->sbTaskCount, ui->wbtnTaskCount);
    allSettings.sbsHandleCount = new SbSetting(macroNameHandleCount, 10, validateHandleCount, ui->sbHandleCount, ui->wbtnHandleCount);
    allSettings.cbsDebugEnableOutput = new CbSetting(macroNameDebugEnableOutput, false, emptyBoolValidator, ui->cbEnableOutput);
    allSettings.cbsDebugProcesAsserts = new CbSetting(macroNameDebugProcesAsserts, false, emptyBoolValidator, ui->cbProcessAsserts);

    // "Volumes" tab (Note: most settings handled
    // by VolumeSettings
    allSettings.cmisBlockSize = new CmbIntSetting(macroNameBlockSize, 512, validateBlockSize, ui->cmbBlockSize, ui->wbtnBlockSize);

    // "Data Storage" tab
    allSettings.cmssByteOrder = new CmbStrSetting(macroNameByteOrder, "Little endian", validateByteOrder, ui->cmbByteOrder, ui->wbtnByteOrder);
    allSettings.cmisNativeAlignment = new CmbIntSetting(macroNameNativeAlignment, 4, validateAlignmentSize, ui->cmbAlignmentSize, ui->wbtnAlignmentSize);
    allSettings.cmssCrc = new CmbStrSetting(macroNameCrc, "Slice by 8 - largest, fastest", validateCrc, ui->cmbCrc, ui->wbtnCrc);
    allSettings.cbsInodeBlockCount = new CbSetting(macroNameInodeCount, true, validateInodeBlockCount, ui->cbInodeBlockCount, ui->wbtnInodeBlockCount);
    allSettings.cbsInodeTimestamps = new CbSetting(macroNameInodeTimestamps, true, validateInodeTimestamps, ui->cbInodeTimestamps, ui->wbtnInodeTimestamps);
    allSettings.cbsUpdateAtime = new CbSetting(macroNameUpdateAtime, false, emptyBoolValidator, ui->cbUpdateAtime);
    allSettings.sbsDirectPtrs = new SbSetting(macroNameDirectPtrs, 4, validateDirectPointers, ui->sbDirectPointers, ui->wbtnDirectPointers);
    allSettings.sbsIndirectPtrs = new SbSetting(macroNameIndirectPtrs, 32, validateIndirectPointers, ui->sbIndirectPointers, ui->wbtnIndirectPointers);

    // "Memory" tab
    allSettings.sbsAllocatedBuffers = new SbSetting(macroNameAllocatedBuffers, 8, validateAllocatedBuffers, ui->sbAllocatedBuffers, ui->wbtnAllocatedBuffers);
    allSettings.lesMemcpy = new LeSetting(macroNameMemcpy, cstdMemcpy, emptyStringValidator, ui->leMemcpy);
    allSettings.lesMemmov = new LeSetting(macroNameMemmov, cstdMemmov, emptyStringValidator, ui->leMemmov);
    allSettings.lesMemset = new LeSetting(macroNameMemset, cstdMemset, emptyStringValidator, ui->leMemset);
    allSettings.lesMemcmp = new LeSetting(macroNameMemcmp, cstdMemcmp, emptyStringValidator, ui->leMemcmp);
    allSettings.lesStrlen = new LeSetting(macroNameStrlen, cstdStrlen, emptyStringValidator, ui->leStrlen);
    allSettings.lesStrcmp = new LeSetting(macroNameStrcmp, cstdStrcmp, emptyStringValidator, ui->leStrcmp);
    allSettings.lesStrncmp = new LeSetting(macroNameStrncmp, cstdStrncmp, emptyStringValidator, ui->leStrncmp);
    allSettings.lesStrncpy = new LeSetting(macroNameStrncpy, cstdStrncpy, emptyStringValidator, ui->leStrncpy);
    allSettings.lesInclude = new LeSetting("", cstdStringH, validateMemInclude, ui->leIncludeFile, ui->wbtnIncludeFile);

    // "Transaction Points" tab
    allSettings.cbsTrManual = new CbSetting(macroNameTrManual, false, validateTransactManual,ui->cbTransactManual, ui->wbtnTransactManual);
    allSettings.cbsTrFileCreat = new CbSetting(macroNameTrFileCreat, true, emptyBoolValidator, ui->cbTransactFileCreate);
    allSettings.cbsTrDirCreat = new CbSetting(macroNameTrDirCreat, true, emptyBoolValidator, ui->cbTransactDirCreate);
    allSettings.cbsTrRename = new CbSetting(macroNameTrRename, true, emptyBoolValidator, ui->cbTransactRename);
    allSettings.cbsTrLink = new CbSetting(macroNameTrLink, true, emptyBoolValidator, ui->cbTransactLink);
    allSettings.cbsTrUnlink = new CbSetting(macroNameTrUnlink, true, emptyBoolValidator, ui->cbTransactUnlink);
    allSettings.cbsTrWrite = new CbSetting(macroNameTrWrite, false, emptyBoolValidator, ui->cbTransactWrite);
    allSettings.cbsTrTruncate = new CbSetting(macroNameTrTruncate, false, emptyBoolValidator, ui->cbTransactTruncate);
    allSettings.cbsTrFSync = new CbSetting(macroNameTrFSync, true, emptyBoolValidator, ui->cbTransactFSync);
    allSettings.cbsTrClose = new CbSetting(macroNameTrClose, true, emptyBoolValidator, ui->cbTransactClose);
    allSettings.cbsTrVolFull = new CbSetting(macroNameTrVolFull, true, validateTransactVolFull, ui->cbTransactVolFull, ui->wbtnTransactVolFull);
    allSettings.cbsTrUmount = new CbSetting(macroNameTrUmount, true, emptyBoolValidator, ui->cbTransactVolUnmount);
    allSettings.cbsTrSync = new CbSetting(macroNameTrSync, true, emptyBoolValidator, ui->cbTransactSync);

    allSettings.cbsInodeBlockCount->notifyList.append(allSettings.sbsDirectPtrs);
    allSettings.cbsInodeBlockCount->notifyList.append(allSettings.sbsIndirectPtrs);
    allSettings.cbsInodeTimestamps->notifyList.append(allSettings.sbsDirectPtrs);
    allSettings.cbsInodeTimestamps->notifyList.append(allSettings.sbsIndirectPtrs);
    allSettings.rbtnsUsePosix->notifyList.append(allSettings.sbsDirectPtrs);
    allSettings.rbtnsUsePosix->notifyList.append(allSettings.sbsIndirectPtrs);
    allSettings.rbtnsUsePosix->notifyList.append(allSettings.cbsInodeBlockCount);
    allSettings.rbtnsUsePosix->notifyList.append(allSettings.cbsInodeTimestamps);
    allSettings.cmisBlockSize->notifyList.append(allSettings.sbsDirectPtrs);
    allSettings.cmisBlockSize->notifyList.append(allSettings.sbsIndirectPtrs);
    allSettings.sbsIndirectPtrs->notifyList.append(allSettings.sbsDirectPtrs);
    allSettings.sbsDirectPtrs->notifyList.append(allSettings.sbsIndirectPtrs);

    allSettings.cbsInodeBlockCount->notifyList.append(allSettings.sbsAllocatedBuffers);
    allSettings.cbsInodeTimestamps->notifyList.append(allSettings.sbsAllocatedBuffers);
    allSettings.rbtnsUsePosix->notifyList.append(allSettings.sbsAllocatedBuffers);
    allSettings.cmisBlockSize->notifyList.append(allSettings.sbsAllocatedBuffers);
    allSettings.sbsIndirectPtrs->notifyList.append(allSettings.sbsAllocatedBuffers);
    allSettings.sbsDirectPtrs->notifyList.append(allSettings.sbsAllocatedBuffers);
    allSettings.cbsPosixRename->notifyList.append(allSettings.sbsAllocatedBuffers);
    allSettings.cbsPosixAtomicRename->notifyList.append(allSettings.sbsAllocatedBuffers);

    allSettings.cbsAutomaticDiscards->notifyList.append(allSettings.cbsPosixFstrim);
    allSettings.cbsPosixFstrim->notifyList.append(allSettings.cbsPosixFstrim);

    // Simulate toggling to init which transaction flags
    // are available
    rbtnUsePosix_toggled(allSettings.rbtnsUsePosix->GetValue());
    ui->cbPosixAtomicRename->setEnabled(allSettings.cbsPosixRename->GetValue());

    // Must be initialized after allSettings.cmisBlockSize
    volumeSettings = new VolumeSettings(ui->lePathPrefix,
                                        ui->cmbSectorSize,
                                        ui->cbSectorSizeAuto,
                                        ui->sbVolSize,
                                        ui->cbVolSizeAuto,
                                        ui->labelVolSizeBytes,
                                        ui->sbVolOff,
                                        ui->labelVolOffBytes,
                                        ui->sbInodeCount,
                                        ui->cmbAtomicWrite,
                                        ui->cmbDiscardsSupported,
                                        ui->cbEnableRetries,
                                        ui->sbBlockIoRetries,
                                        ui->widgetBlockIoRetries,
                                        ui->btnAddVol,
                                        ui->btnRemoveCurrVol,
                                        ui->listVolumes,
                                        ui->wbtnVolumeCtrls,
                                        ui->wbtnPathPrefix,
                                        ui->wbtnSectorSize,
                                        ui->wbtnVolSize,
                                        ui->wbtnVolOff,
                                        ui->wbtnInodeCount,
                                        ui->wbtnAtomicWrite,
                                        ui->wbtnDiscardsSupported,
                                        ui->wbtnIoRetries);

    wbtns.append(ui->wbtnAutomaticDiscards);
    wbtns.append(ui->wbtnFstrim);
    wbtns.append(ui->wbtnTransactVolFull);
    wbtns.append(ui->wbtnTransactManual);
    wbtns.append(ui->wbtnAllocatedBuffers);
    wbtns.append(ui->wbtnInodeTimestamps);
    wbtns.append(ui->wbtnIndirectPointers);
    wbtns.append(ui->wbtnDirectPointers);
    wbtns.append(ui->wbtnCrc);
    wbtns.append(ui->wbtnAlignmentSize);
    wbtns.append(ui->wbtnByteOrder);
    wbtns.append(ui->wbtnVolumeCtrls);
    wbtns.append(ui->wbtnSectorSize);
    wbtns.append(ui->wbtnVolSize);
    wbtns.append(ui->wbtnVolOff);
    wbtns.append(ui->wbtnAtomicWrite);
    wbtns.append(ui->wbtnDiscardsSupported);
    wbtns.append(ui->wbtnInodeCount);
    wbtns.append(ui->wbtnPathPrefix);
    wbtns.append(ui->wbtnBlockSize);
    wbtns.append(ui->wbtnHandleCount);
    wbtns.append(ui->wbtnTaskCount);
    wbtns.append(ui->wbtnPathChar);
    wbtns.append(ui->wbtnFileNameLen);
    wbtns.append(ui->wbtnApiRbtns);
    wbtns.append(ui->wbtnIncludeFile);

    for(int i = 0; i < wbtns.count(); i++)
    {
        connect(wbtns[i], SIGNAL(clicked()),
                this, SIGNAL(warningBtnClicked()));
    }
    connect(ui->cbReadonly, SIGNAL(toggled(bool)),
            this, SLOT(cbReadonly_toggled(bool)));
    connect(ui->cbAutomaticDiscards, SIGNAL(toggled(bool)),
            this, SLOT(cbAutomaticDiscards_toggled(bool)));
    connect(ui->rbtnUsePosix, SIGNAL(toggled(bool)),
            this, SLOT(rbtnUsePosix_toggled(bool)));
    connect(ui->cbPosixRename, SIGNAL(toggled(bool)),
            this, SLOT(cbPosixRename_toggled(bool)));
    connect(ui->cbPosixMkdir, SIGNAL(toggled(bool)),
            this, SLOT(cbPosixMkdir_toggled(bool)));
    connect(ui->cbPosixLink, SIGNAL(toggled(bool)),
            this, SLOT(cbPosixLink_toggled(bool)));
    connect(ui->cbPosixUnlink, SIGNAL(toggled(bool)),
            this, SLOT(cbPosixUnlink_toggled(bool)));
    connect(ui->cbPosixFtruncate, SIGNAL(toggled(bool)),
            this, SLOT(cbPosixFtruncate_toggled(bool)));
    connect(ui->cbPosixFstrim, SIGNAL(toggled(bool)),
            this, SLOT(cbPosixFstrim_toggled(bool)));
    connect(ui->cbFseTruncate, SIGNAL(toggled(bool)),
            this, SLOT(cbFseTruncate_toggled(bool)));
    connect(ui->cbInodeTimestamps, SIGNAL(toggled(bool)),
            this, SLOT(cbInodeTimestamps_toggled(bool)));
    connect(ui->rbtnMemUseCStd, SIGNAL(toggled(bool)),
            this, SLOT(rbtnMemUseCStd_toggled(bool)));
    connect(ui->rbtnMemUseReliance, SIGNAL(toggled(bool)),
            this, SLOT(rbtnMemUseReliance_toggled(bool)));
    connect(ui->rbtnMemCustomize, SIGNAL(toggled(bool)),
            this, SLOT(rbtnMemCustomize_toggled(bool)));
    connect(ui->cbTransactManual, SIGNAL(toggled(bool)),
            this, SLOT(cbTransactManual_toggled(bool)));
    connect(ui->actionAbout, SIGNAL(triggered()),
            this, SLOT(actionAbout_clicked()));

    // Forwarded signals
    connect(ui->actionSave, SIGNAL(triggered()),
            this, SIGNAL(saveClicked()));
    connect(ui->actionSave_As, SIGNAL(triggered()),
            this, SIGNAL(saveAsClicked()));
    connect(ui->actionLoad, SIGNAL(triggered()),
            this, SIGNAL(loadClicked()));

    ui->actionSave->setShortcut(Qt::CTRL | Qt::Key_S);
    ui->actionLoad->setShortcut(Qt::CTRL | Qt::Key_O);
    ui->actionAbout->setShortcut(Qt::Key_F1);

    // Hide settings for unused API
    if(allSettings.rbtnsUsePosix->GetValue())
    {
        ui->frameFseOps->setVisible(false);
    }
    else
    {
        ui->framePosixOps->setVisible(false);
    }

    // Not controlled by a Setting object, so set it here.
    ui->rbtnMemUseCStd->setChecked(true);

    limitReporter = new LimitReporter(ui->lFsizeBytes, ui->lVsizeBytes);
    dindirReporter = new DindirReporter(ui->labelDindirPointers);

    // Start a timer to go off as soon as initialization is complete.
    timerId = startTimer(0);
}

ConfigWindow::~ConfigWindow()
{
    delete ui;
    delete volumeSettings;
    volumeSettings = NULL;

    AllSettings::DeleteAll();

    delete limitReporter;
    delete dindirReporter;
}

void ConfigWindow::SetMemRbtnSelection(MemFnSet mfs)
{
    switch(mfs)
    {
        case UseCStd:
            ui->rbtnMemUseCStd->setChecked(true);
            break;

        case UseReliance:
            ui->rbtnMemUseReliance->setChecked(true);
            break;

        case Customize:
            ui->rbtnMemCustomize->setChecked(true);
            break;
    }
}

// Resize the ConfigWindow to be large enough to show all content, but not
// larger than the screen size. This should be called once after initialization.
void ConfigWindow::timerEvent(QTimerEvent *event)
{
    QWidget::timerEvent(event);

    if(event->timerId() != timerId)
    {
        return;
    }

    // The code below assumes the General tab is selected, which should be true
    // since this function is invoked only on startup. However, a common error
    // is to use the WYSIWYG editor and save the .ui file while a different tab
    // is selected, which will break this code. Assert to verify this isn't the
    // case.
    Q_ASSERT(ui->sawcGeneral->isVisible() == true);

    // Set to content width plus about enough for a scrollbar and frames
    int width = ui->sawcGeneral->width() + 25;

    // Set to content height plus height of the rest of the window plus
    // about enough for a scrollbar
    int height = ui->sawcGeneral->height()
            + this->height() - ui->scrollAreaGeneral->height()
            + 20;

    QDesktopWidget dw;
    QRect mainScrSize = dw.availableGeometry(dw.primaryScreen());

    if(width > mainScrSize.width())
    {
        width = mainScrSize.width();
    }

    if(height > mainScrSize.height())
    {
        height = mainScrSize.height();
    }

    resize(width, height);

#ifdef Q_OS_LINUX
    // On Windows, placing the resize within a timerEvent callback allows the
    // config window to be placed by the OS. On Ubuntu, it still always opens
    // in the corner. Move it to the middle instead.
    if(mainScrSize.width() != 0 || mainScrSize.height() != 0)
    {
        int x = mainScrSize.width()/2 - width/2;
        int y = mainScrSize.height()/2 - height/2;

        move(x, y);
    }
#endif

    killTimer(timerId); // Don't call this function again.
}

void ConfigWindow::cbReadonly_toggled(bool selected)
{
    ui->cbAutomaticDiscards->setEnabled(!selected);
    ui->cbPosixFormat->setEnabled(!selected);
    ui->cbPosixLink->setEnabled(!selected);
    ui->cbPosixUnlink->setEnabled(!selected);
    ui->cbPosixMkdir->setEnabled(!selected);
    ui->cbPosixRmDir->setEnabled(!selected);
    ui->framePosixRenames->setEnabled(!selected);
    ui->cbPosixFtruncate->setEnabled(!selected);
    ui->cbPosixFstrim->setEnabled(!selected);

    ui->cbFseFormat->setEnabled(!selected);
    ui->cbFseSetMask->setEnabled(!selected);
    ui->cbFseTruncate->setEnabled(!selected);

    ui->cbUpdateAtime->setEnabled(!selected && ui->cbInodeTimestamps->isChecked());

    ui->tabTransactionPts->setEnabled(!selected);

    // Disable tr settings tab
    ui->tabWidget->setTabEnabled(4, !selected);
}

void ConfigWindow::cbAutomaticDiscards_toggled(bool selected)
{
    ui->cmbDiscardsSupported->setEnabled(ui->cbPosixFstrim->isChecked() || selected);
}

void ConfigWindow::rbtnUsePosix_toggled(bool selected)
{
    ui->framePosixOps->setVisible(selected);
    ui->frameFseOps->setVisible(!selected);

    ui->cbTransactFileCreate->setEnabled(selected);
    ui->cbTransactDirCreate->setEnabled(selected && ui->cbPosixMkdir->isChecked());
    ui->cbTransactRename->setEnabled(selected && ui->cbPosixRename->isChecked());
    ui->cbTransactLink->setEnabled(selected && ui->cbPosixLink->isChecked());
    ui->cbTransactUnlink->setEnabled(selected && ui->cbPosixUnlink->isChecked());
    ui->cbTransactFSync->setEnabled(selected);
    ui->cbTransactClose->setEnabled(selected);

    ui->cbTransactTruncate->setEnabled(
                (selected && ui->cbPosixFtruncate->isChecked())
                || (!selected && ui->cbFseTruncate->isChecked()));

    ui->cbTransactSync->setEnabled(selected);
}

void ConfigWindow::cbPosixRename_toggled(bool selected)
{
    ui->cbPosixAtomicRename->setEnabled(selected);
    ui->cbTransactRename->setEnabled(selected);
}

void ConfigWindow::cbPosixMkdir_toggled(bool selected)
{
    ui->cbTransactDirCreate->setEnabled(selected);
}

void ConfigWindow::cbPosixLink_toggled(bool selected)
{
    ui->cbTransactLink->setEnabled(selected);
}

void ConfigWindow::cbPosixUnlink_toggled(bool selected)
{
    ui->cbTransactUnlink->setEnabled(selected);
}

void ConfigWindow::cbPosixFtruncate_toggled(bool selected)
{
    // Although this box cannot be toggled by the user while POSIX is disabled,
    // it may be toggled internally when loading configuration values.
    if(ui->rbtnUsePosix->isChecked())
    {
        ui->cbTransactTruncate->setEnabled(selected);
    }
}

void ConfigWindow::cbPosixFstrim_toggled(bool selected)
{
    ui->cmbDiscardsSupported->setEnabled(ui->cbAutomaticDiscards->isChecked() || selected);
}

void ConfigWindow::cbFseTruncate_toggled(bool selected)
{
    // See comment in ::cbPosixFtruncateToggled.
    if(ui->rbtnUseFse->isChecked())
    {
        ui->cbTransactTruncate->setEnabled(selected);
    }
}

void ConfigWindow::cbInodeTimestamps_toggled(bool selected)
{
    ui->cbUpdateAtime->setEnabled(selected && !ui->cbReadonly->isChecked());
}

void ConfigWindow::rbtnMemUseCStd_toggled(bool selected)
{
    if(selected)
    {
        ui->frameMemFnsCust->setEnabled(false);
        ui->leMemcpy->setText(cstdMemcpy);
        ui->leMemmov->setText(cstdMemmov);
        ui->leMemset->setText(cstdMemset);
        ui->leMemcmp->setText(cstdMemcmp);
        ui->leStrlen->setText(cstdStrlen);
        ui->leStrcmp->setText(cstdStrcmp);
        ui->leStrncmp->setText(cstdStrncmp);
        ui->leStrncpy->setText(cstdStrncpy);
        ui->leIncludeFile->setText(cstdStringH);
    }
}

void ConfigWindow::rbtnMemUseReliance_toggled(bool selected)
{
    if(selected)
    {
        ui->frameMemFnsCust->setEnabled(false);
        ui->leMemcpy->clear();
        ui->leMemmov->clear();
        ui->leMemset->clear();
        ui->leMemcmp->clear();
        ui->leStrlen->clear();
        ui->leStrcmp->clear();
        ui->leStrncmp->clear();
        ui->leStrncpy->clear();
        ui->leIncludeFile->clear();
    }
}

void ConfigWindow::rbtnMemCustomize_toggled(bool selected)
{
    if(selected)
    {
        ui->frameMemFnsCust->setEnabled(true);
    }
}

void ConfigWindow::cbTransactManual_toggled(bool selected)
{
    ui->frameAutomaticTransactions->setEnabled(!selected);
}

void ConfigWindow::actionAbout_clicked()
{
    QMessageBox aboutBox(this);
    aboutBox.setModal(true);
    aboutBox.setTextFormat(Qt::RichText);
    aboutBox.setWindowTitle("About");
    aboutBox.setText(
                "Reliance Edge Configuration Utility"
                "<br/><br/>"
                "Version " CONFIG_VERSION
                "<br/><br/>"
                "This utility is designed to be used to configure the Reliance "
                "Edge file system. Documenation may be downloaded from "
                "<a href='http://www.datalight.com/reliance-edge'>"
                "datalight.com/reliance-edge</a>. For email support, contact "
                "<a href='mailto:support@tuxera.com'>"
                "support@tuxera.com</a>."
                );
    aboutBox.exec();
}

