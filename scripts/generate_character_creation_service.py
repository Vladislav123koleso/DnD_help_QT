#!/usr/bin/env python3
import re
from pathlib import Path

root = Path(__file__).resolve().parent.parent
text = (root / "playerpage.cpp").read_text(encoding="utf-8")
lines = text.splitlines(True)

helpers1 = "".join(lines[28:2137])
helpers2 = "".join(lines[2736:2998])

ranges = [
    (2419, 2447),
    (2454, 2472),
    (2504, 2519),
    (2521, 2717),
    (3000, 3060),
    (3062, 3118),
    (3120, 3166),
    (3255, 3323),
    (3325, 3337),
    (3339, 3478),
    (3480, 3575),
    (3645, 3664),
    (3894, 3948),
    (4028, 4416),
    (4541, 4610),
]

replacements = [
    ("PlayerPage::", "CharacterCreationService::"),
    ("currentCharacter", "m_character"),
    ("classPage", "m_classPage"),
    ("targetCharacterLevel", "m_targetLevel"),
    ("allocatedClassLevels", "m_allocatedClassLevels"),
    ("baseAbilityScores", "m_baseAbilityScores"),
    ("selectedClassLevels", "m_selectedClassLevels"),
    ("selectedClasses", "m_selectedClasses"),
    ("selectedSubclassNames", "m_selectedSubclassNames"),
    ("selectedClassSkillSelections", "m_selectedClassSkillSelections"),
    ("selectedClassFeatureChoices", "m_selectedClassFeatureChoices"),
    ("classSelectionOrder", "m_classSelectionOrder"),
    ("levelUpInProgress", "m_levelUpInProgress"),
    ("levelUpChoosingMulticlass", "m_levelUpChoosingMulticlass"),
    ("levelUpPreviousMaxHp", "m_levelUpPreviousMaxHp"),
    ("levelUpPreviousFeatSlots", "m_levelUpPreviousFeatSlots"),
]

methods = []
for start, end in ranges:
    block = "".join(lines[start - 1 : end])
    for old, new in replacements:
        block = block.replace(old, new)
    block = re.sub(r"(?<![A-Za-z0-9_])this(?![A-Za-z0-9_])", "m_parentWidget", block)
    methods.append(block)

header = r'''#include "charactercreationservice.h"

#include "characterprogressionrules.h"
#include "charactersheet.h"
#include "databasemanager.h"
#include "race_selection_page.h"
#include "class_selection_page.h"

#include <QComboBox>
#include <QDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>
#include <algorithm>

namespace {
'''

