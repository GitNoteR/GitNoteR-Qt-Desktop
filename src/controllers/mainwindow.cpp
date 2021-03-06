﻿#include "ui_mainwindow.h"

#include "mainwindow.h"
#include "notelistsortpopupmenu.h"
#include "notelistcellwidget.h"
#include "messagedialog.h"
#include "tools.h"
#include "globals.h"
#include "groupmodel.h"
#include "version.h"
#include "importnotedialog.h"
#include "screenshot.h"

#define __OPEN_PURCHASE_PANEL_TIME__ 300
#define __HOLD_PRESSED_TIME__ 1000

MainWindow::MainWindow(MenuBar *menubar, QWidget *parent) :
        QMainWindow(parent),
        mMenuBar(menubar),
        ui(new Ui::MainWindow),
        mAutoSyncRepoTimer(new SingleTimeout(Gitnoter::SingleTimeNone, this)),
        mAutoLockTimer(new SingleTimeout(Gitnoter::SingleTimeNone, this)),
        mAboutDialog(new AboutDialog(this)),
        mSettingDialog(new SettingDialog(this)),
        mEnterLicenseDialog(new EnterLicenseDialog(this)),
        mGitManager(new GitManager()),
        mLockDialog(new LockDialog(this)),
        mSearchSingleTimeout(new SingleTimeout(Gitnoter::ResetTimeout, this)),
        mOpenPurchasePanelTimestamp((int) QDateTime::currentSecsSinceEpoch()),
        mSyncRepoThread(new QThread(this)),
        mGAnalytics(new GAnalytics(__GOOGLE_ANALYTICS_TRACKING_ID__))
{
    ui->setupUi(this);

    initTempDir();
    init();
    setupUi();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUi()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    ui->widget_listBar->setStyleSheet("#widget_listBar{border: none;border-right: 1px solid rgb(191, 191, 191);}");
    ui->widget_sidebar->setStyleSheet("#widget_sidebar{border: none;border-right: 1px solid rgb(191, 191, 191);}");
#endif

    mMenuBar->addActionToWindowMenu(this);
    mMenuBar->setMainWindow(this);
    mEnterLicenseDialog->init();
    setMenuBar(mMenuBar->menuBar());
    ui->groupTreeWidget->expandAll();
    ui->groupTreeWidget->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->noteListWidget->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->lineEdit_noteSearch->addAction(QIcon(":/images/icon-search.png"), QLineEdit::LeadingPosition);
    ui->markdownEditorWidget->setParent(ui->stackWidget_editor);

    NoteListSortPopupMenu *noteListSortPopupMenu = new NoteListSortPopupMenu(ui->pushButton_sort, this);
    ui->pushButton_sort->setMenu(noteListSortPopupMenu);
    connect(noteListSortPopupMenu, SIGNAL(actionTriggered()), ui->noteListWidget, SLOT(onActionTriggered()));

    ui->splitter->setSizes(gConfigModel->getMainWindowSplitterSizes());
    ui->stackWidget_editor->setCurrentIndex((int) gConfigModel->openMainWindowNoteUuid().isEmpty());
    
    if (!gConfigModel->getMainWindowRect().topLeft().isNull()) {
        setGeometry(gConfigModel->getMainWindowRect());
    }

    if (gConfigModel->getMainWindowFullScreen()) {
        showFullScreen();
    }

    syncRepo();

    updateAutoLockTimer();
    updateAutoSyncRepoTimer();

    showSidebar(gConfigModel->getSidebarWidget());
    showListBar(gConfigModel->getListBarWidget());

    connect(ui->noteListWidget, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this, SLOT(newWindow(QListWidgetItem *)));

    connect(mMenuBar->getActionNewNote(), SIGNAL(triggered()), this, SLOT(appendNote()));
    connect(mMenuBar->getActionNewCategories(), SIGNAL(triggered()), this, SLOT(appendGroup()));
    connect(mMenuBar->getActionNewTags(), SIGNAL(triggered()), this, SLOT(appendGroup()));
    connect(mMenuBar->getActionReloadNotes(), SIGNAL(triggered()), this, SLOT(reload()));
    connect(mMenuBar->getActionPreferences(), SIGNAL(triggered()), this, SLOT(openSettingWidget()));
    connect(mMenuBar->getActionDeleteNote(), SIGNAL(triggered()), this, SLOT(trashNote()));
    connect(mMenuBar->getActionGroupSearch(), SIGNAL(triggered()), this, SLOT(setSearchFocus()));
    connect(mMenuBar->getActionSidebar(), SIGNAL(triggered(bool)), this, SLOT(showSidebar(bool)));
    connect(mMenuBar->getActionListbar(), SIGNAL(triggered(bool)), this, SLOT(showListBar(bool)));
    connect(mMenuBar->getActionSynchNote(), SIGNAL(triggered()), this, SLOT(syncRepo()));
    connect(mMenuBar->getActionImportEvernote(), SIGNAL(triggered()), this, SLOT(importEvernote()));

    connect(mMenuBar->getActionEnterFullScreen(), SIGNAL(triggered()), this, SLOT(enterFullScreen()));
    connect(mMenuBar->getActionFullScreenEditMode(), SIGNAL(triggered()), this, SLOT(fullScreenEditMode()));

    connect(mMenuBar->getActionMixWindow(), SIGNAL(triggered()), this, SLOT(showMinimized()));
    connect(mMenuBar->getActionResizeWindow(), SIGNAL(triggered()), this, SLOT(showMaximized()));

    connect(mMenuBar->getActionNewWindow(), SIGNAL(triggered()), this, SLOT(newWindow()));
    connect(mMenuBar->getActionShowLastWindow(), SIGNAL(triggered()), this, SLOT(showLastWindow()));
    connect(mMenuBar->getActionShowNextWindow(), SIGNAL(triggered()), this, SLOT(showNextWindow()));
    connect(mMenuBar->getActionCloseWindow(), SIGNAL(triggered()), this, SLOT(closeWindow()));
    connect(mMenuBar->getActionCloseAllWindow(), SIGNAL(triggered()), this, SLOT(closeAllWindow()));
    connect(mMenuBar->getActionLockWindow(), SIGNAL(triggered()), this, SLOT(lockWindow()));
    connect(mMenuBar->getActionPreposeWindow(), SIGNAL(triggered()), this, SLOT(preposeWindow()));

    connect(mMenuBar->getActionGuide(), SIGNAL(triggered()), this, SLOT(guide()));
    connect(mMenuBar->getActionHistory(), SIGNAL(triggered()), this, SLOT(historyChange()));
    connect(mMenuBar->getActionMarkdownLanguage(), SIGNAL(triggered()), this, SLOT(markdownLanguage()));
    connect(mMenuBar->getActionFeedback(), SIGNAL(triggered()), this, SLOT(feedback()));
    connect(mMenuBar->getActionIssue(), SIGNAL(triggered()), this, SLOT(issue()));
    connect(mMenuBar->getActionAbout(), SIGNAL(triggered()), this, SLOT(about()));

    connect(mMenuBar->getActionAbout(), SIGNAL(triggered()), this, SLOT(aboutGitnoter()));
    connect(mMenuBar->getActionPreferences(), SIGNAL(triggered()), this, SLOT(setting()));

    connect(mSettingDialog, SIGNAL(globalHotKeysChanged()), mMenuBar, SLOT(initGlobalHotKeys()));
    connect(mSettingDialog, SIGNAL(autoSyncRepoTimeChanged()), this, SLOT(updateAutoSyncRepoTimer()));
    connect(mSettingDialog, SIGNAL(autoLockTimeChanged()), this, SLOT(updateAutoLockTimer()));
    connect(mLockDialog, SIGNAL(hideActivated()), this, SLOT(updateAutoLockTimer()));
}

