import QtQuick
import QtQuick.Controls

import "../theme"

Button {
    id: control

    implicitHeight: 40
    leftPadding: 14
    rightPadding: 14
    font.pixelSize: Theme.fontSize

    contentItem: Text {
        text: control.text
        color: control.enabled ? Theme.text : Theme.textMuted
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radius
        color: control.down
               ? Theme.surfaceRaised
               : control.hovered && control.enabled
                 ? "#242a38"
                 : Theme.surface
        border.color: control.hovered && control.enabled ? Theme.primary : Theme.border
        border.width: 1
    }
}
