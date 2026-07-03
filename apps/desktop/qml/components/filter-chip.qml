import QtQuick
import QtQuick.Controls

import "../theme"

CheckBox {
    id: chip

    property string toolTipText: ""

    Accessible.role: Accessible.CheckBox
    Accessible.name: chip.text
    Accessible.description: chip.toolTipText

    HoverHandler {
        id: hoverHandler

        cursorShape: chip.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    ToolTip.visible: hoverHandler.hovered && chip.toolTipText.length > 0
    ToolTip.delay: 450
    ToolTip.timeout: 9000
    ToolTip.text: chip.toolTipText

    indicator: Rectangle {
        implicitWidth: 16
        implicitHeight: 16
        x: 10
        y: parent.height / 2 - height / 2
        radius: 5
        color: chip.checked ? Theme.primarySoft : Theme.surfaceSunken
        border.color: chip.checked ? Theme.primary : Theme.borderStrong
        border.width: 1

        Rectangle {
            anchors.centerIn: parent
            width: 7
            height: 7
            radius: 3
            color: Theme.text
            visible: chip.checked
        }
    }

    contentItem: Text {
        text: chip.text
        color: chip.enabled ? Theme.text : Theme.textFaint
        font.pixelSize: Theme.fontSizeSmall
        verticalAlignment: Text.AlignVCenter
        leftPadding: 32
        rightPadding: 12
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitHeight: 32
        radius: 16
        color: chip.hovered && chip.enabled ? Theme.surfaceRaised : Theme.surface
        border.color: chip.checked ? Theme.borderStrong : Theme.border
        border.width: 1
    }
}
