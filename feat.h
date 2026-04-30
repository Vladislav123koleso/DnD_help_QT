#ifndef FEAT_H
#define FEAT_H

#include <QString>
#include <QStringList>

class Feat {
public:
    Feat();

    QString slug;
    QString name;
    QString source;
    QString description;
    QString prerequisite;
    QStringList benefits;
};

#endif // FEAT_H