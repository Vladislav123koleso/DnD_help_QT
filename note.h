#ifndef NOTE_H
#define NOTE_H

#include <QString>
#include <QDateTime>

class Note {
public:
    Note();
    QString title;
    QString content;
    QDateTime created;
    QDateTime modified;
};

#endif // NOTE_H
