import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id: btn
    width: 212
    height: 357
        property string buttonText: "человек - ебейшая раса"

       // Публичные свойства
       property alias image: btnImage.source
       property alias text: hoverText.text

       // Внутренние свойства для состояний
       property bool isHovered: false
       property bool isPressed: false
       property bool toggled: false

        property string normalImage: "images/Minerva.jpg"
       property string hoverImage: "images/Minerva2.jpg"
       //property string activeImage: ""

        property real borderRadius: 30

       Image {
           id: btnImage
           anchors.fill: parent
           source: btn.toggled ? btn.hoverImage :
                  (isHovered ? btn.hoverImage : btn.normalImage)


           fillMode: Image.PreserveAspectCrop
                       layer.enabled: true
                       layer.effect: OpacityMask  {
                           maskSource: Rectangle {
                               width: btnImage.width
                               height: btnImage.height
                               radius: borderRadius - 1
                           }
                       }
       }

       Text {
           id: hoverText
           width: 212
           height: 150
           anchors.centerIn: parent
           anchors.verticalCenterOffset: 103
           text: btn.buttonText
           horizontalAlignment: Text.AlignHCenter
           verticalAlignment: Text.AlignBottom
           wrapMode: Text.Wrap
           font.pointSize: 18
           color: "#ffffff"
           opacity: isHovered ? 1 : 0

           // Анимация изменения прозрачности
           Behavior on opacity {
               NumberAnimation { duration: 200 }
           }
       }

       Rectangle {
           id: frame
           anchors.fill: parent
           color: "transparent"
           border.color: "#FFD700"
           border.width: btn.toggled ? 3 : 0
           radius: borderRadius

           // Анимация изменения рамки
           Behavior on border.width {
               NumberAnimation { duration: 100 }
           }
       }

       MouseArea {
           id: mouseArea
           anchors.fill: parent
           hoverEnabled: true

           onEntered: btn.isHovered = true
           onExited: btn.isHovered = false
           onPressed: btn.isPressed = true
           onReleased: btn.isPressed = false
           onClicked: {
               btn.toggled = !btn.toggled  // Переключаем состояние при каждом клике
                console.log("Кнопка", btn.toggled ? "активна" : "неактивна")
           }
       }
       // Сбрасываем состояние нажатости кнопки при получении сигнала от другой кнопки
        function reset() {
            btn.toggled = false
        }
}
