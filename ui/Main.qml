import QtQuick

Window {
    id: root
    width: 1100
    height: 720
    visible: true
    title: qsTr("Faraday — Battery Intelligence Suite")
    color: "#0d1117"

    Text {
        anchors.centerIn: parent
        text: qsTr("Faraday scaffold — Phase 0")
        color: "#e6edf3"
        font.pixelSize: 24
    }
}