void MainWindow::initTempDir()
{
    mGitManager->initLocalRepo(Tools::qstringToConstData(__RepoPath__));

    QDir dir;
    dir.mkdir(__RepoNoteTextPath__);
    dir.mkdir(__RepoNoteDataPath__);
}

void MainWindow::on_noteListWidget_itemClicked(QListWidgetItem *item)
{
    NoteModel *noteModel = item->data(Qt::UserRole).value<NoteModel *>();
    if (noteModel->getUuid() != gConfigModel->openMainWindowNoteUuid()) {
        gConfigModel->setOpenMainWindowNoteUuid(noteModel->getUuid());
        markdownEditorWidget()->init(noteModel, this);
    }
}

void MainWindow::on_splitter_splitterMoved(int, int)
{
    gConfigModel->setMainWindowSplitterSizes(ui->splitter->sizes());
}

void MainWindow::on_pushButton_noteAdd_clicked()
{
    if (gConfigModel->getSideSelectedType() == Gitnoter::Trash) {
        if (gConfigModel->openMainWindowNoteUuid().isEmpty()) {
            return;
        }

        MessageDialog *messageDialog = new MessageDialog(this, this);

        if (mHoldPressedTime + __HOLD_PRESSED_TIME__ < QDateTime::currentMSecsSinceEpoch()) {
            connect(messageDialog, SIGNAL(applyClicked()), this, SLOT(restoreNoteAll()));
            messageDialog->openMessage(tr(u8"确定需要恢复所有笔记吗?"), tr(u8"恢复笔记提示"), tr(u8"确定恢复"));
        }
        else {
            connect(messageDialog, SIGNAL(applyClicked()), this, SLOT(restoreNote()));
            NoteModel *noteModel = ui->noteListWidget->getNoteModel(gConfigModel->openMainWindowNoteUuid());
            const QString category = noteModel->getCategory().isEmpty()
                                     ? ui->groupTreeWidget->topLevelItem(Gitnoter::All)->text(0)
                                     : noteModel->getCategory();
            messageDialog->openMessage(tr(u8"笔记将被恢复到 %1\n\nTip: 长按添加按钮可恢复回收站内所有笔记哦~").arg(category),
                                       tr(u8"恢复笔记提示"),
                                       tr(u8"确定恢复"));
        }
    }
    else {
        appendNote();
    }
}

