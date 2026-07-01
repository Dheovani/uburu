import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Rectangle {
    property string directory: ""

    Layout.fillWidth: true
    Layout.preferredHeight: 34
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        Label {
            text: qsTr("Escopo")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeTiny
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            text: directory.length > 0 ? directory : qsTr("Nenhum diretório selecionado")
            color: directory.length > 0 ? Theme.text : Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideMiddle
        }
    }
}
