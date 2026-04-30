#ifndef PLAYERPAGE_H
#define PLAYERPAGE_H

#include <QWidget>
#include <QTabWidget>
#include <QStackedWidget>
#include <QJsonObject>
#include <QMap>
#include "background.h"
#include "feat.h"
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
    void levelUpCharacter();
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
    QMap<QString, int> baseAbilityScores;
    QMap<QString, int> selectedClassLevels;
    QMap<QString, Class> selectedClasses;
    QStringList classSelectionOrder;
    bool levelUpInProgress = false;
    int levelUpPreviousMaxHp = 0;
    int levelUpPreviousFeatSlots = 0;
    QJsonObject levelUpSnapshot;
    
    void setupUi();
    void resetCreationProgress();
    void resetClassSelection();
    void updateCharacterClassSummary();
    void prepareSelectedClassesFromCharacter();
    void applyRaceDerivedBenefits(const Race &race);
    void cancelPendingLevelUp(bool restoreCharacter = false);
    int remainingLevelsToAllocate() const;
    bool chooseBaseAbilityScores();
    void applyBaseAbilityScores();
    bool applyRaceAbilityBonuses(const Race &race);
    void chooseCharacterBackground();
    bool applyBackground(const Background &background);
    bool chooseStartingFeats();
    void applyFeat(const Feat &feat);
    bool chooseAbilityScoreImprovement(const QString &sourceLabel);
    void applyAbilityIncrease(const QString &abilityName, int amount);
    bool resolveChosenLanguages(const QStringList &languageEntries, const QString &sourceName, const QStringList &existingLanguages, QStringList *resolvedLanguages);
    bool chooseRaceGrantedSpells(const Race &race);
    void chooseStartingEquipment();
    void chooseStartingSpells();
    void addInventoryItem(const Item &item);
    void addInventoryTextEntry(const QString &entry);
    void completeCharacterCreation();
    void synchronizeCharacterFromClasses(bool refillCurrentHp);
    void saveCurrentCharacter();
    void loadCharacterForCurrentCampaign();
};

#endif // PLAYERPAGE_H
