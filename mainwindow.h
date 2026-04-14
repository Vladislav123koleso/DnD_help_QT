#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include "startpage.h"
#include "playerpage.h"
#include "masterpage.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCampaignSelected(const QString &campaignName, bool isMaster);
    void onMainMenuRequested();

private:
    Ui::MainWindow *ui;
    QStackedWidget *stackedWidget;
    StartPage *startPage;
    PlayerPage *playerPage;
    MasterPage *masterPage;
};
#endif // MAINWINDOW_H
