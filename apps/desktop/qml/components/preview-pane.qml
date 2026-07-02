import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Panel {
    id: root

    property string filePath: ""
    property string location: ""
    property string preview: ""
    property bool loading: false

    color: Theme.windowRaised

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            EyebrowLabel {
                text: qsTr("PREVIEW")
            }

            Label {
                Layout.fillWidth: true
                text: root.filePath.length > 0 ? root.filePath : qsTr("Pré-visualização")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideMiddle
            }

            MutedLabel {
                Layout.fillWidth: true
                text: root.loading
                      ? qsTr("Carregando pré-visualização...")
                      : root.location.length > 0
                      ? root.location
                      : qsTr("Selecione um resultado para inspecionar o contexto.")
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surfaceSunken
            border.color: Theme.border
            border.width: 1
            clip: true

            ScrollView {
                anchors.fill: parent
                anchors.margins: 1

                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    placeholderText: qsTr("O conteúdo do arquivo aparecerá aqui")
                    text: root.preview

                    color: Theme.text
                    placeholderTextColor: Theme.textFaint
                    selectionColor: Theme.primary
                    selectedTextColor: "white"
                    font.family: "Consolas"
                    font.pixelSize: 13
                    leftPadding: 16
                    rightPadding: 16
                    topPadding: 16
                    bottomPadding: 16

                    background: Rectangle {
                        color: "transparent"
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: root.loading
                color: "#66000000"

                BusyIndicator {
                    anchors.centerIn: parent
                    running: root.loading
                }
            }
        }
    }
}
