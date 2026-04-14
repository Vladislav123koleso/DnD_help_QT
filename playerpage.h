#ifndef PLAYERPAGE_H
#define PLAYERPAGE_H

#include <QWidget>
#include <QTabWidget>
#include <QStackedWidget>
#include <QMap>
#include "race_selection_page.h"
#include "class_selection_page.h"
#include "charactersheet.h"
#include "character.h"
#include "race.h"
#include "class.h"

class PlayerPage : public QWidget
{
    Q_OBJECT
public:
    explicit PlayerPage(QWidget *parent = nullptr);
    void setCampaign(const QString &campaignName);

signals:
    void mainMenuRequested();

private slots:
    void startCharacterCreation();
    void showCharacterInfo();
    void onRaceChosen(const Race &race);
    void onClassChosen(const Class &cls);

private:
    QTabWidget *tabWidget;
    QStackedWidget *charStack; // Stack inside the character tab
    RaceSelectionPage *racePage; // The race selection page
    ClassSelectionPage *classPage; // The class selection page
    CharacterSheet *characterSheet; // The character sheet widget
    QString currentCampaign;
    
    Character *currentCharacter;
    int targetCharacterLevel;
    int allocatedClassLevels;
    QMap<QString, int> selectedClassLevels;
    
    void setupUi();
    void resetCreationProgress();
    void updateCharacterClassSummary();
    int remainingLevelsToAllocate() const;
    bool chooseAndApplyBaseAbilityScores();
    void applyRaceAbilityBonuses(const Race &race);
};

#endif // PLAYERPAGE_H
