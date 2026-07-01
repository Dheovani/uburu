import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Panel {
    id: root

    property alias model: resultList.model
    property int resultCount: resultList.count

    signal resultSelected(string filePath, string location, string preview)
    signal resultsCleared()

    color: Theme.surface

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                EyebrowLabel {
                    text: qsTr("OCORRÊNCIAS")
                }

                Label {
                    text: qsTr("Resultados")
                    color: Theme.text
                    font.pixelSize: 18
                    font.bold: true
                }
            }

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: resultCountText.implicitWidth + 18
                Layout.preferredHeight: 26
                radius: 13
                color: Theme.surfaceRaised
                border.color: Theme.border
                border.width: 1

                Text {
                    id: resultCountText

                    anchors.centerIn: parent
                    text: qsTr("%1").arg(resultList.count)
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                }
            }
        }

        ListView {
            id: resultList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            boundsBehavior: Flickable.StopAtBounds

            onCountChanged: {
                if (count === 0)
                    root.resultsCleared()
            }

            delegate: ItemDelegate {
                required property int index
                required property string filePath
                required property string location
                required property string preview

                width: ListView.view.width
                height: 64
                highlighted: ListView.isCurrentItem

                onClicked: {
                    resultList.currentIndex = index
                    root.resultSelected(filePath, location, preview)
                }

                contentItem: Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 4

                    Text {
                        text: filePath
                        width: parent.width
                        color: Theme.text
                        font.pixelSize: Theme.fontSize
                        font.bold: true
                        elide: Text.ElideMiddle
                    }

                    Text {
                        text: location
                        width: parent.width
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideRight
                    }
                }

                background: Rectangle {
                    radius: Theme.radius
                    color: highlighted ? Theme.surfaceRaised : hovered ? Theme.surfaceRaised : Theme.surfaceSunken
                    border.color: highlighted ? Theme.primary : Theme.border
                    border.width: 1
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: resultList.count === 0
                color: "transparent"

                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 48, 340)
                    spacing: 8

                    Text {
                        text: qsTr("Pronto para buscar")
                        width: parent.width
                        color: Theme.text
                        font.pixelSize: 17
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        text: qsTr("Escolha uma pasta e digite uma consulta para ver os resultados aqui.")
                        width: parent.width
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
