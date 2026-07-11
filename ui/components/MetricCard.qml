import QtQuick
import Faraday

Rectangle {
    id: card

    property string title: ""
    property string value: "—"
    property string sub: ""
    property string icon: ""
    property color accent: Theme.accent

    color: Theme.surface
    border.color: Theme.border
    border.width: 1
    radius: Theme.radius
    implicitHeight: 108

    Behavior on color { ColorAnimation { duration: 200 } }

    Rectangle {
        width: 3
        radius: 1.5
        color: card.accent
        anchors { left: parent.left; leftMargin: 12; top: parent.top; topMargin: 14; bottom: parent.bottom; bottomMargin: 14 }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 16
        anchors.leftMargin: 26
        spacing: 4

        Row {
            spacing: 6
            Text {
                text: card.icon
                font.pixelSize: 13
                color: Theme.textDim
                visible: card.icon.length > 0
            }
            Text {
                text: card.title
                color: Theme.textDim
                font.pixelSize: 12
                font.letterSpacing: 1.2
            }
        }
        Text {
            text: card.value
            color: Theme.text
            font.pixelSize: 26
            font.bold: true
            elide: Text.ElideRight
            width: parent.width
        }
        Text {
            text: card.sub
            color: Theme.textDim
            font.pixelSize: 12
            visible: card.sub.length > 0
            elide: Text.ElideRight
            width: parent.width
        }
    }
}