void MainWindow::on_pushButton_noteSubtract_clicked()
{
    if (gConfigModel->getSideSelectedType() == Gitnoter::Trash) {

        if (gConfigModel->openMainWindowNoteUuid().isEmpty()) {
            return;
        }

        MessageDialog *messageDialog = new MessageDialog(this, this);

        if (mHoldPressedTime + __HOLD_PRESSED_TIME__ < QDateTime::currentMSecsSinceEpoch()) {
            connect(messageDialog, SIGNAL(applyClicked()), this, SLOT(removeNoteAll()));
            messageDialog->openMessage(tr(u8"确定需要清空回收站吗?\n删除后将无法恢复\n\nPs: 大概吧 (*￣rǒ￣)"), tr(u8"清空笔记提示"), tr(u8"确定清空"));
        }
        else {
            connect(messageDialog, SIGNAL(applyClicked()), this, SLOT(removeNote()));
            messageDialog->openMessage(tr(u8"删除后将无法恢复\n\nTip: 长按删除按钮可清空回收站哦~"), tr(u8"删除笔记提示"), tr(u8"确定删除"));
        }
    }
    else {
        trashNote();
    }
}

void MainWindow::on_groupTreeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    if (item->data(0, Qt::UserRole).isNull()) {
        return;
    }

    ui->lineEdit_noteSearch->clear();
    gConfigModel->setSideSelected(groupTreeWidget()->getGroupModel(item));
    updateView(Gitnoter::NoteListWidget);
}

void MainWindow::on_pushButton_add_clicked()
{
    appendGroup();
}

void MainWindow::on_pushButton_subtract_clicked()
{
    Gitnoter::GroupType type = gConfigModel->getSideSelectedType();
    if (Gitnoter::Category <= type) {
        MessageDialog *messageDialog = new MessageDialog(this, this);
        connect(messageDialog, SIGNAL(applyClicked()), this, SLOT(removeGroup()));
        messageDialog->openMessage(tr(u8"笔记本删除后, 笔记本内的笔记将会移动到回收站~ \n\nTip: 还没想好要说些什么o(╯□╰)o"), tr(u8"删除笔记本提示"), tr(u8"确定删除"));
    }
}

void MainWindow::setNoteListTitle()
{
    GroupModel *groupModel = ui->groupTreeWidget->selectedGroupModel();
    if (groupModel) {
        ui->label_groupName->setText(tr(u8"%1 (%2)").arg(groupModel->getName()).arg(groupModel->getCount()));
        if (!groupModel->getCount()) setWindowTitle(groupModel->getName());
    }
}

