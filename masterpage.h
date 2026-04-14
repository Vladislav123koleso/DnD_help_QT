#ifndef MASTERPAGE_H
#define MASTERPAGE_H

#include <QWidget>
#include <QListWidget>
#include <QStackedWidget>

class MasterPage : public QWidget
{
    Q_OBJECT
public:
    explicit MasterPage(QWidget *parent = nullptr);
    void setCampaign(const QString &campaignName);

signals:
    void mainMenuRequested();

private:
    QString currentCampaign;
    QListWidget *navBar;
    QStackedWidget *contentStack;
    
    void setupUi();
};

#endif // MASTERPAGE_H
