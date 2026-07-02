import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Rectangle {
    property string status: ""
    property bool running: false
    property bool cancelling: false

    Layout.fillWidth: true
    Layout.preferredHeight: 34
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 7
            Layout.preferredHeight: 7
            radius: 4
            color: cancelling ? Theme.warning : running ? Theme.primary : Theme.accent
            opacity: 0.78
        }

        Label {
            Layout.fillWidth: true
            text: status.length > 0 ? status : qsTr("Aguardando comando de busca")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideRight
        }
    }
}
