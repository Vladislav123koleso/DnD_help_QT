#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QListWidget>
#include <QResizeEvent>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QApplication>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setObjectName(QStringLiteral("AppMainWindow"));
    setWindowTitle(QStringLiteral("DnD Helper"));
    setMinimumSize(940, 620);
    resize(1360, 860);

    stackedWidget = new QStackedWidget(this);
    stackedWidget->setObjectName(QStringLiteral("RootStack"));
    setCentralWidget(stackedWidget);

    startPage = new StartPage(this);
    playerPage = new PlayerPage(this);
    masterPage = new MasterPage(this);

    stackedWidget->addWidget(startPage);
    stackedWidget->addWidget(playerPage);
    stackedWidget->addWidget(masterPage);

    stackedWidget->setCurrentWidget(startPage);

    connect(startPage, &StartPage::campaignSelected, this, &MainWindow::onCampaignSelected);
    connect(playerPage, &PlayerPage::mainMenuRequested, this, &MainWindow::onMainMenuRequested);
    connect(masterPage, &MasterPage::mainMenuRequested, this, &MainWindow::onMainMenuRequested);

    applyResponsiveUi(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onCampaignSelected(const QString &campaignName, bool isMaster)
{
    if (isMaster) {
        masterPage->setCampaign(campaignName);
        stackedWidget->setCurrentWidget(masterPage);
    } else {
        playerPage->setCampaign(campaignName);
        stackedWidget->setCurrentWidget(playerPage);
    }

    applyResponsiveUi(true);
}

void MainWindow::onMainMenuRequested()
{
    stackedWidget->setCurrentWidget(startPage);
    applyResponsiveUi(true);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    applyResponsiveUi();
}

void MainWindow::applyResponsiveUi(bool force)
{
    const int w = width();
    const bool nextCompact = w < 1180;
    const bool nextTight = w < 980;

    if (!force && compactUi == nextCompact && tightUi == nextTight) {
        return;
    }

    compactUi = nextCompact;
    tightUi = nextTight;

    setProperty("uxCompact", compactUi);
    setProperty("uxTight", tightUi);
    if (stackedWidget) {
        stackedWidget->setProperty("uxCompact", compactUi);
        stackedWidget->setProperty("uxTight", tightUi);
    }

    const QList<QTabWidget*> tabWidgets = findChildren<QTabWidget*>();
    for (QTabWidget *tabWidget : tabWidgets) {
        if (!tabWidget) {
            continue;
        }
        tabWidget->setUsesScrollButtons(compactUi);
        tabWidget->setElideMode(compactUi ? Qt::ElideRight : Qt::ElideNone);
        if (QTabBar *bar = tabWidget->tabBar()) {
            bar->setUsesScrollButtons(compactUi);
            bar->setExpanding(!compactUi);
        }
    }

    const QList<QToolBar*> toolBars = findChildren<QToolBar*>();
    for (QToolBar *toolBar : toolBars) {
        if (toolBar) {
            toolBar->setIconSize(tightUi ? QSize(14, 14) : QSize(18, 18));
        }
    }

    if (QListWidget *sidebarNav = findChild<QListWidget*>(QStringLiteral("SidebarNav"))) {
        sidebarNav->setMinimumWidth(tightUi ? 156 : 194);
    }

    if (QWidget *sidebarPanel = findChild<QWidget*>(QStringLiteral("SidebarPanel"))) {
        const bool collapsed = sidebarPanel->property("sidebarCollapsed").toBool();
        if (!collapsed) {
            sidebarPanel->setMinimumWidth(tightUi ? 170 : 210);
            sidebarPanel->setMaximumWidth(tightUi ? 260 : 340);
        }
    }

    if (QApplication *app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        app->setStyleSheet(app->styleSheet());
    }
    update();
}
