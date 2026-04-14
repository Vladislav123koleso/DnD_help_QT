/*
    This is a UI file (.ui.qml) that is intended to be edited in Qt Design Studio only.
    It is supposed to be strictly declarative and only uses a subset of QML. If you edit
    this file manually, you might introduce QML code that is not supported by Qt Design Studio.
    Check out https://doc.qt.io/qtcreator/creator-quick-ui-forms.html for details on .ui.qml files.
    */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DndHelperDesign

Rectangle {
    id: root
    width: Constants.width
    height: Constants.height
    color: "#f1eabd"

    property int racesPerPage: 10 // Can be adjusted based on cell size and window width
    property int currentPage: 0
    property int totalPages: Math.ceil(racesModel.count / racesPerPage)

    ListModel {
        id: racesModel
        ListElement { name: "Человек"; normalImage: "images/race/human.jpg"; hoverImage: "images/race/human2.jpg"; hoverText: "Человек - универсальная раса" }
        ListElement { name: "Эльф"; normalImage: "images/race/elf.jpg"; hoverImage: "images/race/elf2.jpg"; hoverText: "Изящный и мудрый эльф" }
        ListElement { name: "Дворф"; normalImage: "images/race/human.jpg"; hoverImage: "images/race/human2.jpg"; hoverText: "Крепкий и выносливый дворф" }
        ListElement { name: "Халфлинг"; normalImage: "images/race/elf.jpg"; hoverImage: "images/race/elf2.jpg"; hoverText: "Маленький и удачливый" }
        ListElement { name: "Драконорожденный"; normalImage: "images/race/human.jpg"; hoverImage: "images/race/human2.jpg"; hoverText: "Потомок драконов" }
        ListElement { name: "Гном"; normalImage: "images/race/elf.jpg"; hoverImage: "images/race/elf2.jpg"; hoverText: "Изобретательный и веселый" }
        ListElement { name: "Полуэльф"; normalImage: "images/race/human.jpg"; hoverImage: "images/race/human2.jpg"; hoverText: "Дитя двух миров" }
        ListElement { name: "Полуорк"; normalImage: "images/race/elf.jpg"; hoverImage: "images/race/elf2.jpg"; hoverText: "Сильный и яростный" }
        ListElement { name: "Тифлинг"; normalImage: "images/race/human.jpg"; hoverImage: "images/race/human2.jpg"; hoverText: "Наследник демонов" }
        ListElement { name: "Аасимар"; normalImage: "images/race/elf.jpg"; hoverImage: "images/race/elf2.jpg"; hoverText: "Потомок ангелов" }
        ListElement { name: "Человек 2"; normalImage: "images/race/human.jpg"; hoverImage: "images/race/human2.jpg"; hoverText: "11" }
        ListElement { name: "Эльф 2"; normalImage: "images/race/elf.jpg"; hoverImage: "images/race/elf2.jpg"; hoverText: "12" }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        Text {
            id: titleText
            text: qsTr("Создание Персонажа")
            font.pixelSize: 48
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            id: subtitleText
            text: qsTr("Выберите расу")
            font.pixelSize: 42
            Layout.alignment: Qt.AlignLeft
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ArrowButton {
                id: backArrowButton
                rotation: 180
                Layout.preferredWidth: 112
                Layout.preferredHeight: 106
                Layout.alignment: Qt.AlignVCenter
                enabled: racesGridView.currentIndex > 0
                onClicked: racesGridView.currentIndex -= racesPerPage
            }

            GridView {
                id: racesGridView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                model: racesModel
                cellWidth: 312
                cellHeight: 430

                // This makes GridView show items page by page
                property int firstVisibleIndex: 0
                currentIndex: 0
                onCurrentIndexChanged: {
                    if (currentIndex < 0) currentIndex = 0;
                    if (currentIndex >= count) currentIndex = Math.floor((count - 1) / racesPerPage) * racesPerPage;
                }

                delegate: ColumnLayout {
                    width: GridView.cellWidth
                    height: GridView.cellHeight
                    spacing: 5

                    // Make item invisible if it's not on the current page
                    visible: index >= racesGridView.currentIndex && index < racesGridView.currentIndex + racesPerPage

                    RasaButtonAnimation {
                        Layout.alignment: Qt.AlignHCenter
                        height: 357
                        width: 212
                        normalImage: model.normalImage
                        hoverImage: model.hoverImage || model.normalImage
                        buttonText: model.hoverText
                    }

                    Text {
                        text: model.name
                        font.pointSize: 18
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            ArrowButton {
                id: nextArrowButton
                Layout.preferredWidth: 112
                Layout.preferredHeight: 106
                Layout.alignment: Qt.AlignVCenter
                enabled: racesGridView.currentIndex + racesPerPage < racesModel.count
                onClicked: racesGridView.currentIndex += racesPerPage
            }
        }
    }
}
