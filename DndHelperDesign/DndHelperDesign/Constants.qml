pragma Singleton
import QtQuick

QtObject {
    property color backgroundColor: "#31363b"
    property int width: 1024
    property int height: 768
    readonly property font defaultFont: Qt.font({
        family: "Roboto",
        pixelSize: 20
    })
}