void MainWindow::restoreNote()
{
    noteListWidget()->restore(gConfigModel->openMainWindowNoteUuid());
    updateView(Gitnoter::NoteListWidget);
}

void MainWindow::restoreNoteAll()
{
    QList<NoteModel *> noteModelList = noteListWidget()->getNoteModelList(Gitnoter::Trash);
    for (auto &&item : noteModelList) {
        noteListWidget()->restore(item->getUuid());
    }
    updateView(Gitnoter::NoteListWidget);
}

void MainWindow::appendNote()
{
    Gitnoter::GroupType type = gConfigModel->getSideSelectedType();
    QString category = gConfigModel->getSideSelectedName();

    if (Gitnoter::Category != type) {
        category = "";
    }

    NoteModel *noteModel = noteListWidget()->append(category);
    gConfigModel->setOpenMainWindowNoteUuid(noteModel->getUuid());
    noteListWidget()->setListWidget();
    groupTreeWidget()->setItemSelected();
    markdownEditorWidget()->init(noteModel, this);
}

void MainWindow::removeNote()
{
    noteListWidget()->remove(gConfigModel->openMainWindowNoteUuid());
    updateView(Gitnoter::NoteListWidget);
}

void MainWindow::removeNoteAll()
{
    QList<NoteModel *> noteModelList = noteListWidget()->getNoteModelList(Gitnoter::Trash);
    for (auto &&item : noteModelList) {
        noteListWidget()->remove(item->getUuid());
    }
    updateView(Gitnoter::NoteListWidget);
}

void MainWindow::trashNote()
{
    noteListWidget()->trash(gConfigModel->openMainWindowNoteUuid());
    updateView(Gitnoter::NoteListWidget);
}

GroupTreeWidget *MainWindow::groupTreeWidget()
{
    return ui->groupTreeWidget;
}

NoteListWidget *MainWindow::noteListWidget()
{
    return ui->noteListWidget;
}

MarkdownEditorWidget *MainWindow::markdownEditorWidget()
{
    return ui->markdownEditorWidget;
}

void MainWindow::init()
{
    ui->noteListWidget->init(this);
    ui->groupTreeWidget->init(ui->noteListWidget->getNoteModelList(), this);
    updateView(Gitnoter::NoteListTitle | Gitnoter::MarkdownEditorWidget | Gitnoter::NoteListSort);
}

void MainWindow::removeGroup()
{
    Gitnoter::GroupType type = gConfigModel->getSideSelectedType();
    const QString name = gConfigModel->getSideSelectedName();

    if (Gitnoter::Category <= type) {
        gConfigModel->setSideSelected(groupTreeWidget()->getGroupModel(Gitnoter::All));
        ui->groupTreeWidget->remove(type, name);
        updateView(Gitnoter::NoteListWidget);
    }
}

void MainWindow::appendGroup()
{
    Gitnoter::GroupType type = gConfigModel->getSideSelectedType();
    QString name = "";
    for (int i = 0; i < 100; ++i) {
        name = ((Gitnoter::Tag == type) ? tr(u8"新建标签%1") : tr(u8"新建笔记本%1")).arg(i == 0 ? "" : QString::number(i));
        if (groupTreeWidget()->append(type, name)) {
            groupTreeWidget()->editItem(groupTreeWidget()->getTreeWidgetItem(type, name));
            gConfigModel->setSideSelected(type, name);
            updateView(Gitnoter::GroupTreeWidget);
            break;
        }
    }
}

void MainWindow::searchNote(const QString &text)
{
    noteListWidget()->search(text.isEmpty() ? ui->lineEdit_noteSearch->text() : text);
}

void MainWindow::sortNote()
{

}

QStackedWidget *MainWindow::stackedWidget()
{
    return ui->stackWidget_editor;
}

QSplitter *MainWindow::splitter()
{
    return ui->splitter;
}

void MainWindow::on_lineEdit_noteSearch_textChanged(const QString &arg1)
{
    mSearchSingleTimeout->singleShot(500, this, SLOT(searchNote()));
}

