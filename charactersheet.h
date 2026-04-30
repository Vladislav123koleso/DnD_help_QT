#ifndef CHARACTERSHEET_H
#define CHARACTERSHEET_H

#include <QMap>
#include <QWidget>
#include "character.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QTabWidget;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QWidget;
QT_END_NAMESPACE

namespace Ui {
class CharacterSheet;
}

class CharacterSheet : public QWidget
{
    Q_OBJECT

public:
    explicit CharacterSheet(QWidget *parent = nullptr);
    ~CharacterSheet();

    void setCharacter(Character *c);

signals:
    void characterUpdated();

private slots:
    void onFieldChanged();
    void onSpellFiltersChanged();
    void onSpellItemChanged(QTreeWidgetItem *item, int column);
    void onSpellSelectionChanged();
    void onFeatureSelectionChanged();

private:
    Ui::CharacterSheet *ui;
    Character *m_character = nullptr;
    bool m_updatingUi = false;
    bool m_updatingSpellTree = false;
    QList<Spell> m_allSpells;

    QTabWidget *detailsTabs = nullptr;
    QTextEdit *historyEdit = nullptr;

    QLabel *spellcastingSummaryLabel = nullptr;
    QComboBox *spellClassFilter = nullptr;
    QComboBox *spellLevelFilter = nullptr;
    QComboBox *spellStateFilter = nullptr;
    QLineEdit *spellSearchEdit = nullptr;
    QWidget *spellSlotsContainer = nullptr;
    QVBoxLayout *spellSlotsLayout = nullptr;
    QTreeWidget *spellTree = nullptr;
    QLabel *spellDetailsTitleLabel = nullptr;
    QLabel *spellDetailsMetaLabel = nullptr;
    QTextEdit *spellDetailsText = nullptr;

    QTreeWidget *featuresTree = nullptr;
    QTextEdit *featureDetailsText = nullptr;
    QLabel *languagesValueLabel = nullptr;
    QLabel *skillsValueLabel = nullptr;
    QLabel *toolsValueLabel = nullptr;
    QLabel *savesValueLabel = nullptr;
    QLabel *armorValueLabel = nullptr;
    QLabel *weaponsValueLabel = nullptr;
    QLabel *proficiencyBonusValueLabel = nullptr;
    QMap<QString, QLabel*> proficiencyIndicatorLabels;
    QMap<QString, QLabel*> proficiencyValueLabels;
    QListWidget *inventoryList = nullptr;
    QListWidget *attacksList = nullptr;

    void setupExtendedSections();
    void clearExtendedSections();
    void updateFromCharacter();
    void updateNarrativeSection();
    void updateSpellcastingSection();
    void updateFeatureSection();
    void updateOverviewPanel();
    void rebuildSpellSlotEditors();
    void rebuildSpellTree();
    void rebuildFeatureTree();
    QStringList spellcastingClasses() const;
    QMap<int, int> spellSlotMaximumsForClass(const QString &className) const;
    QString spellSlotKey(const QString &className, int slotLevel) const;
    int spellSlotCurrentValue(const QString &className, int slotLevel, int maximum) const;
    bool canCastSpellForClass(const QString &className, int spellLevel) const;
    QList<Spell> knownSpellsForClass(const QString &className) const;
    QList<Spell> spellbookForClass(const QString &className) const;
    QList<Spell> availablePreparedSpellsForClass(const QString &className, int classLevel, int maxSpellLevel) const;
    Spell findSpellForClass(const QString &className, const QString &spellName, int spellLevel) const;
    bool setSpellPreparedState(const QString &className, const Spell &spell, bool prepared);
    int preparedSpellLimitForClass(const QString &className) const;
    void updateSpellDetails(QTreeWidgetItem *item);
    void updateFeatureDetails(QTreeWidgetItem *item);
};

#endif // CHARACTERSHEET_H