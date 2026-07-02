pragma Singleton

import QtQuick

QtObject {
    property string mode: "system"

    readonly property bool systemPrefersDark: Qt.application.styleHints.colorScheme === Qt.ColorScheme.Dark
    readonly property bool systemMode: mode !== "light" && mode !== "dark"
    readonly property bool darkMode: mode === "dark" || (systemMode && systemPrefersDark)

    readonly property color window: darkMode ? "#070a12" : "#eef3fb"
    readonly property color windowRaised: darkMode ? "#0d1220" : "#ffffff"
    readonly property color surface: darkMode ? "#111827" : "#f8fbff"
    readonly property color surfaceRaised: darkMode ? "#182235" : "#e9f0fb"
    readonly property color surfaceSunken: darkMode ? "#080c15" : "#edf3fb"
    readonly property color border: darkMode ? "#263247" : "#c8d3e4"
    readonly property color borderStrong: darkMode ? "#3b4a67" : "#8ea2c0"
    readonly property color primary: darkMode ? "#4f7cff" : "#315fd8"
    readonly property color primaryPressed: darkMode ? "#365fd5" : "#244cb0"
    readonly property color primarySoft: darkMode ? "#203a7a" : "#dce7ff"
    readonly property color accent: darkMode ? "#17e6a1" : "#008f68"
    readonly property color accentSoft: darkMode ? "#123f36" : "#d8f6ed"
    readonly property color warning: darkMode ? "#ffcc66" : "#a46000"
    readonly property color text: darkMode ? "#eef4ff" : "#142033"
    readonly property color textMuted: darkMode ? "#97a3ba" : "#50627d"
    readonly property color textFaint: darkMode ? "#65738d" : "#7c8da8"
    readonly property color previewSurface: "#080c15"
    readonly property color previewText: "#eef4ff"

    readonly property int spacing: 10
    readonly property int radius: 12
    readonly property int radiusLarge: 18
    readonly property int fontSize: 14
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeTiny: 11
    readonly property int fontSizeTitle: 24

}
