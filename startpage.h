#ifndef STARTPAGE_H
#define STARTPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>

class StartPage : public QWidget
{
    Q_OBJECT
public:
    explicit StartPage(QWidget *parent = nullptr);

signals:
    void campaignSelected(const QString &campaignName, bool isMaster);

private slots:
    void onCreateCampaignClicked();
    void onCampaignButtonClicked(const QString &campaignName);
    void onDeleteCampaignClicked(const QString &campaignName);

private:
    QPushButton *createButton;
    QWidget *campaignsContainer;
    QVBoxLayout *campaignsLayout;
    
    void loadCampaigns();
    void clearLayout(QLayout *layout);
};

#endif // STARTPAGE_H