footer = r'''
}

CharacterCreationService::CharacterCreationService(QObject *parent)
    : QObject(parent)
{
    m_racePage = new RaceSelectionPage;
    m_classPage = new ClassSelectionPage;
}

CharacterCreationService::~CharacterCreationService()
{
    delete m_racePage;
    delete m_classPage;
}

void CharacterCreationService::setParentWidget(QWidget *widget)
{
    m_parentWidget = widget;
}

void CharacterCreationService::setCharacter(Character *character)
{
    m_character = character;
}

void CharacterCreationService::setTargetLevel(int level)
{
    m_targetLevel = qBound(1, level, 20);
    if (m_character) {
        m_character->level = m_targetLevel;
    }
}

void CharacterCreationService::setBaseAbilityScores(const QMap<QString, int> &scores)
{
    m_baseAbilityScores = scores;
}

void CharacterCreationService::resetForNpc()
{
    m_levelUpInProgress = false;
    m_levelUpChoosingMulticlass = false;
}

int CharacterCreationService::remainingLevelsToAllocate() const
{
    return qMax(0, m_targetLevel - m_allocatedClassLevels);
}

void CharacterCreationService::syncAbilityScoresFromCharacter()
{
    if (!m_character) {
        return;
    }
    m_baseAbilityScores.clear();
    m_baseAbilityScores.insert(QStringLiteral("Сила"), m_character->strength);
    m_baseAbilityScores.insert(QStringLiteral("Ловкость"), m_character->dexterity);
    m_baseAbilityScores.insert(QStringLiteral("Телосложение"), m_character->constitution);
    m_baseAbilityScores.insert(QStringLiteral("Интеллект"), m_character->intelligence);
    m_baseAbilityScores.insert(QStringLiteral("Мудрость"), m_character->wisdom);
    m_baseAbilityScores.insert(QStringLiteral("Харизма"), m_character->charisma);
}

bool CharacterCreationService::applyRaceSelection(const Race &race)
{
    if (!m_character || !m_parentWidget) {
        return false;
    }

    resetClassSelection();
    applyBaseAbilityScores();

    Race resolvedRace = race;
    if (!applyRaceTraitChoices(m_parentWidget, race, m_character->level, &resolvedRace)) {
        return false;
    }

    m_character->setRace(resolvedRace.name);
    m_character->size = resolvedRace.size;
    m_character->speed = resolvedRace.speed;
    m_character->flyingSpeed = resolvedRace.flyingSpeed;

    QStringList resolvedRaceLanguages;
    if (!resolveChosenLanguages(raceLanguageEntriesForSelection(resolvedRace), resolvedRace.name, {}, &resolvedRaceLanguages)) {
        return false;
    }

    if (!applyRaceAbilityBonuses(resolvedRace)) {
        return false;
    }

    m_character->languages = resolvedRaceLanguages;

    if (!chooseRaceGrantedSpells(resolvedRace)) {
        return false;
    }

    m_character->traits = filteredRaceTraits(resolvedRace.traits);
    const QStringList baseSkills = m_character->skillProficiencies;
    const QStringList baseTools = m_character->toolProficiencies;
    const QStringList baseArmor = m_character->armorProficiencies;
    const QStringList baseWeapons = m_character->weaponProficiencies;
    applyRaceDerivedBenefits(resolvedRace);

    if (!applyRaceChoiceBenefits(m_parentWidget, resolvedRace, m_character, baseSkills, baseTools, baseArmor, baseWeapons)) {
        m_character->skillProficiencies = baseSkills;
        m_character->toolProficiencies = baseTools;
        m_character->armorProficiencies = baseArmor;
        m_character->weaponProficiencies = baseWeapons;
        return false;
    }

    m_character->recalculateDerivedStats(false);
    return true;
}

bool CharacterCreationService::applyClassSelection(const Class &cls, int levelsToAdd)
{
    if (!m_character || levelsToAdd <= 0) {
        return false;
    }

    m_targetLevel = qMax(m_targetLevel, levelsToAdd);
    if (m_character) {
        m_character->level = m_targetLevel;
    }

    if (!applyClassLevelChange(cls, levelsToAdd)) {
        return false;
    }

    chooseStartingSpells();
    m_character->recalculateDerivedStats(false);
    return true;
}

bool CharacterCreationService::configureSpells()
{
    if (!m_character) {
        return false;
    }
    chooseStartingSpells();
    m_character->recalculateDerivedStats(false);
    return true;
}

bool CharacterCreationService::showRacePicker(Race *selectedRace)
{
    if (!m_parentWidget || !selectedRace) {
        return false;
    }

    QDialog dialog(m_parentWidget);
    dialog.setWindowTitle(QStringLiteral("Выбор расы"));
    dialog.resize(980, 720);

    auto *layout = new QVBoxLayout(&dialog);
    auto *page = new RaceSelectionPage(&dialog);
    layout->addWidget(page);
    page->showList();

    Race chosen;
    bool accepted = false;
    QObject::connect(page, &RaceSelectionPage::raceChosen, &dialog, [&](const Race &race) {
        chosen = race;
        accepted = true;
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted || !accepted) {
        return false;
    }

    *selectedRace = chosen;
    return true;
}

bool CharacterCreationService::showClassPicker(Class *selectedClass)
{
    if (!m_parentWidget || !selectedClass) {
        return false;
    }

    QDialog dialog(m_parentWidget);
    dialog.setWindowTitle(QStringLiteral("Выбор класса"));
    dialog.resize(980, 720);

    auto *layout = new QVBoxLayout(&dialog);
    auto *page = new ClassSelectionPage(&dialog);
    layout->addWidget(page);
    page->clearClassFilters();
    page->showList();

    Class chosen;
    bool accepted = false;
    QObject::connect(page, &ClassSelectionPage::classChosen, &dialog, [&](const Class &cls) {
        chosen = cls;
        accepted = true;
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted || !accepted) {
        return false;
    }

    *selectedClass = chosen;
    return true;
}
'''

out = header + helpers1 + helpers2 + footer + "".join(methods)
(root / "charactercreationservice.cpp").write_text(out, encoding="utf-8")
print(f"Wrote {root / 'charactercreationservice.cpp'} ({len(out)} chars)")
