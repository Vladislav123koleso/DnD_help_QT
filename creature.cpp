#include "creature.h"
#include <QJsonDocument>

Creature::Creature() : 
    id(0), str(10), dex(10), con(10), inte(10), wis(10), cha(10)
{
}

QList<CreatureAction> Creature::parseActions(const QString& jsonStr)
{
    QList<CreatureAction> result;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const auto& val : arr) {
            if (val.isObject()) {
                QJsonObject obj = val.toObject();
                result.append({obj["name"].toString(), obj["text"].toString()});
            }
        }
    }
    return result;
}

QString Creature::actionsToJson(const QList<CreatureAction>& list)
{
    QJsonArray arr;
    for (const auto& action : list) {
        QJsonObject obj;
        obj["name"] = action.name;
        obj["text"] = action.text;
        arr.append(obj);
    }
    return QJsonDocument(arr).toJson(QJsonDocument::Compact);
}
