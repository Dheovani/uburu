import QtQuick
import QtQuick.Layouts

import "../theme"

Rectangle {
    property string title
    property string value

    Layout.fillWidth: true
    Layout.preferredHeight: 58
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    Column {
        anchors.centerIn: parent
        width: parent.width - 24
        spacing: 3

        Text {
            text: title
            width: parent.width
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeTiny
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text {
            text: value
            width: parent.width
            color: Theme.text
            font.pixelSize: Theme.fontSizeSmall
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }
}
