import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Rectangle {
    property string status: ""
    property string indexingStatus: ""
    property int searchProgress: 0
    property bool searchProgressIndeterminate: running
    property int indexingProgress: 0
    property bool indexingProgressIndeterminate: indexingRunning && indexingProgress <= 0
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

        ActivityProgress {
            Layout.preferredWidth: 86
            visible: running
            value: searchProgress
            indeterminate: searchProgressIndeterminate
            accentColor: cancelling ? Theme.warning : Theme.primary
            Accessible.name: qsTr("Progresso da busca")
        }

        ActivityProgress {
            Layout.preferredWidth: 96
            visible: indexingRunning
            value: indexingProgress
            indeterminate: indexingProgressIndeterminate
            accentColor: Theme.primary
            Accessible.name: qsTr("Progresso da indexação")
        }
    }

    component ActivityProgress: Item {
        property int value: 0
        property bool indeterminate: false
        property color accentColor: Theme.primary

        Layout.preferredHeight: 6
        implicitHeight: 6
        clip: true

        onIndeterminateChanged: {
            if (!indeterminate)
                progressFill.x = 0
        }

        Rectangle {
            anchors.fill: parent
            radius: 3
            color: Theme.surfaceRaised
            border.color: Theme.border
        }

        Rectangle {
            id: progressFill

            width: parent.indeterminate ? parent.width * 0.34 : parent.width * Math.max(0, Math.min(1, parent.value / 100))
            height: parent.height
            radius: 3
            color: parent.accentColor
            opacity: parent.indeterminate ? 0.78 : 1
            x: 0

            NumberAnimation on x {
                running: progressFill.parent.visible && progressFill.parent.indeterminate
                loops: Animation.Infinite
                from: -progressFill.width
                to: progressFill.parent.width
                duration: 900
            }
        }
    }
}
