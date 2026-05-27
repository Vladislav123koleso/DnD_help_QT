#ifndef NPCCREATORWIDGET_H
#define NPCCREATORWIDGET_H

#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <QTreeWidget>

#include "npc.h"
#include "character.h"
#include "charactercreationservice.h"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QListWidget;
class QTextEdit;

class NpcCreatorWidget : public QWidget
{
    Q_OBJECT
    friend class NpcTreeWidget;
public:
    explicit NpcCreatorWidget(QWidget *parent = nullptr);
    ~NpcCreatorWidget() override;

    void setStorageScope(const QString &scope);

private slots:
    void createNpc();
    void createFolder();
    void deleteSelectedTreeItem();
    void renameSelectedTreeItem();
    void onTreeSelectionChanged();
    void onTreeStructureChanged();
    void onTreeItemExpanded(QTreeWidgetItem *item);
    void onTreeItemCollapsed(QTreeWidgetItem *item);
    void onFormFieldChanged();
    void addAttack();
    void removeAttack();
    void addTrait();
    void removeTrait();
    void addSpell();
    void removeSpell();
    void addInventoryItem();
    void removeInventoryItem();
    void persistToDisk();
    void runCreationWizard();
    void updateCreationStatusLabel();

private:
    enum class TreeItemKind { Folder, Npc };

    void setupUi();
    void loadFromDisk();
    void schedulePersist();
    QString storageFilePath() const;

    void rebuildTree();
    void syncStructureFromTree();
    void selectNpcById(const QString &npcId);
    void saveCurrentNpcFromForm();
    void loadNpcIntoForm(const NpcEntry &entry);
    NpcEntry readNpcFromForm() const;
    QString currentNpcId() const;
    QTreeWidgetItem *findTreeItemById(const QString &id, TreeItemKind kind) const;
    void showTreeContextMenu(const QPoint &pos);
    void ensureStorageReady();
    void setFormEnabled(bool enabled);
    void updateToolbarState();
    QString targetFolderIdForNewNpc() const;
    int nextSortOrderInFolder(const QString &folderId) const;
    bool listNameExists(const QString &name, const QString &exceptId = {}) const;
    bool folderNameExists(const QString &name, const QString &exceptId = {}) const;
    void ensureWorkCharacter();
    void syncWorkCharacterAbilityScores();
    void applyWorkCharacterToCurrentNpc();
    void recalculateNpcDerivedFields();
    void updateStatblockPreview();
    void applyFormValuesToWorkCharacter();
    void applyDerivedFieldsFromWorkCharacter();
    void updateAbilityModifiersLabel();
    void reloadArmorSourcesFromItems();

    QTreeWidget *npcTree = nullptr;
    QPushButton *addNpcBtn = nullptr;
    QPushButton *addFolderBtn = nullptr;
    QPushButton *deleteBtn = nullptr;
    QWidget *formPanel = nullptr;

    QLineEdit *listNameEdit = nullptr;
    QLineEdit *nameEdit = nullptr;
    QLineEdit *raceEdit = nullptr;
    QLineEdit *classEdit = nullptr;
    QComboBox *sizeCombo = nullptr;
    QLineEdit *typeEdit = nullptr;
    QLineEdit *challengeEdit = nullptr;
    QSpinBox *xpSpin = nullptr;
    QLabel *creationStatusLabel = nullptr;
    QPushButton *runWizardBtn = nullptr;
    QComboBox *alignmentCombo = nullptr;
    QSpinBox *levelSpin = nullptr;

    QSpinBox *strSpin = nullptr;
    QSpinBox *dexSpin = nullptr;
    QSpinBox *conSpin = nullptr;
    QSpinBox *intSpin = nullptr;
    QSpinBox *wisSpin = nullptr;
    QSpinBox *chaSpin = nullptr;
    QLabel *strAbilityLabel = nullptr;
    QLabel *dexAbilityLabel = nullptr;
    QLabel *conAbilityLabel = nullptr;
    QLabel *intAbilityLabel = nullptr;
    QLabel *wisAbilityLabel = nullptr;
    QLabel *chaAbilityLabel = nullptr;

    QSpinBox *acSpin = nullptr;
    QSpinBox *maxHpSpin = nullptr;
    QSpinBox *currentHpSpin = nullptr;
    QSpinBox *speedSpin = nullptr;
    QSpinBox *initiativeSpin = nullptr;
    QSpinBox *pbSpin = nullptr;
    QComboBox *armorSourceCombo = nullptr;
    QComboBox *shieldSourceCombo = nullptr;
    QLineEdit *acTypeEdit = nullptr;
    QLineEdit *hitDiceEdit = nullptr;
    QLineEdit *speedDetailsEdit = nullptr;
    QLineEdit *sensesEdit = nullptr;
    QLineEdit *vulnerabilitiesEdit = nullptr;
    QLineEdit *resistancesEdit = nullptr;
    QLineEdit *immunitiesEdit = nullptr;
    QLineEdit *conditionImmunitiesEdit = nullptr;
    QLabel *derivedSavesLabel = nullptr;
    QLabel *derivedSkillsLabel = nullptr;
    QLabel *passivePerceptionLabel = nullptr;
    QTextEdit *statblockPreview = nullptr;

    QLineEdit *ageEdit = nullptr;
    QLineEdit *heightEdit = nullptr;
    QLineEdit *weightEdit = nullptr;
    QLineEdit *skinEdit = nullptr;
    QLineEdit *hairEdit = nullptr;
    QTextEdit *appearanceEdit = nullptr;

    QTextEdit *descriptionEdit = nullptr;
    QTextEdit *notesEdit = nullptr;
    QPlainTextEdit *languagesEdit = nullptr;
    QPlainTextEdit *skillsEdit = nullptr;
    QPlainTextEdit *savesEdit = nullptr;
    QPlainTextEdit *armorEdit = nullptr;
    QPlainTextEdit *weaponsEdit = nullptr;

    QListWidget *attacksList = nullptr;
    QListWidget *traitsList = nullptr;
    QListWidget *spellsList = nullptr;
    QListWidget *inventoryList = nullptr;

    QMap<QString, NpcEntry> npcs;
    QMap<QString, NpcFolder> folders;
    QString currentNpcIdValue;
    QString m_storageScope;
    bool m_loading = false;
    bool m_recalculating = false;
    bool m_internalAutoUpdate = false;
    int m_lastAutoAc = 10;
    int m_lastAutoMaxHp = 10;
    int m_acManualAdjustment = 0;
    int m_hpManualAdjustment = 0;
    QTimer *saveTimer = nullptr;
    Character *workCharacter = nullptr;
    CharacterCreationService *creationService = nullptr;

    static constexpr int RoleKind = Qt::UserRole;
    static constexpr int RoleId = Qt::UserRole + 1;
};

#endif // NPCCREATORWIDGET_H
