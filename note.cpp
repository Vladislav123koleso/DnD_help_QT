#include "note.h"
#include <QUuid>

Note::Note() 
    : id(QUuid::createUuid().toString()),
      title(""), 
      htmlContent(""), 
      plainText(""),
      created(QDateTime::currentDateTime()), 
      modified(QDateTime::currentDateTime()) 
{}

Note::Note(const QString &titleValue, const QString &htmlContent)
    : id(QUuid::createUuid().toString()),
      title(titleValue),
      htmlContent(htmlContent),
      plainText(htmlContent),  // Можно очистить от HTML тегов позже
      created(QDateTime::currentDateTime()),
      modified(QDateTime::currentDateTime())
{}