void MainWindow::updateView(Gitnoter::UpdateViewFlags flags)
{
    if (flags.testFlag(Gitnoter::GroupTreeWidget)) {
        groupTreeWidget()->setItemSelected();
    }

    if (flags.testFlag(Gitnoter::NoteListWidget)) {
        noteListWidget()->setListWidget();
    }

    if (flags.testFlag(Gitnoter::MarkdownEditorWidget)) {
        markdownEditorWidget()->init(gConfigModel->openMainWindowNoteUuid(), this);
    }

    if (flags.testFlag(Gitnoter::NoteListTitle)) {
        setNoteListTitle();
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange || event->type() == QEvent::ActivationChange) {
        if (windowState().testFlag(Qt::WindowFullScreen)) {
            gConfigModel->setMainWindowFullScreen(true);
        }
        else if ((windowState() == Qt::WindowNoState || windowState() == Qt::WindowMaximized) && !isFullScreen()) {
            gConfigModel->setMainWindowFullScreen(false);
        }
    }
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    gConfigModel->setMainWindowRect(geometry());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    gConfigModel->setMainWindowRect(geometry());
}

MenuBar *MainWindow::menuBar()
{
    return mMenuBar;
}

void MainWindow::openSettingWidget()
{

}

void MainWindow::setSearchFocus()
{
    ui->lineEdit_noteSearch->setFocus();
}

void MainWindow::updateAutoSyncRepoTimer()
{
    int autoSyncRepoTime = gConfigModel->getAutoSyncRepoTime();
    mAutoSyncRepoTimer->singleShot(autoSyncRepoTime, this, SLOT(syncRepo()));
}

void MainWindow::updateAutoLockTimer()
{
    int autoLockTime = gConfigModel->getAutoLockTime();
    mAutoLockTimer->singleShot(autoLockTime, this, SLOT(lockWindow()));
}

void MainWindow::lockWindow()
{
    if (mLockDialog->isHidden()) {
        mLockDialog->showWidget();
    }

    gConfigModel->setHasLockWindow(true);
}

void MainWindow::showSidebar(bool clicked)
{
    mMenuBar->getActionSidebar()->setChecked(clicked);

    ui->widget_sidebar->setVisible(clicked);
    gConfigModel->setSidebarWidget(clicked);
}

void MainWindow::showListBar(bool clicked)
{
    mMenuBar->getActionListbar()->setChecked(clicked);

    ui->widget_listBar->setVisible(clicked);
    gConfigModel->setListBarWidget(clicked);
}

void MainWindow::enterFullScreen()
{
    (isActiveWindow() && isFullScreen()) ? showNormal() : showFullScreen();
}

void MainWindow::fullScreenEditMode()
{
    if (isActiveWindow() && isFullScreen()) {
        bool sidebar = gConfigModel->getSidebarWidget();
        bool listBar = gConfigModel->getListBarWidget();

        ui->widget_listBar->setVisible(sidebar);
        ui->widget_sidebar->setVisible(listBar);

        showNormal();
    }
    else {
        ui->widget_listBar->setVisible(false);
        ui->widget_sidebar->setVisible(false);

        showFullScreen();
    }
}

void MainWindow::newWindow(QListWidgetItem *)
{
    const QString uuid = gConfigModel->openMainWindowNoteUuid();
    if (uuid.isEmpty()) {
        return;
    }

    for (auto &&window : mMenuBar->windowMenuWidgetList()) {
        if (MarkdownEditorWidget *widget
                = reinterpret_cast<MarkdownEditorWidget *>(qobject_cast<MarkdownEditorWidget *>(window))) {
            if (widget->noteModel()->getUuid() == uuid) {
                widget->raise();
                window->activateWindow();
                return;
            }
        }
    }

    MarkdownEditorWidget *markdownEditorWidget = new MarkdownEditorWidget(this);
    markdownEditorWidget->init(uuid, this);
    markdownEditorWidget->show();

    mMenuBar->addActionToWindowMenu(markdownEditorWidget);
}

