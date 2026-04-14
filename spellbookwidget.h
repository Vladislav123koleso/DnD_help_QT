#ifndef SPELLBOOKWIDGET_H
#define SPELLBOOKWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include "spell.h"

class SpellBookWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SpellBookWidget(QWidget *parent = nullptr);

private slots:
    void filterSpells();
    void onSpellSelected(QListWidgetItem *item);

private:
    // Filters
    QLineEdit *searchBar;
    QComboBox *classFilter;
    QComboBox *levelFilter;
    QComboBox *schoolFilter; // Use as damage type proxy or just school? User asked for damage type.
    QComboBox *subclassFilter; // Dropdown for subclass
    QComboBox *castingTimeFilter; // Action types
    QCheckBox *checkV;
    QCheckBox *checkS;
    QCheckBox *checkM;
    QCheckBox *checkConcentration;
    QCheckBox *checkRitual;

    // Content
    QListWidget *spellList;
    
    // Details
    QLabel *nameLabel;
    QLabel *infoLabel; // Level, School
    QTextEdit *descriptionText;
    
    QList<Spell> m_allSpells;

    void loadAllSpells();
    void setupUi();
};

#endif // SPELLBOOKWIDGET_H
