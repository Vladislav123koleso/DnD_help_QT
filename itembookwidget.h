#ifndef ITEMBOOKWIDGET_H
#define ITEMBOOKWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QTextEdit>
#include <QLabel>
#include <QComboBox>
#include "item.h"

class ItemBookWidget : public QWidget
{
    Q_OBJECT
public:
    enum FilterMode {
        AllItems,
        GeneralItems,     // Everything EXCEPT Weapons and Armor
        WeaponsAndArmor   // ONLY Weapons and Armor
    };
    Q_ENUM(FilterMode)

    explicit ItemBookWidget(FilterMode mode = AllItems, QWidget *parent = nullptr);

private slots:
    void filterItems();
    void onItemSelected(QListWidgetItem *item);

private:
    QLineEdit *searchBar;
    QComboBox *typeFilter;
    QComboBox *rarityFilter;
    
    QListWidget *itemList;
    
    // Details View
    QLabel *nameLabel;
    QLabel *typeLabel;
    QLabel *rarityLabel;
    QLabel *costLabel;
    QLabel *weightLabel;
    QTextEdit *descriptionText;

    QList<Item> allItems;
    FilterMode currentMode;
    
    void loadItems();
};

#endif // ITEMBOOKWIDGET_H
