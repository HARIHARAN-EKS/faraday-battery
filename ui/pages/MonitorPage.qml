import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Faraday

Item {
    id: page

    // Visible time window in seconds; 0 = whole session.
    property real windowSec: 0
    property var trend: ({ valid: false })

    function refreshTrend() { trend = battery.liveTrend() }

    Connections {
        target: battery
        function onLiveSeriesChanged() {
            page.refreshTrend()
            chart.requestPaint()
        }
        function onReportChanged() { chart.requestPaint() }
    }
    Connections {
        target: Theme
        function onDarkChanged() { chart.requestPaint() }
    }
    onWindowSecChanged: chart.requestPaint()
    Component.onCompleted: refreshTrend()

    Column {
        anchors.centerIn: parent
        spacing: 10
        visible: !battery.batteryPresent
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "🔌"
            font.pixelSize: 48
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("No battery detected — live monitoring is disabled.")
            color: Theme.textDim
            font.pixelSize: 15
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: Theme.spacing
        visible: battery.batteryPresent

        // ---- Controls ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: qsTr("Live monitor")
                color: Theme.text
                font.pixelSize: 20
                font.bold: true
            }
            Item { Layout.fillWidth: true }

            Text { text: qsTr("Interval"); color: Theme.textDim; font.pixelSize: 12 }
            ComboBox {
                id: intervalBox
                model: [1, 2, 5, 10, 30, 60, 120, 300]
                implicitWidth: 90
                Component.onCompleted: {
                    const idx = model.indexOf(battery.sampleIntervalSec)
                    currentIndex = idx >= 0 ? idx : 4
                }
                onActivated: battery.sampleIntervalSec = model[currentIndex]
                displayText: currentText + " s"
            }

            Text { text: qsTr("Zoom"); color: Theme.textDim; font.pixelSize: 12 }
            ComboBox {
                id: zoomBox
                implicitWidth: 110
                textRole: "label"
                valueRole: "sec"
                model: [
                    { label: qsTr("All"), sec: 0 },
                    { label: qsTr("5 min"), sec: 300 },
                    { label: qsTr("15 min"), sec: 900 },
                    { label: qsTr("1 hour"), sec: 3600 },
                    { label: qsTr("4 hours"), sec: 14400 }
                ]
                onActivated: page.windowSec = currentValue
            }

            Button {
                text: qsTr("Sample now")
                onClicked: battery.sampleNow()
            }
            Button {
                text: qsTr("Reset origin")
                onClicked: battery.resetSessionOrigin()
            }
        }

        // ---- Stats strip ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 24

            Repeater {
                model: [
                    { label: qsTr("STATE"), value: battery.statusText },
                    { label: qsTr("POWER"),
                      value: battery.powerKnown ? Math.abs(battery.netPowerW).toFixed(1) + " W" : "—" },
                    { label: qsTr("TREND"),
                      value: page.trend.valid
                             ? (page.trend.slopePctPerHour >= 0 ? "+" : "")
                               + page.trend.slopePctPerHour.toFixed(1) + " %/h"
                             : "—" },
                    { label: qsTr("EXPECTED DRAIN"),
                      value: battery.expectedDrainW > 0
                             ? battery.expectedDrainW.toFixed(1) + " W" : "—" },
                    { label: qsTr("ESTIMATE"),
                      value: battery.timeEstimateText.length ? battery.timeEstimateText : "—" }
                ]
                Column {
                    spacing: 2
                    Text {
                        text: modelData.label
                        color: Theme.textDim
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                    }
                    Text {
                        text: modelData.value
                        color: Theme.text
                        font.pixelSize: 15
                        font.bold: true
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        // ---- Chart ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            radius: Theme.radius

            Canvas {
                id: chart
                anchors.fill: parent
                anchors.margins: 10
                antialiasing: true

                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()

                function fmtTime(sec) {
                    sec = Math.max(0, Math.round(sec))
                    const h = Math.floor(sec / 3600)
                    const m = Math.floor((sec % 3600) / 60)
                    const s = sec % 60
                    if (h > 0) return h + ":" + String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0")
                    return m + ":" + String(s).padStart(2, "0")
                }

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const W = width, H = height
                    const padL = 44, padR = 16, padT = 14, padB = 30
                    const plotW = W - padL - padR
                    const plotH = H - padT - padB
                    if (plotW <= 10 || plotH <= 10)
                        return

                    const all = battery.liveSeries
                    // Time range
                    let tMax = 60
                    if (all.length > 0)
                        tMax = Math.max(60, all[all.length - 1].t)
                    let tMin = 0
                    if (page.windowSec > 0)
                        tMin = Math.max(0, tMax - page.windowSec)

                    const xOf = t => padL + (t - tMin) / Math.max(1, tMax - tMin) * plotW
                    const yOf = p => padT + (100 - p) / 100 * plotH

                    // Grid
                    ctx.strokeStyle = Theme.chartGrid
                    ctx.fillStyle = Theme.textDim
                    ctx.lineWidth = 1
                    ctx.font = "10px sans-serif"
                    for (let pct = 0; pct <= 100; pct += 25) {
                        const y = yOf(pct)
                        ctx.beginPath()
                        ctx.moveTo(padL, y)
                        ctx.lineTo(W - padR, y)
                        ctx.stroke()
                        ctx.textAlign = "right"
                        ctx.fillText(pct + "%", padL - 6, y + 3)
                    }
                    const ticks = 6
                    ctx.textAlign = "center"
                    for (let i = 0; i <= ticks; i++) {
                        const t = tMin + (tMax - tMin) * i / ticks
                        const x = xOf(t)
                        ctx.beginPath()
                        ctx.moveTo(x, padT)
                        ctx.lineTo(x, H - padB)
                        ctx.stroke()
                        ctx.fillText(fmtTime(t), x, H - padB + 16)
                    }

                    // Visible samples
                    const pts = []
                    for (let i = 0; i < all.length; i++) {
                        const p = all[i]
                        if (p.t >= tMin && p.percent >= 0)
                            pts.push(p)
                    }

                    // Expected-discharge comparison line (from usage history)
                    const expSlope = battery.expectedDischargeSlopePctPerHour
                    if (expSlope < 0 && pts.length > 0) {
                        const p0 = pts[0]
                        ctx.strokeStyle = Theme.warn
                        ctx.setLineDash([3, 5])
                        ctx.lineWidth = 1.4
                        ctx.beginPath()
                        ctx.moveTo(xOf(p0.t), yOf(p0.percent))
                        const tEnd = tMax
                        const yEnd = p0.percent + expSlope * (tEnd - p0.t) / 3600
                        ctx.lineTo(xOf(tEnd), yOf(Math.max(0, yEnd)))
                        ctx.stroke()
                        ctx.setLineDash([])
                    }

                    // Extrapolated trend line
                    if (page.trend.valid) {
                        const slopeSec = page.trend.slopePctPerHour / 3600
                        ctx.strokeStyle = Theme.accent
                        ctx.setLineDash([6, 5])
                        ctx.lineWidth = 1.4
                        ctx.beginPath()
                        ctx.moveTo(xOf(page.trend.lastT), yOf(page.trend.lastY))
                        // Extend to chart edge or to 0 % / 100 %
                        let tEnd = tMax + (tMax - tMin) * 0.25
                        let yEnd = page.trend.lastY + slopeSec * (tEnd - page.trend.lastT)
                        yEnd = Math.max(0, Math.min(100, yEnd))
                        if (Math.abs(slopeSec) > 1e-9)
                            tEnd = page.trend.lastT + (yEnd - page.trend.lastY) / slopeSec
                        ctx.lineTo(xOf(tEnd), yOf(yEnd))
                        ctx.stroke()
                        ctx.setLineDash([])
                    }

                    // Main series
                    if (pts.length > 0) {
                        ctx.strokeStyle = battery.charging ? Theme.good : Theme.accent
                        ctx.lineWidth = 2
                        ctx.beginPath()
                        for (let i = 0; i < pts.length; i++) {
                            const x = xOf(pts[i].t), y = yOf(pts[i].percent)
                            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                        }
                        ctx.stroke()

                        // Critical-discharge markers
                        ctx.fillStyle = Theme.bad
                        for (let i = 0; i < pts.length; i++) {
                            if (pts[i].critical) {
                                ctx.beginPath()
                                ctx.arc(xOf(pts[i].t), yOf(pts[i].percent), 4, 0, Math.PI * 2)
                                ctx.fill()
                            }
                        }

                        // Last point dot
                        const last = pts[pts.length - 1]
                        ctx.fillStyle = battery.charging ? Theme.good : Theme.accent
                        ctx.beginPath()
                        ctx.arc(xOf(last.t), yOf(last.percent), 4.5, 0, Math.PI * 2)
                        ctx.fill()
                    } else {
                        ctx.fillStyle = Theme.textDim
                        ctx.textAlign = "center"
                        ctx.font = "13px sans-serif"
                        ctx.fillText(qsTr("Collecting samples…"), padL + plotW / 2, padT + plotH / 2)
                    }
                }
            }

            // Legend
            Row {
                anchors { top: parent.top; right: parent.right; margins: 14 }
                spacing: 14
                Repeater {
                    model: [
                        { c: battery.charging ? Theme.good : Theme.accent, t: qsTr("Charge level") },
                        { c: Theme.accent, t: qsTr("Trend") },
                        { c: Theme.warn, t: qsTr("Expected discharge") },
                        { c: Theme.bad, t: qsTr("Critical draw") }
                    ]
                    Row {
                        spacing: 5
                        Rectangle {
                            width: 10; height: 3; radius: 1.5
                            color: modelData.c
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: modelData.t
                            color: Theme.textDim
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }
    }
}
