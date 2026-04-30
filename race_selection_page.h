#ifndef RACE_SELECTION_PAGE_H
#define RACE_SELECTION_PAGE_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMouseEvent>
#include <QGridLayout>
#include <QStackedWidget>
#include <QDebug>
#include <QFile>
#include <QStringList>
#include "racecard.h"
#include "racedetailspage.h"

class RaceSelectionPage : public QWidget {
    Q_OBJECT
public:
    explicit RaceSelectionPage(QWidget *parent = nullptr);

signals:
    void raceChosen(const Race &race);

public:
    Race getRaceData(const QString &name);

    void onRaceSelected(const QString &raceName);
    void showList();
    void confirmSelection();

private:
   void setupUi();
   QStackedWidget *stackedWidget;
   QWidget *listPage;
   RaceDetailsPage *detailsPage;
    QWidget *scrollContent;
   
   QMap<QString, Race> raceData;
    QMap<QString, QStringList> subraceGroups;
   void loadRaceData();
    void buildSubraceGroups();
    void loadSubracesMapFromJson();
    QString resolveRacesJsonPath() const;
    QString resolveSubracesMapPath() const;
    QString detectImagePath(const QString &raceName) const;
    QString shortDescription(const Race &race) const;
    Race displayRaceForSelection(const QString &raceName) const;
    Race mergeRaceWithSubrace(const Race &baseRace, const Race &subrace) const;
    QString parentRaceNameForSubrace(const QString &raceName) const;
    QMap<QString, QString> displayTraitsForParentRace(const Race &race) const;
    QStringList subraceOptionsForRace(const QString &raceName) const;
};



#endif // RACE_SELECTION_PAGE_H
