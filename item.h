#ifndef ITEM_H
#define ITEM_H

#include <QString>

class Item {
public:
    Item();
    int id; // DB ID
    QString name;
    QString nameEng;
    QString type;
    QString rarity;
    QString cost;
    QString weight;
    QString source;
    QString description;
    
    // Для инвентаря
    int quantity;
    bool isEquipped;
};

#endif // ITEM_H
