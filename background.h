#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <QMap>
#include <QString>
#include <QStringList>

class Background {
public:
    Background();

    QString slug;
    QString name;
    QString source;
    QString description;

    QStringList skillProficiencies;
    QStringList toolProficiencies;
    QStringList languages;
    QStringList equipment;

    QString featureName;
    QString featureDescription;
    QMap<QString, QString> traits;
};

#endif // BACKGROUND_H