import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Rectangle {
    property string status: ""
    property string indexingStatus: ""
    property int indexingProgress: 0
    property bool indexingRunning: false
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

        Label {
            text: indexingStatus.length > 0 ? indexingStatus : qsTr("Indexação inativa")
            color: Theme.textFaint
            font.pixelSize: Theme.fontSizeTiny
            elide: Text.ElideRight
            Layout.maximumWidth: parent.width * 0.42
        }

        ProgressBar {
            Layout.preferredWidth: 96
            Layout.preferredHeight: 6
            from: 0
            to: 100
            value: indexingProgress
            visible: indexingRunning
            Accessible.name: qsTr("Progresso da indexação")

            background: Rectangle {
                radius: 3
                color: Theme.surfaceRaised
                border.color: Theme.border
            }

            contentItem: Item {
                Rectangle {
                    width: parent.width * (indexingProgress / 100)
                    height: parent.height
                    radius: 3
                    color: Theme.primary
                }
            }
        }
    }
}
