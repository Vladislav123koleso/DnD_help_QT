import QtQuick
import QtQuick.Controls

Item {
    GridView {
        id: racesGridView
        //width: 1486
        width: parent.width - nextArrowButton.width - backArrowButton.width - 2 * parent.spacing
        height: parent.height
        anchors.centerIn: parent
        clip: true
        interactive: false
        visible: true

        model: ListModel {
            id: racesModel
            ListElement {
                name: "Человек"
                normalImage: "images/race/human.jpg"
                hoverImage: "images/race/human2.jpg"
                hoverText: "Человек - ебейшая раса"
            }
            ListElement {
                name: "Эльф"
                normalImage: "images/race/elf.jpg"
                hoverImage: "images/race/elf2.jpg"
                hoverText: "Секси Эльф"
            }
            ListElement {
                name: "Человек"
                normalImage: "images/race/human.jpg"
                hoverImage: "images/race/human2.jpg"
                hoverText: "Человек - ебейшая раса"
            }
            ListElement {
                name: "Эльф"
                normalImage: "images/race/elf.jpg"
                hoverImage: "images/race/elf2.jpg"
                hoverText: "Секси Эльф"
            }
            ListElement {
                name: "Человек"
                normalImage: "images/race/human.jpg"
                hoverImage: "images/race/human2.jpg"
                hoverText: "Человек - ебейшая раса"
            }
            ListElement {
                name: "Эльф"
                normalImage: "images/race/elf.jpg"
                hoverImage: "images/race/elf2.jpg"
                hoverText: "Секси Эльф"
            }
            ListElement {
                name: "Человек"
                normalImage: "images/race/human.jpg"
                hoverImage: "images/race/human2.jpg"
                hoverText: "Человек - ебейшая раса"
            }
            ListElement {
                name: "Эльф"
                normalImage: "images/race/elf.jpg"
                hoverImage: "images/race/elf2.jpg"
                hoverText: "Секси Эльф"
            }
            ListElement {
                name: "Человек"
                normalImage: "images/race/human.jpg"
                hoverImage: "images/race/human2.jpg"
                hoverText: "Человек - ебейшая раса"
            }
            ListElement {
                name: "Эльф"
                normalImage: "images/race/elf.jpg"
                hoverImage: "images/race/elf2.jpg"
                hoverText: "Секси Эльф"
            }
            ListElement {
                name: "Человек"
                normalImage: "images/race/human.jpg"
                hoverImage: "images/race/human2.jpg"
                hoverText: "11"
            }
            ListElement {
                name: "Эльф"
                normalImage: "images/race/elf.jpg"
                hoverImage: "images/race/elf2.jpg"
                hoverText: "12"
            }
        }
        delegate: Item {
            x: 5
            height: 50

            Column {
                spacing: 5
                RasaButtonAnimation {
                    id: btn1
                    height: 357
                    width: 212
                    x: 6
                    y: 5
                    normalImage: model.normalImage
                    hoverImage: model.hoverImage || model.normalImage
                    buttonText: model.hoverText
                }

                Text {
                    x: 5
                    text: name
                    font.pointSize: 18
                    font.bold: true

                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
        cellWidth: 312
        cellHeight: 430
    }
}
