import QtQuick
import QtQuick.Controls

import "../theme"

Rectangle {
    id: root

    property string text: ""

    implicitWidth: 18
    implicitHeight: 18
    radius: 9
    color: hoverHandler.hovered ? Theme.surfaceRaised : Theme.surfaceSunken
    border.color: hoverHandler.hovered ? Theme.primary : Theme.border
    border.width: 1
    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("Informação")
    Accessible.description: root.text

    Text {
        anchors.centerIn: parent
        text: "i"
        color: hoverHandler.hovered ? Theme.text : Theme.textMuted
        font.pixelSize: Theme.fontSizeTiny
        font.bold: true
    }

    HoverHandler {
        id: hoverHandler

        cursorShape: Qt.WhatsThisCursor
    }

    ToolTip.visible: hoverHandler.hovered && root.text.length > 0
    ToolTip.delay: 450
    ToolTip.timeout: 9000
    ToolTip.text: root.text
}
