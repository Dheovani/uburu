import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import "theme"
import "components"

ApplicationWindow {
    width: 1180
    height: 760
    visible: true
    title: qsTr("Uburu — Busca avançada")
    color: Theme.window

    FolderDialog {
        id: folderDialog
        title: qsTr("Selecionar diretório ou repositório")
        onAccepted: searchController.selectDirectory(selectedFolder)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: Theme.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            property int controlHeight: 40

            TextField {
                id: searchField
                Layout.fillWidth: true
                Layout.preferredHeight: parent.controlHeight
                placeholderText: qsTr("Texto a pesquisar")
                verticalAlignment: TextInput.AlignVCenter
                onAccepted: searchButton.clicked()

                color: Theme.text
                placeholderTextColor: Theme.textMuted
                selectionColor: Theme.primary
                selectedTextColor: "white"
                font.pixelSize: Theme.fontSize
                padding: 10

                background: Rectangle {
                    radius: Theme.radius
                    color: Theme.surface
                    border.color: searchField.activeFocus ? Theme.primary : Theme.border
                    border.width: 1
                }
            }

            SecondaryButton {
                text: qsTr("Selecionar pasta")
                onClicked: folderDialog.open()
            }

            PrimaryButton {
                id: searchButton
                text: qsTr("Buscar")
                enabled: !searchController.running
                         && searchField.text.length > 0
                         && searchController.directory.length > 0

                onClicked: searchController.startSearch(
                    searchField.text,
                    regex.checked,
                    caseSensitive.checked,
                    wholeWord.checked,
                    gitignore.checked
                )
            }

            SecondaryButton {
                text: qsTr("Cancelar")
                enabled: searchController.running
                onClicked: searchController.cancel()
            }
        }

        Label {
            text: searchController.directory.length > 0
                  ? searchController.directory
                  : qsTr("Nenhum diretório selecionado")

            elide: Text.ElideMiddle
            Layout.fillWidth: true
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
        }

        RowLayout {
            spacing: 14

            CheckBox {
                id: regex
                text: qsTr("Expressão regular")
                palette.text: Theme.text
            }

            CheckBox {
                id: caseSensitive
                text: qsTr("Diferenciar maiúsculas e minúsculas")
                palette.text: Theme.text
            }

            CheckBox {
                id: wholeWord
                text: qsTr("Palavra inteira")
                palette.text: Theme.text
            }

            CheckBox {
                id: gitignore
                text: qsTr("Respeitar .gitignore")
                checked: true
                palette.text: Theme.text
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            background: Rectangle {
                color: Theme.surface
                radius: Theme.radius
                border.color: Theme.border
            }

            ListView {
                id: resultList
                SplitView.preferredWidth: 470
                model: searchController.results
                clip: true

                delegate: ItemDelegate {
                    required property int index
                    required property string filePath
                    required property string location
                    required property string preview

                    width: ListView.view.width
                    height: 54
                    highlighted: ListView.isCurrentItem
                    onClicked: resultList.currentIndex = index

                    contentItem: Column {
                        spacing: 3

                        Text {
                            text: filePath
                            color: Theme.text
                            font.pixelSize: Theme.fontSize
                            elide: Text.ElideMiddle
                            width: parent.width
                        }

                        Text {
                            text: location
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }

                    background: Rectangle {
                        color: highlighted
                            ? Qt.rgba(0.31, 0.49, 1.0, 0.22)
                            : hovered
                                ? Theme.surfaceRaised
                                : "transparent"

                        border.color: highlighted ? Theme.primary : "transparent"
                        border.width: highlighted ? 1 : 0
                        radius: 8
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: qsTr("Os resultados aparecerão aqui")
                    visible: resultList.count === 0
                    color: Theme.textMuted
                }
            }

            ScrollView {
                SplitView.fillWidth: true

                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    placeholderText: qsTr("Pré-visualização do arquivo")
                    text: resultList.currentItem ? resultList.currentItem.preview : ""

                    color: Theme.text
                    placeholderTextColor: Theme.textMuted
                    selectionColor: Theme.primary
                    selectedTextColor: "white"
                    font.family: "Consolas"
                    font.pixelSize: 13

                    background: Rectangle {
                        color: "#0b0d12"
                        border.color: Theme.border
                        border.width: 1
                    }
                }
            }
        }

        Label {
            text: searchController.status
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
        }
    }
}