void MainWindow::showLastWindow()
{
    QWidgetList windowMenuWidgetList = mMenuBar->windowMenuWidgetList();
    if (windowMenuWidgetList.length() < 2) {
        return;
    }

    int nowActiveWindowIndex = -1;
    for (int i = 0; i < windowMenuWidgetList.length(); ++i) {
        if (windowMenuWidgetList[i]->isActiveWindow()) {
            nowActiveWindowIndex = i;
            break;
        }
    }

    if (nowActiveWindowIndex < 1) {
        return;
    }

    windowMenuWidgetList[nowActiveWindowIndex - 1]->raise();
    windowMenuWidgetList[nowActiveWindowIndex - 1]->activateWindow();

    mMenuBar->addActionToWindowMenu();
}

void MainWindow::showNextWindow()
{
    QWidgetList windowMenuWidgetList = mMenuBar->windowMenuWidgetList();

    if (windowMenuWidgetList.length() < 2) {
        return;
    }

    int nowActiveWindowIndex = -1;
    for (int i = 0; i < windowMenuWidgetList.length(); ++i) {
        if (windowMenuWidgetList[i]->isActiveWindow()) {
            nowActiveWindowIndex = i;
            break;
        }
    }

    if (nowActiveWindowIndex == -1 || nowActiveWindowIndex == windowMenuWidgetList.length() - 1) {
        return;
    }

    windowMenuWidgetList[nowActiveWindowIndex + 1]->raise();
    windowMenuWidgetList[nowActiveWindowIndex + 1]->activateWindow();

    mMenuBar->addActionToWindowMenu();
}

void MainWindow::closeWindow()
{
    QWidgetList windowMenuWidgetList = mMenuBar->windowMenuWidgetList();

    for (auto &&widget : windowMenuWidgetList) {
        if (widget->isActiveWindow()) {
            mMenuBar->removeActionToWindowMenu(widget);
            widget->close();
            break;
        }
    }
}

void MainWindow::closeAllWindow()
{
    QWidgetList windowMenuWidgetList = mMenuBar->windowMenuWidgetList();

    for (auto &&widget : windowMenuWidgetList) {
        if (MarkdownEditorWidget *markdownEditorWidget
                = reinterpret_cast<MarkdownEditorWidget *>(qobject_cast<MarkdownEditorWidget *>(widget))) {
            mMenuBar->removeActionToWindowMenu(markdownEditorWidget);
            markdownEditorWidget->close();
        }
    }
}

void MainWindow::preposeWindow()
{
    QWidgetList windowMenuWidgetList = mMenuBar->windowMenuWidgetList();

    if (windowMenuWidgetList.length() < 2) {
        return;
    }

    QWidget *nowActiveWindow = nullptr;
    for (auto &&widget : windowMenuWidgetList) {
        if (widget->isActiveWindow()) {
            nowActiveWindow = widget;
            break;
        }
        else {
            widget->raise();
            widget->activateWindow();
        }
    }

    if (nowActiveWindow) {
        nowActiveWindow->raise();
        nowActiveWindow->activateWindow();
    }
}

void MainWindow::guide()
{
    QDesktopServices::openUrl(QUrl(__GuideUrl__));
}

void MainWindow::historyChange()
{
    QDesktopServices::openUrl(QUrl(__HistoryChangeUrl__));
}

void MainWindow::markdownLanguage()
{
    QDesktopServices::openUrl(QUrl(__MarkdownLanguageUrl__));
}

void MainWindow::feedback()
{
    const QString body = QString("\n\n") + VER_PRODUCTNAME_STR + " for "
                         + QSysInfo::productType().toUpper() + " Version "
                         + VER_PRODUCTVERSION_STR + " (build " + QString::number(VER_PRODUCTBUILD_STR) + ")"
                         + "\n" + QSysInfo::prettyProductName();
    QDesktopServices::openUrl(QUrl(QString("mailto:?to=%1&subject=Gitnoter for %2: Feedback&body=%3").arg(
                    __EmailToUser__, QSysInfo::prettyProductName(), body), QUrl::TolerantMode));
}

void MainWindow::issue()
{
    QDesktopServices::openUrl(QUrl(__IssueUrl__));
}

void MainWindow::about()
{

}

void MainWindow::closeEvent(QCloseEvent *event)
{
    exit(0);
}

void MainWindow::importEvernote()
{
    (new ImportNoteDialog(this))->init();
}

void MainWindow::aboutGitnoter()
{
    mAboutDialog->exec();
}

