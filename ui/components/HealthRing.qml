import QtQuick
import Faraday

// Large color-coded health ring. `value` is 0..100 (or negative = unknown).
Item {
    id: root

    property real value: -1
    property color ringColor: Theme.textDim
    property string label: qsTr("HEALTH")
    property string sublabel: ""

    // Animated presentation value
    property real shownValue: Math.max(0, value)
    Behavior on shownValue {
        NumberAnimation { duration: 700; easing.type: Easing.OutCubic }
    }

    implicitWidth: 220
    implicitHeight: 220

    onValueChanged: canvas.requestPaint()
    onShownValueChanged: canvas.requestPaint()
    onRingColorChanged: canvas.requestPaint()

    Connections {
        target: Theme
        function onDarkChanged() { canvas.requestPaint() }
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            const w = width, h = height
            ctx.reset()
            const cx = w / 2, cy = h / 2
            const radius = Math.min(w, h) / 2 - 14
            const lineW = Math.max(10, radius * 0.16)
            const start = -Math.PI * 0.5

            // Track
            ctx.beginPath()
            ctx.lineWidth = lineW
            ctx.strokeStyle = Theme.chartGrid
            ctx.arc(cx, cy, radius, 0, Math.PI * 2)
            ctx.stroke()

            // Value arc
            if (root.value >= 0) {
                const frac = Math.min(1, root.shownValue / 100)
                ctx.beginPath()
                ctx.lineWidth = lineW
                ctx.lineCap = "round"
                ctx.strokeStyle = root.ringColor
                ctx.arc(cx, cy, radius, start, start + frac * Math.PI * 2)
                ctx.stroke()
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 2

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.value >= 0 ? Math.round(root.shownValue) + "%" : "—"
            color: root.value >= 0 ? root.ringColor : Theme.textDim
            font.pixelSize: 44
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label
            color: Theme.textDim
            font.pixelSize: 13
            font.letterSpacing: 2
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.sublabel.length > 0
            text: root.sublabel
            color: root.ringColor
            font.pixelSize: 14
            font.bold: true
        }
    }
}
