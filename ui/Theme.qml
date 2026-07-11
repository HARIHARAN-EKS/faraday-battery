pragma Singleton
import QtQuick

QtObject {
    id: theme

    property bool dark: true

    readonly property color background: dark ? "#0d1117" : "#f2f4f8"
    readonly property color surface: dark ? "#161b22" : "#ffffff"
    readonly property color surfaceAlt: dark ? "#1c2129" : "#f6f8fa"
    readonly property color border: dark ? "#30363d" : "#d0d7de"
    readonly property color text: dark ? "#e6edf3" : "#1f2328"
    readonly property color textDim: dark ? "#8b949e" : "#656d76"
    readonly property color accent: dark ? "#58a6ff" : "#0969da"
    readonly property color good: dark ? "#3fb950" : "#1a7f37"
    readonly property color warn: dark ? "#d29922" : "#9a6700"
    readonly property color bad: dark ? "#f85149" : "#cf222e"
    readonly property color chartGrid: dark ? "#21262d" : "#e7ebf0"

    readonly property int radius: 10
    readonly property int spacing: 14
}