void MainWindow::setting()
{
    mSettingDialog->open();
}

void MainWindow::fullScreenShot(size_t)
{
    if (!isActiveWindow() || ui->markdownEditorWidget->editorHasFocus()) {
        return;
    }

    appendNote();
    const QPixmap pixmap = ScreenShot::fullScreenShot();
    ui->markdownEditorWidget->savePixmap(pixmap, tr(u8"全屏"));
}

void MainWindow::windowScreenShot(size_t)
{
    if (!isActiveWindow() || ui->markdownEditorWidget->editorHasFocus()) {
        return;
    }

    appendNote();
    QPixmap pixmap = ScreenShot::windowScreenShot();
    if (!pixmap.isNull()) {
        ui->markdownEditorWidget->savePixmap(pixmap, tr(u8"窗口"));
    }
}

void MainWindow::partScreenShot(size_t)
{
    if (!isActiveWindow() || ui->markdownEditorWidget->editorHasFocus()) {
        return;
    }

    appendNote();
    ScreenShot *screenShot = new ScreenShot(this);
    if (screenShot->exec() == QDialog::Accepted) {
        const QPixmap pixmap = screenShot->shotPixmap();
        if (!pixmap.isNull()) {
            ui->markdownEditorWidget->savePixmap(pixmap, tr(u8"截屏"));
        }
    }
}

void MainWindow::reload()
{
    ui->noteListWidget->clear();
    init();
}

void MainWindow::setRemoteToRepo()
{
    const QString repoUrl = gConfigModel->getRepoUrl();
    const QString repoEmail = gConfigModel->getRepoEmail();
    const QString repoPassword = gConfigModel->getRepoPassword();
    const QString repoUrlNew = gConfigModel->getRepoUrlNew();
    const QString repoEmailNew = gConfigModel->getRepoEmailNew();
    const QString repoPasswordNew = gConfigModel->getRepoPasswordNew();
    const QString repoPathTemp = QString(__RepoPath__).replace(__RepoName__, __RepoNameTemp__);

    const char *email = Tools::qstringToConstData(repoEmailNew);
    const char *password = Tools::qstringToConstData(repoPasswordNew);
    const char *username = Tools::qstringToConstData(repoEmailNew);
    const char *path = Tools::qstringToConstData(repoPathTemp);
    const char *url = Tools::qstringToConstData(repoUrlNew);

    if (repoUrl != repoUrlNew) {
        QDir(repoPathTemp).removeRecursively();

        GitManager *gitManager = new GitManager();
        gitManager->setUserPass(email, password);
        gitManager->setSignature(username, email);
        int result = gitManager->clone(url, path);
        if (result < 0) {
            MessageDialog::openMessage(this, tr(u8"更新仓库失败, 请确认仓库地址和账户密码~"));
            return;
        }

        delete gitManager;

        gConfigModel->setRepoUrl(repoUrlNew);
        gConfigModel->setRepoEmail(repoEmailNew);
        gConfigModel->setRepoPassword(repoPasswordNew);
        gConfigModel->setLocalRepoStatus(Gitnoter::RemoteRepo);

        MessageDialog *messageDialog = MessageDialog::openMessage(this, tr(u8"更新仓库成功, 是否把现有的数据拷贝至新仓库中~"));
        connect(messageDialog, SIGNAL(applyClicked()), this, SLOT(setRepoApplyClicked()));
        connect(messageDialog, SIGNAL(closeClicked()), this, SLOT(setRepoCloseClicked()));
    }
}

void MainWindow::setRepoApplyClicked()
{
    const QString repoPathTemp = QString(__RepoPath__).replace(__RepoName__, __RepoNameTemp__);
    const QString repoNoteTextPathTemp = QString(__RepoNoteTextPath__).replace(__RepoName__, __RepoNameTemp__);
    QDir().mkdir(repoNoteTextPathTemp);
    QList<NoteModel *> noteModelList = ui->noteListWidget->getNoteModelList();
    for (auto &&noteModel : noteModelList) {
        const QString newPath = QDir(repoNoteTextPathTemp).filePath(noteModel->getUuid());
        QDir(newPath).removeRecursively();
        QDir().rename(noteModel->getNoteDir(), newPath);
    }

    const QString repoNoteDataPathTemp = QString(__RepoNoteDataPath__).replace(__RepoName__, __RepoNameTemp__);
    QDir(repoNoteDataPathTemp).removeRecursively();
    QDir().rename(__RepoNoteDataPath__, repoNoteDataPathTemp);

    QDir(__RepoPath__).removeRecursively();
    QDir().rename(repoPathTemp, __RepoPath__);

    syncRepo();
}

