import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

MenuItem {
    id: root

    property string shortcutText: ""

    implicitWidth: Math.max(238, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: 38
    leftPadding: 12
    rightPadding: 12
    topPadding: 0
    bottomPadding: 0
    font.pixelSize: Theme.fontSizeSmall
    Accessible.role: Accessible.MenuItem
    Accessible.name: root.text

    contentItem: RowLayout {
        spacing: 18

        Text {
            Layout.fillWidth: true
            text: root.text
            color: root.enabled ? Theme.text : Theme.textFaint
            font: root.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            visible: root.shortcutText.length > 0
            text: root.shortcutText
            color: Theme.textFaint
            font.pixelSize: Theme.fontSizeTiny
            verticalAlignment: Text.AlignVCenter
        }
    }

    background: Rectangle {
        radius: 8
        color: root.highlighted ? Theme.primarySoft : "transparent"
        border.color: root.highlighted ? Theme.borderStrong : "transparent"
        border.width: 1
    }
}
