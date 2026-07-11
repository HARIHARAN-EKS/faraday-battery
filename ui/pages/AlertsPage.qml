import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Faraday

Item {
    id: page

    property string chargeCapMessage: ""

    component SectionCard: Rectangle {
        default property alias content: inner.data
        property string heading: ""
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        radius: Theme.radius
        implicitHeight: inner.implicitHeight + 46

        Text {
            id: headingText
            anchors { top: parent.top; left: parent.left; margins: 14 }
            text: parent.heading
            color: Theme.textDim
            font.pixelSize: 11
            font.letterSpacing: 1.2
        }
        ColumnLayout {
            id: inner
            anchors { top: headingText.bottom; left: parent.left; right: parent.right; margins: 14; topMargin: 8 }
            spacing: 8
        }
    }

    component LabeledSlider: RowLayout {
        property string label: ""
        property string suffix: "%"
        property real from: 0
        property real to: 100
        property real value: 0
        signal committed(real newValue)
        Layout.fillWidth: true
        spacing: 12
        Text {
            Layout.preferredWidth: 230
            text: label
            color: Theme.text
            font.pixelSize: 13
        }
        Slider {
            id: sliderCtl
            Layout.fillWidth: true
            from: parent.from
            to: parent.to
            value: parent.value
            stepSize: 1
            onPressedChanged: if (!pressed) parent.committed(value)
        }
        Text {
            Layout.preferredWidth: 60
            text: Math.round(sliderCtl.value) + parent.suffix
            color: Theme.accent
            font.pixelSize: 13
            font.bold: true
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }

        ColumnLayout {
            id: column
            x: 20
            y: 16
            width: parent.width - 40
            spacing: Theme.spacing

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Alerts & thresholds")
                    color: Theme.text
                    font.pixelSize: 20
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                Text { text: qsTr("Alerts enabled"); color: Theme.textDim; font.pixelSize: 13 }
                Switch {
                    checked: battery.settingValue("alertsEnabled")
                    onToggled: battery.setSetting("alertsEnabled", checked)
                }
            }

            SectionCard {
                heading: qsTr("THRESHOLD ALERTS")

                LabeledSlider {
                    label: qsTr("Low battery level")
                    from: 5; to: 60
                    value: battery.settingValue("lowLevelThresholdPct")
                    onCommitted: (v) => battery.setSetting("lowLevelThresholdPct", Math.round(v))
                }
                LabeledSlider {
                    label: qsTr("High temperature")
                    suffix: " °C"
                    from: 35; to: 85
                    value: battery.settingValue("highTempThresholdC")
                    onCommitted: (v) => battery.setSetting("highTempThresholdC", Math.round(v))
                }
                LabeledSlider {
                    label: qsTr("Low pack voltage")
                    suffix: " V"
                    from: 5; to: 20
                    value: battery.settingValue("lowVoltageThresholdmV") / 1000.0
                    onCommitted: (v) => battery.setSetting("lowVoltageThresholdmV", Math.round(v * 1000))
                }
            }

            SectionCard {
                heading: qsTr("CHARGING REMINDERS")

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: battery.settingValue("unplugAtFullReminder")
                        onToggled: battery.setSetting("unplugAtFullReminder", checked)
                    }
                    Text {
                        text: qsTr("Remind me to unplug when charged above")
                        color: Theme.text
                        font.pixelSize: 13
                    }
                    SpinBox {
                        from: 50; to: 100
                        value: battery.settingValue("unplugReminderPct")
                        onValueModified: battery.setSetting("unplugReminderPct", value)
                    }
                    Text { text: "%"; color: Theme.textDim; font.pixelSize: 13 }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: battery.settingValue("chargeBelowReminder")
                        onToggled: battery.setSetting("chargeBelowReminder", checked)
                    }
                    Text {
                        text: qsTr("Remind me to plug in when below")
                        color: Theme.text
                        font.pixelSize: 13
                    }
                    SpinBox {
                        from: 5; to: 50
                        value: battery.settingValue("chargeReminderPct")
                        onValueModified: battery.setSetting("chargeReminderPct", value)
                    }
                    Text { text: "%"; color: Theme.textDim; font.pixelSize: 13 }
                    Item { Layout.fillWidth: true }
                }
            }

            SectionCard {
                heading: qsTr("TRAY & STARTUP")

                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: battery.minimizeToTray
                        onToggled: battery.setSetting("minimizeToTray", checked)
                    }
                    Text {
                        text: qsTr("Minimize to tray instead of closing")
                        color: Theme.text
                        font.pixelSize: 13
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        visible: !tray.available
                        text: qsTr("(tray unavailable on this system)")
                        color: Theme.warn
                        font.pixelSize: 11
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Switch {
                        checked: battery.autostartEnabled
                        onToggled: battery.autostartEnabled = checked
                    }
                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: qsTr("Launch with Windows (opt-in)")
                            color: Theme.text
                            font.pixelSize: 13
                        }
                        Text {
                            text: qsTr("Uses a Startup-folder shortcut — Faraday never writes the registry.")
                            color: Theme.textDim
                            font.pixelSize: 11
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
            }

            SectionCard {
                visible: battery.chargeCapSupported
                heading: qsTr("FIRMWARE CHARGE CAP")

                Text {
                    Layout.fillWidth: true
                    text: battery.chargeCapDetail
                    color: Theme.text
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Button {
                        text: qsTr("Enable 80% cap")
                        onClicked: page.chargeCapMessage = battery.tryToggleChargeCap(true)
                    }
                    Button {
                        text: qsTr("Disable cap")
                        onClicked: page.chargeCapMessage = battery.tryToggleChargeCap(false)
                    }
                    Item { Layout.fillWidth: true }
                }
                Text {
                    Layout.fillWidth: true
                    visible: page.chargeCapMessage.length > 0
                    text: page.chargeCapMessage
                    color: Theme.warn
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
