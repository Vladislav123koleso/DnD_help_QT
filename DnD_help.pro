QT += quick qml widgets sql
CONFIG += c++17

#
QML_IMPORT_PATH =

RESOURCES += qml.qrc


QML_DESIGNER_IMPORT_PATH =


QMAKE_CXXFLAGS_DEBUG += -g
QMAKE_LFLAGS_DEBUG += -g

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

SOURCES += \
    character.cpp \
    main.cpp \
    mainwindow.cpp \
    startpage.cpp \
    playerpage.cpp \
    masterpage.cpp \
    spell.cpp \
    item.cpp \
    race.cpp \
    class.cpp \
    note.cpp \
    databasemanager.cpp \
    spellbookwidget.cpp \
    itembookwidget.cpp \
    creature.cpp \
    bestiarywidget.cpp \
    race_selection_page.cpp \
    racedetailspage.cpp \
    class_selection_page.cpp \
    classdetailspage.cpp \
    flowlayout.cpp \
    racecard.cpp \
    classcard.cpp \
    charactersheet.cpp

HEADERS += \
    character.h \
    mainwindow.h \
    startpage.h \
    playerpage.h \
    masterpage.h \
    spell.h \
    item.h \
    race.h \
    class.h \
    note.h \
    databasemanager.h \
    spellbookwidget.h \
    itembookwidget.h \
    creature.h \
    bestiarywidget.h \
    race_selection_page.h \
    racedetailspage.h \
    class_selection_page.h \
    classdetailspage.h \
    flowlayout.h \
    racecard.h \
    classcard.h \
    charactersheet.h


FORMS += \
    mainwindow.ui \
    racecard.ui \
    classcard.ui \
    charactersheet.ui

# Add the qmldir file to DISTFILES so it's included in the package
DISTFILES += \
    DndHelperDesign/DndHelperDesign/qmldir

# Correctly define the resources
OTHER_FILES += \
    *.json \
    data/*.json \
    scripts/*.json \
    DndHelperDesign/DndHelperDesignContent/*.qml \
    DndHelperDesign/DndHelperDesignContent/components/*.qml \
    DndHelperDesign/DndHelperDesignContent/fonts/fonts.txt \
    DndHelperDesign/DndHelperDesignContent/images/class/* \
    DndHelperDesign/DndHelperDesignContent/images/race/* \
    DndHelperDesign/DndHelperDesignContent/images/ui/* \
    DndHelperDesign/DndHelperDesign/*.qml \
    DndHelperDesign/DndHelperDesign/GraphicalEffects/*.qml \
    DndHelperDesign/DndHelperDesign/GraphicalEffects/private/*.qml
