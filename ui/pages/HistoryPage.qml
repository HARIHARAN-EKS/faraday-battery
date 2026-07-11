import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Faraday

Item {
    id: page

    property real windowDays: 0   // 0 = all
    property var history: []
    property var usage: []
    property var degradation: ({ valid: false })

    function reload() {
        history = battery.capacityHistoryList()
        usage = battery.usageLog(200)
        degradation = battery.degradationInfo()
        capacityChart.requestPaint()
        cycleChart.requestPaint()
    }

    Component.onCompleted: reload()
    Connections {
        target: battery
        function onReportChanged() { page.reload() }
    }
    Connections {
        target: Theme
        function onDarkChanged() { capacityChart.requestPaint(); cycleChart.requestPaint() }
    }
    onWindowDaysChanged: { capacityChart.requestPaint(); cycleChart.requestPaint() }

    function visiblePoints() {
        if (windowDays <= 0 || history.length === 0)
            return history
        const cutoff = Date.now() - windowDays * 86400000
        return history.filter(p => p.dateMs >= cutoff)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: Theme.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: qsTr("History & timeline")
                color: Theme.text
                font.pixelSize: 20
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text {
                visible: page.degradation.valid
                text: page.degradation.valid
                      ? qsTr("Trend: %1 mWh/day").arg(page.degradation.slopeMWhPerDay.toFixed(1))
                        + (page.degradation.eolDate !== undefined
                           ? "  ·  " + qsTr("80% threshold ≈ %1").arg(page.degradation.eolDate)
                           : "")
                      : ""
                color: Theme.textDim
                font.pixelSize: 12
            }
            Text { text: qsTr("Range"); color: Theme.textDim; font.pixelSize: 12 }
            ComboBox {
                textRole: "label"
                valueRole: "days"
                implicitWidth: 120
                model: [
                    { label: qsTr("All"), days: 0 },
                    { label: qsTr("1 month"), days: 30 },
                    { label: qsTr("3 months"), days: 91 },
                    { label: qsTr("6 months"), days: 183 },
                    { label: qsTr("1 year"), days: 365 }
                ]
                onActivated: page.windowDays = currentValue
            }
        }

        // ---- Capacity decline chart ----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(200, parent.height * 0.34)
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            radius: Theme.radius

            Canvas {
                id: capacityChart
                anchors.fill: parent
                anchors.margins: 10
                antialiasing: true
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const pts = page.visiblePoints().filter(p => p.fullChargemWh > 0)
                    const W = width, H = height
                    const padL = 58, padR = 14, padT = 16, padB = 26
                    const plotW = W - padL - padR, plotH = H - padT - padB
                    if (plotW <= 10 || plotH <= 10) return

                    ctx.fillStyle = Theme.textDim
                    ctx.font = "10px sans-serif"
                    if (pts.length === 0) {
                        ctx.textAlign = "center"
                        ctx.font = "13px sans-serif"
                        ctx.fillText(qsTr("No capacity history available yet"), W / 2, H / 2)
                        return
                    }

                    const t0 = pts[0].dateMs
                    const t1 = Math.max(pts[pts.length - 1].dateMs, t0 + 86400000)
                    let yMax = 0
                    for (const p of pts)
                        yMax = Math.max(yMax, p.fullChargemWh, p.designmWh > 0 ? p.designmWh : 0)
                    yMax *= 1.06
                    const yMin = 0

                    const xOf = t => padL + (t - t0) / (t1 - t0) * plotW
                    const yOf = v => padT + (yMax - v) / (yMax - yMin) * plotH

                    // Grid + labels
                    ctx.strokeStyle = Theme.chartGrid
                    ctx.lineWidth = 1
                    for (let i = 0; i <= 4; i++) {
                        const v = yMax * i / 4
                        ctx.beginPath()
                        ctx.moveTo(padL, yOf(v)); ctx.lineTo(W - padR, yOf(v))
                        ctx.stroke()
                        ctx.textAlign = "right"
                        ctx.fillText((v / 1000).toFixed(1) + " Wh", padL - 6, yOf(v) + 3)
                    }
                    ctx.textAlign = "center"
                    for (let i = 0; i <= 4; i++) {
                        const t = t0 + (t1 - t0) * i / 4
                        ctx.fillText(new Date(t).toLocaleDateString(Qt.locale(), "MMM d"),
                                     xOf(t), H - padB + 15)
                    }

                    // Design capacity (dashed)
                    const design = pts.reduce((a, p) => Math.max(a, p.designmWh > 0 ? p.designmWh : 0), 0)
                    if (design > 0) {
                        ctx.strokeStyle = Theme.textDim
                        ctx.setLineDash([4, 5])
                        ctx.beginPath()
                        ctx.moveTo(padL, yOf(design)); ctx.lineTo(W - padR, yOf(design))
                        ctx.stroke()
                        ctx.setLineDash([])
                    }

                    // Full-charge capacity line + dots
                    ctx.strokeStyle = Theme.accent
                    ctx.lineWidth = 2
                    ctx.beginPath()
                    pts.forEach((p, i) => {
                        const x = xOf(p.dateMs), y = yOf(p.fullChargemWh)
                        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                    })
                    ctx.stroke()
                    ctx.fillStyle = Theme.accent
                    for (const p of pts) {
                        ctx.beginPath()
                        ctx.arc(xOf(p.dateMs), yOf(p.fullChargemWh), 3, 0, Math.PI * 2)
                        ctx.fill()
                    }
                }
            }

            Text {
                anchors { top: parent.top; left: parent.left; margins: 12 }
                text: qsTr("CAPACITY DECLINE — full charge vs design")
                color: Theme.textDim
                font.pixelSize: 10
                font.letterSpacing: 1.2
            }
        }

        // ---- Cycle history chart ----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            radius: Theme.radius

            Canvas {
                id: cycleChart
                anchors.fill: parent
                anchors.margins: 10
                antialiasing: true
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const pts = page.visiblePoints().filter(p => p.cycleCount !== undefined
                                                                 && p.cycleCount !== null
                                                                 && p.cycleCount >= 0)
                    const W = width, H = height
                    const padL = 58, padR = 14, padT = 18, padB = 8
                    const plotW = W - padL - padR, plotH = H - padT - padB
                    if (plotW <= 10 || plotH <= 10) return
                    ctx.fillStyle = Theme.textDim
                    ctx.font = "10px sans-serif"
                    if (pts.length === 0) {
                        ctx.textAlign = "center"
                        ctx.fillText(qsTr("No cycle history"), W / 2, H / 2)
                        return
                    }
                    const t0 = pts[0].dateMs
                    const t1 = Math.max(pts[pts.length - 1].dateMs, t0 + 86400000)
                    const cMax = Math.max(1, pts.reduce((a, p) => Math.max(a, p.cycleCount), 0))
                    const xOf = t => padL + (t - t0) / (t1 - t0) * plotW
                    const yOf = c => padT + (cMax - c) / cMax * plotH

                    ctx.textAlign = "right"
                    ctx.fillText(cMax.toString(), padL - 6, yOf(cMax) + 3)
                    ctx.fillText("0", padL - 6, yOf(0) + 3)

                    ctx.strokeStyle = Theme.good
                    ctx.lineWidth = 2
                    ctx.beginPath()
                    pts.forEach((p, i) => {
                        const x = xOf(p.dateMs), y = yOf(p.cycleCount)
                        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                    })
                    ctx.stroke()
                }
            }

            Text {
                anchors { top: parent.top; left: parent.left; margins: 12 }
                text: qsTr("CYCLE COUNT")
                color: Theme.textDim
                font.pixelSize: 10
                font.letterSpacing: 1.2
            }
        }

        // ---- Usage log ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            radius: Theme.radius

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                Text {
                    text: qsTr("RECENT USAGE — stitched from powercfg")
                    color: Theme.textDim
                    font.pixelSize: 10
                    font.letterSpacing: 1.2
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { Layout.preferredWidth: 150; text: qsTr("Time"); color: Theme.textDim; font.pixelSize: 11; font.bold: true }
                    Text { Layout.preferredWidth: 90; text: qsTr("State"); color: Theme.textDim; font.pixelSize: 11; font.bold: true }
                    Text { Layout.preferredWidth: 50; text: qsTr("Source"); color: Theme.textDim; font.pixelSize: 11; font.bold: true }
                    Text { Layout.preferredWidth: 80; text: qsTr("Duration"); color: Theme.textDim; font.pixelSize: 11; font.bold: true }
                    Text { Layout.preferredWidth: 90; text: qsTr("Energy Δ"); color: Theme.textDim; font.pixelSize: 11; font.bold: true }
                    Text { Layout.fillWidth: true; text: qsTr("Level"); color: Theme.textDim; font.pixelSize: 11; font.bold: true }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: page.usage
                    ScrollBar.vertical: ScrollBar { }

                    delegate: Item {
                        width: ListView.view.width
                        height: 26

                        RowLayout {
                            anchors.fill: parent
                            spacing: 8
                            Text {
                                Layout.preferredWidth: 150
                                text: new Date(modelData.tsMs).toLocaleString(Qt.locale(), "MMM d hh:mm:ss")
                                color: Theme.text
                                font.pixelSize: 11
                            }
                            Text {
                                Layout.preferredWidth: 90
                                text: modelData.type
                                color: Theme.text
                                font.pixelSize: 11
                            }
                            Text {
                                Layout.preferredWidth: 50
                                text: modelData.ac ? qsTr("AC") : qsTr("Battery")
                                color: modelData.ac ? Theme.accent : Theme.warn
                                font.pixelSize: 11
                            }
                            Text {
                                Layout.preferredWidth: 80
                                text: battery.formatDuration(modelData.durationSec)
                                color: Theme.textDim
                                font.pixelSize: 11
                            }
                            Text {
                                Layout.preferredWidth: 90
                                text: modelData.deltamWh !== undefined && modelData.deltamWh !== null
                                      ? (modelData.deltamWh >= 0 ? "+" : "") + modelData.deltamWh + " mWh"
                                      : "—"
                                color: modelData.deltamWh >= 0 ? Theme.good : Theme.warn
                                font.pixelSize: 11
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.percent >= 0 ? modelData.percent.toFixed(0) + "%" : "—"
                                color: Theme.text
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
    }
}
