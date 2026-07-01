pragma Singleton

import QtQuick

QtObject {
    readonly property color window: "#070a12"
    readonly property color windowRaised: "#0d1220"
    readonly property color surface: "#111827"
    readonly property color surfaceRaised: "#182235"
    readonly property color surfaceSunken: "#080c15"
    readonly property color border: "#263247"
    readonly property color borderStrong: "#3b4a67"
    readonly property color primary: "#4f7cff"
    readonly property color primaryPressed: "#365fd5"
    readonly property color primarySoft: "#203a7a"
    readonly property color accent: "#17e6a1"
    readonly property color accentSoft: "#123f36"
    readonly property color warning: "#ffcc66"
    readonly property color text: "#eef4ff"
    readonly property color textMuted: "#97a3ba"
    readonly property color textFaint: "#65738d"

    readonly property int spacing: 10
    readonly property int radius: 12
    readonly property int radiusLarge: 18
    readonly property int fontSize: 14
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeTiny: 11
    readonly property int fontSizeTitle: 24
}
