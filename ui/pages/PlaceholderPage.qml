import QtQuick
import Faraday

Item {
    property string title: ""
    property string note: qsTr("This section arrives in a later phase.")

    Column {
        anchors.centerIn: parent
        spacing: 8
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: title
            color: Theme.text
            font.pixelSize: 22
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: note
            color: Theme.textDim
            font.pixelSize: 14
        }
    }
}
