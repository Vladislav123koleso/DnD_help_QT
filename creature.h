#ifndef CREATURE_H
#define CREATURE_H

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>

struct CreatureAction {
    QString name;
    QString text;
};

class Creature {
public:
    Creature();
    
    int id; // DB ID
    QString name;
    QString nameEng;
    QString type;
    QString source;
    
    // Combat Stats
    QString ac;
    QString hp;
    QString speed;
    
    // Ability Scores
    int str;
    int dex;
    int con;
    int inte; // int is keyword
    int wis;
    int cha;
    
    // Info
    QString senses;
    QString languages;
    QString challenge;
    
    // Complex fields (stored as JSON string in DB, parsed to lists here)
    QList<CreatureAction> traits;
    QList<CreatureAction> actions;
    QList<CreatureAction> legendaryActions;
    
    QString description; // HTML or Text
    
    // Helper to serialize/deserialize
    static QList<CreatureAction> parseActions(const QString& jsonStr);
    static QString actionsToJson(const QList<CreatureAction>& list);
};

#endif // CREATURE_H
