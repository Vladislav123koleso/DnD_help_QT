import QtQuick
import QtQuick.Controls

Button {
    width: 50
    height: 50
    opacity: enabled ? 1.0 : 0.5

    background: Rectangle {
        color: "transparent"
        radius: width / 2
        border.color: "gray"
        border.width: 2
        rotation: 90.0

        Image {
            anchors.centerIn: parent
            source: "images/ui/arrowButton.png"
            width: 45
            height: 45
            sourceSize: Qt.size(width, height)
        }
    }

    // Эффект при нажатии
    Behavior on scale {
        NumberAnimation {
            duration: 100
        }
    }
    onPressed: scale = 0.9
    onReleased: scale = 1.0
}
