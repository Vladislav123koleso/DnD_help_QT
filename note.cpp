#include "note.h"

Note::Note() : title(""), content(""), created(QDateTime::currentDateTime()), modified(QDateTime::currentDateTime()) {}
