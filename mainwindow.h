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
class QResizeEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onCampaignSelected(const QString &campaignName, bool isMaster);
    void onMainMenuRequested();

private:
    void applyResponsiveUi(bool force = false);

    Ui::MainWindow *ui;
    QStackedWidget *stackedWidget;
    StartPage *startPage;
    PlayerPage *playerPage;
    MasterPage *masterPage;
    bool compactUi = false;
    bool tightUi = false;
};
#endif // MAINWINDOW_H
