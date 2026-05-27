#ifndef CHARACTERCREATIONSERVICE_H
#define CHARACTERCREATIONSERVICE_H

#include <QMap>
#include <QObject>
#include <QStringList>

#include "character.h"
#include "class.h"
#include "race.h"

#include "background.h"
#include "feat.h"

class QWidget;
class RaceSelectionPage;
class ClassSelectionPage;

class CharacterCreationService : public QObject
{
    Q_OBJECT
public:
    explicit CharacterCreationService(QObject *parent = nullptr);
    ~CharacterCreationService() override;

    void setParentWidget(QWidget *widget);
    void setCharacter(Character *character);
    void setTargetLevel(int level);
    void setBaseAbilityScores(const QMap<QString, int> &scores);
    void resetForNpc();
    void syncAbilityScoresFromCharacter();

    Character *character() const { return m_character; }

    bool runCreationWizard();
    bool runAbilityScoreStep();
    bool runRaceStep();
    bool runClassStep();
    bool runBackgroundAndEquipmentStep();
    bool runSpellsStep();

private:
    bool runClassSelectionFlow();
    bool finalizeCreation(bool includeSpells = true);
    void chooseCharacterBackground();
    bool applyBackground(const Background &background);
    bool chooseStartingFeats();
    void applyFeat(const Feat &feat);
    void chooseStartingEquipment();
    void addInventoryItem(const Item &item);
    void addInventoryTextEntry(const QString &entry);
    bool chooseAbilityScoreImprovement(const QString &sourceLabel);
    int remainingLevelsToAllocate() const;
    void resetClassSelection();
    void updateCharacterClassSummary();
    void applyRaceDerivedBenefits(const Race &race);
    bool chooseRaceGrantedSpells(const Race &race);
    bool chooseClassSkillProficiencies(Class &cls, bool multiclassEntry);
    bool chooseClassFeatureChoices(const Class &cls, int classLevel);
    bool applyClassLevelChange(const Class &cls, int levelsToAdd);
    bool chooseBaseAbilityScores();
    void applyBaseAbilityScores();
    bool applyRaceAbilityBonuses(const Race &race);
    void applyAbilityIncrease(const QString &abilityName, int amount);
    bool resolveChosenLanguages(
        const QStringList &languageEntries,
        const QString &sourceName,
        const QStringList &existingLanguages,
        QStringList *resolvedLanguages);
    bool chooseSubclassForClass(Class *cls, int classLevel);
    void chooseStartingSpells();
    void synchronizeCharacterFromClasses(bool refillCurrentHp);
    bool showRacePicker(Race *selectedRace);
    bool showClassPicker(Class *selectedClass);
    bool applyRaceSelection(const Race &race);

    QWidget *m_parentWidget = nullptr;
    Character *m_character = nullptr;
    RaceSelectionPage *m_racePage = nullptr;
    ClassSelectionPage *m_classPage = nullptr;

    int m_targetLevel = 1;
    int m_allocatedClassLevels = 0;
    QMap<QString, int> m_baseAbilityScores;
    QMap<QString, int> m_selectedClassLevels;
    QMap<QString, Class> m_selectedClasses;
    QMap<QString, QString> m_selectedSubclassNames;
    QMap<QString, QStringList> m_selectedClassSkillSelections;
    QMap<QString, QString> m_selectedClassFeatureChoices;
    QStringList m_classSelectionOrder;
    bool m_levelUpInProgress = false;
    bool m_levelUpChoosingMulticlass = false;
    int m_levelUpPreviousMaxHp = 0;
    int m_levelUpPreviousFeatSlots = 0;
};

#endif // CHARACTERCREATIONSERVICE_H
