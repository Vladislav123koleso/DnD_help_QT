#ifndef MASTERPAGE_H
#define MASTERPAGE_H

#include <QWidget>
#include <QListWidget>
#include <QStackedWidget>
#include "noteswidget.h"
#include "npccreatorwidget.h"
#include "npclistwidget.h"
#include "namegeneratorwidget.h"
#include "artifactgeneratorwidget.h"

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
    NotesWidget *notesWidget = nullptr;
    NpcCreatorWidget *npcCreatorWidget = nullptr;
    NpcListWidget *npcListWidget = nullptr;
    NameGeneratorWidget *nameGeneratorWidget = nullptr;
    ArtifactGeneratorWidget *artifactGeneratorWidget = nullptr;
    
    void setupUi();
};

#endif // MASTERPAGE_H
