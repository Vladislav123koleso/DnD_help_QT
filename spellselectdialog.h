#ifndef SPELLSELECTDIALOG_H
#define SPELLSELECTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include "spell.h"

class SpellSelectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SpellSelectDialog(QWidget *parent = nullptr);
    
    QList<Spell> selectedSpells() const;
    
private slots:
    void filterSpells();
    void onSpellSelected();
    void onSpellDoubleClicked(QListWidgetItem *item);
    void addSelectedSpell();
    void removeSelectedSpell();

private:
    // Available spells side
    QLineEdit *searchBar;
    QComboBox *classFilter;
    QComboBox *levelFilter;
    QComboBox *schoolFilter;
    QComboBox *castingTimeFilter;
    QCheckBox *checkV;
    QCheckBox *checkS;
    QCheckBox *checkM;
    QCheckBox *checkConcentration;
    QCheckBox *checkRitual;
    
    QListWidget *availableSpellList;
    QLabel *nameLabel;
    QLabel *infoLabel;
    QTextEdit *descriptionText;
    
    // Selected spells side
    QListWidget *selectedSpellList;
    QPushButton *addBtn;
    QPushButton *removeBtn;
    
    QList<Spell> m_allSpells;
    QList<Spell> m_selectedSpells;
    QList<Spell> m_filteredSpells;

    void loadAllSpells();
    void setupUi();
    void setupFilters();
    void updateSelectedList();
};

#endif // SPELLSELECTDIALOG_H
