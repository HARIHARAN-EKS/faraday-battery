import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Faraday

Item {
    id: page

    // --- No battery / loading states -------------------------------------
    Column {
        anchors.centerIn: parent
        spacing: 12
        visible: !battery.batteryPresent

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: battery.ready ? "🔌" : "⏳"
            font.pixelSize: 56
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: battery.ready ? qsTr("No battery detected")
                                : qsTr("Reading battery sensors…")
            color: Theme.text
            font.pixelSize: 24
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: battery.ready
            text: qsTr("This appears to be a desktop system or a virtual machine.\nLive monitoring is disabled.")
            color: Theme.textDim
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // --- Dashboard --------------------------------------------------------
    Flickable {
        anchors.fill: parent
        visible: battery.batteryPresent
        contentWidth: width
        contentHeight: content.height + 32
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }

        ColumnLayout {
            id: content
            x: 20
            y: 16
            width: parent.width - 40
            spacing: Theme.spacing

            // Header row: ring + status
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: headerRow.implicitHeight + 36
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
                radius: Theme.radius

                RowLayout {
                    id: headerRow
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 28

                    HealthRing {
                        Layout.preferredWidth: 210
                        Layout.preferredHeight: 210
                        value: battery.healthPercent
                        ringColor: battery.gradeColor
                        sublabel: battery.gradeName
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        RowLayout {
                            spacing: 10
                            Text {
                                text: battery.statusText
                                color: Theme.text
                                font.pixelSize: 28
                                font.bold: true
                            }
                            Rectangle {
                                visible: battery.onAcPower
                                radius: 4
                                color: Qt.alpha(Theme.accent, 0.15)
                                implicitWidth: acText.implicitWidth + 14
                                implicitHeight: acText.implicitHeight + 6
                                Text {
                                    id: acText
                                    anchors.centerIn: parent
                                    text: qsTr("AC")
                                    color: Theme.accent
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                            }
                        }

                        Text {
                            text: battery.chargePercent >= 0
                                  ? qsTr("%1% charged").arg(battery.chargePercent.toFixed(1))
                                  : qsTr("Charge level unknown")
                            color: Theme.textDim
                            font.pixelSize: 16
                        }
                        Text {
                            visible: battery.timeEstimateText.length > 0
                            text: battery.timeEstimateText
                            color: Theme.accent
                            font.pixelSize: 15
                        }

                        // Charge bar
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            height: 10
                            radius: 5
                            color: Theme.chartGrid
                            Rectangle {
                                width: parent.width * Math.max(0, Math.min(1, battery.chargePercent / 100))
                                height: parent.height
                                radius: 5
                                color: battery.charging ? Theme.accent : battery.gradeColor
                                Behavior on width { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            text: battery.verdictHeadline
                            color: Theme.text
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            // Metric card grid (responsive)
            GridLayout {
                id: grid
                Layout.fillWidth: true
                columns: Math.max(1, Math.floor(width / 235))
                columnSpacing: Theme.spacing
                rowSpacing: Theme.spacing

                MetricCard {
                    Layout.fillWidth: true
                    title: qsTr("CAPACITY")
                    icon: "⚡"
                    value: {
                        battery.energyUnit // re-evaluate when the unit changes
                        return battery.remainingCapacitymWh >= 0
                               ? battery.formatEnergy(battery.remainingCapacitymWh) : "—"
                    }
                    sub: {
                        battery.energyUnit
                        return battery.fullChargeCapacitymWh >= 0
                               ? qsTr("of %1 full charge").arg(battery.formatEnergy(battery.fullChargeCapacitymWh))
                               : ""
                    }
                }
                MetricCard {
                    Layout.fillWidth: true
                    title: qsTr("HEALTH")
                    icon: "❤"
                    accent: battery.gradeColor
                    value: battery.healthPercent >= 0 ? battery.healthPercent.toFixed(1) + "%" : "—"
                    sub: {
                        battery.energyUnit
                        return battery.designCapacitymWh >= 0
                               ? qsTr("design %1").arg(battery.formatEnergy(battery.designCapacitymWh))
                               : ""
                    }
                }
                MetricCard {
                    Layout.fillWidth: true
                    title: qsTr("WEAR")
                    icon: "⚙"
                    accent: battery.wearPercent > 20 ? Theme.warn : Theme.good
                    value: battery.wearPercent >= 0 ? battery.wearPercent.toFixed(1) + "%" : "—"
                    sub: qsTr("capacity lost vs new")
                }
                MetricCard {
                    Layout.fillWidth: true
                    title: qsTr("CYCLES")
                    icon: "↻"
                    value: battery.cycleCount >= 0 ? battery.cycleCount.toString() : "—"
                    sub: qsTr("charge cycles used")
                }
                MetricCard {
                    Layout.fillWidth: true
                    title: qsTr("TEMPERATURE")
                    icon: "\u{1F321}"
                    accent: battery.temperatureKnown && battery.temperatureC > 45 ? Theme.bad : Theme.accent
                    value: {
                        battery.temperatureUnit
                        return battery.temperatureKnown
                               ? battery.formatTemperature(battery.temperatureC) : "—"
                    }
                    sub: qsTr("system thermal estimate")
                }
                MetricCard {
                    Layout.fillWidth: true
                    title: qsTr("VOLTAGE")
                    icon: "⚡"
                    value: battery.voltageV >= 0 ? battery.voltageV.toFixed(2) + " V" : "—"
                    sub: qsTr("pack voltage")
                }
                MetricCard {
                    Layout.fillWidth: true
                    title: qsTr("POWER")
                    icon: "→"
                    accent: battery.netPowerW >= 0 ? Theme.good : Theme.warn
                    value: battery.powerKnown ? Math.abs(battery.netPowerW).toFixed(1) + " W" : "—"
                    sub: battery.powerKnown
                         ? (battery.netPowerW > 0 ? qsTr("charging")
                            : battery.netPowerW < 0 ? qsTr("discharging") : qsTr("idle"))
                         : ""
                }
                MetricCard {
                    Layout.fillWidth: true
                    title: qsTr("SAMPLING")
                    icon: "⏱"
                    value: battery.sampleIntervalSec + " s"
                    sub: qsTr("live sample interval")
                }
            }

            // Verdict details + insights
            Rectangle {
                Layout.fillWidth: true
                visible: battery.verdictDetails.length > 0 || battery.habitInsights.length > 0
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
                radius: Theme.radius
                implicitHeight: verdictCol.implicitHeight + 32

                ColumnLayout {
                    id: verdictCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
                    spacing: 6

                    Text {
                        text: qsTr("ASSESSMENT")
                        color: Theme.textDim
                        font.pixelSize: 12
                        font.letterSpacing: 1.2
                    }
                    Repeater {
                        model: battery.verdictDetails
                        Text {
                            Layout.fillWidth: true
                            text: "•  " + modelData
                            color: Theme.text
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                    }
                    Text {
                        visible: battery.habitInsights.length > 0
                        Layout.topMargin: 8
                        text: qsTr("CHARGING HABITS")
                        color: Theme.textDim
                        font.pixelSize: 12
                        font.letterSpacing: 1.2
                    }
                    Repeater {
                        model: battery.habitInsights
                        Text {
                            Layout.fillWidth: true
                            text: "•  " + modelData
                            color: Theme.text
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            // Multiple batteries
            Repeater {
                model: battery.batteries.length > 1 ? battery.batteries : []
                Rectangle {
                    Layout.fillWidth: true
                    color: Theme.surface
                    border.color: Theme.border
                    border.width: 1
                    radius: Theme.radius
                    implicitHeight: 64
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 16
                        Text {
                            text: qsTr("Pack %1").arg(index + 1) + "  ·  " +
                                  (modelData.name.length ? modelData.name : modelData.instanceName)
                            color: Theme.text
                            font.pixelSize: 14
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: (modelData.remainingmWh !== undefined && modelData.remainingmWh !== null
                                   ? (modelData.remainingmWh / 1000).toFixed(1) : "—") + " / " +
                                  (modelData.fullChargeCapacitymWh !== undefined && modelData.fullChargeCapacitymWh !== null
                                   ? (modelData.fullChargeCapacitymWh / 1000).toFixed(1) : "—") + " Wh"
                            color: Theme.textDim
                            font.pixelSize: 13
                        }
                    }
                }
            }

            // Advanced / detailed drawer
            Rectangle {
                Layout.fillWidth: true
                color: Theme.surface
                border.color: Theme.border
                border.width: 1
                radius: Theme.radius
                implicitHeight: advancedHeader.height + (advancedOpen ? advancedBody.implicitHeight + 16 : 0)
                clip: true

                property bool advancedOpen: false
                id: advancedBox

                Behavior on implicitHeight { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                Item {
                    id: advancedHeader
                    width: parent.width
                    height: 48

                    Text {
                        anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                        text: (advancedBox.advancedOpen ? "▾  " : "▸  ") + qsTr("Advanced — raw sensor streams")
                        color: Theme.text
                        font.pixelSize: 14
                        font.bold: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: advancedBox.advancedOpen = !advancedBox.advancedOpen
                    }
                }

                ColumnLayout {
                    id: advancedBody
                    anchors { left: parent.left; right: parent.right; top: advancedHeader.bottom; margins: 16; topMargin: 0 }
                    spacing: 2
                    visible: advancedBox.advancedOpen

                    Repeater {
                        model: battery.rawStreams
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Text {
                                Layout.preferredWidth: 280
                                text: modelData.source
                                color: Theme.textDim
                                font.pixelSize: 11
                                font.family: "Consolas"
                                elide: Text.ElideMiddle
                            }
                            Text {
                                Layout.preferredWidth: 240
                                text: modelData.field
                                color: Theme.text
                                font.pixelSize: 12
                                font.family: "Consolas"
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.value
                                color: Theme.accent
                                font.pixelSize: 12
                                font.family: "Consolas"
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Text {
                        visible: battery.unavailableSources.length > 0
                        Layout.topMargin: 10
                        text: qsTr("UNAVAILABLE ON THIS SYSTEM")
                        color: Theme.textDim
                        font.pixelSize: 11
                        font.letterSpacing: 1.2
                    }
                    Repeater {
                        model: battery.unavailableSources
                        Text {
                            Layout.fillWidth: true
                            text: "· " + modelData
                            color: Theme.warn
                            font.pixelSize: 11
                            font.family: "Consolas"
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }
        }
    }
}
