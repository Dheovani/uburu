import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Panel {
    id: root

    property string directory: ""
    property bool running: false
    property bool compact: false
    property int resultCount: 0

    signal selectDirectory()
    signal startSearch(string query, bool regex, bool caseSensitive, bool wholeWord, bool gitignore)
    signal cancelSearch()

    Layout.fillWidth: true
    Layout.preferredHeight: compact ? 196 : 164
    color: Theme.surface

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Item {
                Layout.fillHeight: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    EyebrowLabel {
                        text: qsTr("BUSCA LOCAL")
                    }

                    Label {
                        text: qsTr("Uburu")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeTitle
                        font.bold: true
                    }
                }

                Rectangle {
                    Layout.preferredHeight: 28
                    Layout.preferredWidth: statusText.implicitWidth + 22
                    radius: 14
                    color: Theme.surfaceRaised
                    border.color: Theme.border
                    border.width: 1

                    Text {
                        id: statusText

                        anchors.centerIn: parent
                        text: root.running ? qsTr("Buscando") : qsTr("Pronto")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeTiny
                        font.bold: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Layout.alignment: Qt.AlignVCenter

                TextField {
                    id: searchField

                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    placeholderText: qsTr("Pesquisar em arquivos")
                    verticalAlignment: TextInput.AlignVCenter
                    onAccepted: searchButton.clicked()

                    color: Theme.text
                    placeholderTextColor: Theme.textMuted
                    selectionColor: Theme.primary
                    selectedTextColor: "white"
                    font.pixelSize: 15
                    leftPadding: 14
                    rightPadding: 14

                    background: Rectangle {
                        radius: Theme.radius
                        color: Theme.surfaceSunken
                        border.color: searchField.activeFocus ? Theme.primary : Theme.border
                        border.width: 1
                    }
                }

                SecondaryButton {
                    text: qsTr("Pasta")
                    onClicked: root.selectDirectory()
                }

                PrimaryButton {
                    id: searchButton

                    text: qsTr("Buscar")
                    enabled: !root.running && searchField.text.length > 0 && root.directory.length > 0
                    onClicked: root.startSearch(
                        searchField.text,
                        regex.checked,
                        caseSensitive.checked,
                        wholeWord.checked,
                        gitignore.checked
                    )
                }

                SecondaryButton {
                    text: qsTr("Cancelar")
                    enabled: root.running
                    onClicked: root.cancelSearch()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                FilterChip {
                    id: regex
                    text: qsTr("Regex")
                }

                FilterChip {
                    id: caseSensitive
                    text: qsTr("Case-sensitive")
                }

                FilterChip {
                    id: wholeWord
                    text: qsTr("Palavra inteira")
                }

                FilterChip {
                    id: gitignore
                    text: qsTr("Respeitar .gitignore")
                    checked: true
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }

        Item {
            Layout.preferredWidth: root.compact ? 0 : 238
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignVCenter
            visible: !root.compact

            ColumnLayout {
                id: metricsColumn

                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: implicitHeight
                spacing: 8

                MetricCard {
                    title: qsTr("Modo")
                    value: qsTr("Progressivo")
                }

                MetricCard {
                    title: qsTr("Resultados")
                    value: qsTr("%1 visíveis").arg(root.resultCount)
                }
            }
        }
    }
}