void MainWindow::setRepoCloseClicked()
{
    const QString repoPathTemp = QString(__RepoPath__).replace(__RepoName__, __RepoNameTemp__);
    QDir(__RepoPath__).removeRecursively();
    QDir().rename(repoPathTemp, __RepoPath__);

    reload();
}

void MainWindow::syncRepo()
{
    if (Gitnoter::RemoteRepo != gConfigModel->getLocalRepoStatus() || mSyncRepoThread->isRunning()) {
        return;
    }

    const QString repoEmail = gConfigModel->getRepoEmail();
    const QString repoPassword = gConfigModel->getRepoPassword();
    const QString repoUsername = gConfigModel->getRepoUsername();

    if (repoEmail.isEmpty() || repoPassword.isEmpty() || repoUsername.isEmpty()) {
        gConfigModel->setLocalRepoStatus(Gitnoter::LocalRepo);
        return;
    }

    mGitManager->initLocalRepo(Tools::qstringToConstData(__RepoPath__));
    mGitManager->setUserPass(Tools::qstringToConstData(repoEmail), Tools::qstringToConstData(repoPassword));
    mGitManager->setSignature(Tools::qstringToConstData(repoUsername), Tools::qstringToConstData(repoEmail));

    bool repoChange = false;

    connect(mSyncRepoThread, &QThread::started, [&]() {
        mGitManager->commitA();
        int startLogCount = static_cast<int>(mGitManager->getLogList().size());

        mGitManager->pull();
        int endPullLogCount = static_cast<int>(mGitManager->getLogList().size());

        mGitManager->commitA();
        int endCommitLogCount = static_cast<int>(mGitManager->getLogList().size());

        if (endPullLogCount != endCommitLogCount) {
            mGitManager->push();
        }

        repoChange = (startLogCount != endPullLogCount);

        disconnect(mSyncRepoThread);
        mSyncRepoThread->exit();
    });
    mSyncRepoThread->start();

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [&, timer]()  {
        if (!mSyncRepoThread->isRunning()) {
            if (repoChange) {
                reload();
            }
            timer->stop();
            delete timer;
        }
    });
    timer->start(1000);
}

void MainWindow::openPurchasePanel()
{
    if (mEnterLicenseDialog->isLicense()
        || mOpenPurchasePanelTimestamp + __OPEN_PURCHASE_PANEL_TIME__ > QDateTime::currentSecsSinceEpoch()) {
        return;
    }

    MessageDialog *messageDialog = new MessageDialog(this, this);
    messageDialog->setMessageInfo(
            tr(u8"您的应用尚未激活\n购买许可证以获得完整体验 %1").arg(__PurchaseLicenseUrl__),
            tr(u8"感谢您试用 %1（￣▽￣）").arg(VER_PRODUCTNAME_STR), tr(u8"购买"));
    connect(messageDialog, &MessageDialog::applyClicked, [=]() {
        mOpenPurchasePanelTimestamp = (int) QDateTime::currentSecsSinceEpoch();
        QDesktopServices::openUrl(QUrl(__PurchaseLicenseUrl__));
    });
    connect(messageDialog, &MessageDialog::closeClicked, [=]() {
        mOpenPurchasePanelTimestamp = (int) QDateTime::currentSecsSinceEpoch();
    });

    messageDialog->exec();
}

void MainWindow::showEvent(QShowEvent *showEvent)
{
    QWidget::showEvent(showEvent);

    mGAnalytics->sendScreenView(objectName());
}

void MainWindow::on_pushButton_noteSubtract_pressed()
{
    mHoldPressedTime = QDateTime::currentMSecsSinceEpoch();
}

void MainWindow::on_pushButton_noteAdd_pressed()
{
    mHoldPressedTime = QDateTime::currentMSecsSinceEpoch();
}
