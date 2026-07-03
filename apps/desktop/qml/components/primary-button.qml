import QtQuick
import QtQuick.Controls

import "../theme"

Button {
    id: control

    implicitHeight: 40
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: Theme.fontSize
    Accessible.role: Accessible.Button
    Accessible.name: control.text

    HoverHandler {
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? "white" : Theme.textMuted
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radius
        color: !control.enabled
               ? Theme.surfaceRaised
               : control.down
                 ? Theme.primaryPressed
                 : Theme.primary
        border.color: control.hovered && control.enabled ? "#86a2ff" : "transparent"
        border.width: 1
    }
}
