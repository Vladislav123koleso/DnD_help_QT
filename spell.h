#ifndef SPELL_H
#define SPELL_H

#include <QString>

class Spell {
public:
    Spell();
    QString name;
    int level;
    QString school;
    bool ritual;
    QString castingTime;
    QString range;
    QString components;
    QString duration;
    QString description;
    QString classes;
    QString subclasses;
    QString selectionClass;
    QString source;
    QString upper; // Накладывание более высокой ячейкой
};

#endif // SPELL_H
