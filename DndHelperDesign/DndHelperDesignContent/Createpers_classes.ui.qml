

/*
This is a UI file (.ui.qml) that is intended to be edited in Qt Design Studio only.
It is supposed to be strictly declarative and only uses a subset of QML. If you edit
this file manually, you might introduce QML code that is not supported by Qt Design Studio.
Check out https://doc.qt.io/qtcreator/creator-quick-ui-forms.html for details on .ui.qml files.
*/
import QtQuick
import QtQuick.Controls
import DndHelperDesign

Rectangle {
    id: root
    width: Constants.width
    height: Constants.height
    layer.enabled: false

    Rectangle {
        id: rectangle
        x: 0
        y: 0
        width: Constants.width
        height: Constants.height
        color: "#f1eabd"
        anchors.verticalCenter: parent.verticalCenter
        Text {
            id: _text
            x: 659
            y: 0
            width: 480
            height: 63
            text: qsTr("Создание Персонажа")
            font.pixelSize: 48
            activeFocusOnTab: false
            layer.enabled: false
            layer.smooth: false
            //anchors.verticalCenter: 480
            //anchors.horizontalCenter: 63
        }

        Text {
            id: _text1
            x: 79
            y: 95
            width: 480
            height: 63
            text: qsTr("Выберите класс")
            font.pixelSize: 42
        }
    }

    GridView {
        id: gridView3
        x: 76
        y: 186
        width: 1786
        height: 720
        visible: true
        model: ListModel {
            ListElement {
                name: "Воин"
                normalImage: "images/class/.jpg"
                hoverImage: "images/class/2.jpg"
                hoverText: ""
            }
            ListElement {
                name: "Маг"
                normalImage: "images/class/.jpg"
                hoverImage: "images/class/2.jpg"
                hoverText: ""
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
                // AnimatedImage {
                //     id: animatedImage
                //     width: 280
                //     height: 280
                //     source: "qrc:/qtquickplugin/images/template_image.png"
                // }
                Text {
                    x: 5
                    text: name
                    font.pointSize: 18
                    font.bold: true

                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
        cellWidth: 350

        cellHeight: 350
    }
}
