import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    width: 1180
    height: 760
    visible: true
    title: qsTr("Uburu — Busca avançada")

    FolderDialog {
        id: folderDialog
        title: qsTr("Selecionar diretório ou repositório")
        onAccepted: searchController.selectDirectory(selectedFolder)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("Texto a pesquisar")
                onAccepted: searchButton.clicked()
            }
            Button {
                text: qsTr("Selecionar pasta")
                onClicked: folderDialog.open()
            }
            Button {
                id: searchButton
                text: qsTr("Buscar")
                enabled: !searchController.running && searchField.text.length > 0 && searchController.directory.length > 0
                onClicked: searchController.startSearch(searchField.text, regex.checked,
                                                         caseSensitive.checked, wholeWord.checked,
                                                         gitignore.checked)
            }
            Button {
                text: qsTr("Cancelar")
                enabled: searchController.running
                onClicked: searchController.cancel()
            }
        }

        Label {
            text: searchController.directory.length > 0
                  ? searchController.directory : qsTr("Nenhum diretório selecionado")
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }

        RowLayout {
            CheckBox { id: regex; text: qsTr("Expressão regular") }
            CheckBox { id: caseSensitive; text: qsTr("Diferenciar maiúsculas e minúsculas") }
            CheckBox { id: wholeWord; text: qsTr("Palavra inteira") }
            CheckBox { id: gitignore; text: qsTr("Respeitar .gitignore"); checked: true }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
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
                    text: filePath + "  " + location
                    highlighted: ListView.isCurrentItem
                    onClicked: resultList.currentIndex = index
                }
                Label {
                    anchors.centerIn: parent
                    text: qsTr("Os resultados aparecerão aqui")
                    visible: resultList.count === 0
                }
            }
            ScrollView {
                SplitView.fillWidth: true
                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    placeholderText: qsTr("Pré-visualização do arquivo")
                    text: resultList.currentItem ? resultList.currentItem.preview : ""
                }
            }
        }

        Label { text: searchController.status }
    }
}
