#ifndef NOTE_H
#define NOTE_H

#include <QString>
#include <QDateTime>
#include <QTextDocument>

class Note {
public:
    Note();
    Note(const QString &titleValue, const QString &htmlContent = "");
    
    QString id;
    QString title;
    QString htmlContent;  // Хранит HTML форматированный текст
    QString plainText;    // Простой текст для поиска
    QDateTime created;
    QDateTime modified;
};

#endif // NOTE_H
