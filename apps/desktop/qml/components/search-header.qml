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
    property int filesScanned: 0
    property string timeToFirstResult: "—"
    property string searchDuration: "—"
    property bool hasDocumentContentExtractorGap: hasUnsupportedDocumentContentTypes(documentTypesField.text)
    property bool autoSearchEnabled: true
    property int debounceIntervalMs: 450
    property bool pendingAutoSearch: false

    signal selectDirectory()
    signal startSearch(string query,
                       bool regex,
                       bool caseSensitive,
                       bool wholeWord,
                       bool gitignore,
                       bool includeSubdirectories,
                       string documentTypes)
    signal cancelSearch()

    Layout.fillWidth: true
    Layout.preferredHeight: (compact ? 238 : 198) + (hasDocumentContentExtractorGap ? 30 : 0)
    color: Theme.surface

    function hasUnsupportedDocumentContentTypes(text) {
        const unsupportedDocumentTypes = ["pdf", "doc", "docx", "odt", "rtf", "epub"]
        const documentTypes = text.toLowerCase().split(/[,;\s]+/)

        for (const documentType of documentTypes) {
            const normalizedDocumentType = documentType.replace(/^\.+/, "")

            if (unsupportedDocumentTypes.indexOf(normalizedDocumentType) !== -1)
                return true
        }

        return false
    }

    function canSearch() {
        return !root.running && searchField.text.length > 0 && root.directory.length > 0
    }

    function requestDebouncedSearch() {
        if (!root.autoSearchEnabled || searchField.text.length === 0 || root.directory.length === 0)
            return

        if (root.running) {
            root.pendingAutoSearch = true
            root.cancelSearch()
            return
        }

        autoSearchTimer.restart()
    }

    function runSearch() {
        if (!canSearch())
            return

        root.startSearch(
            searchField.text,
            regex.checked,
            caseSensitive.checked,
            wholeWord.checked,
            gitignore.checked,
            includeSubdirectories.checked,
            documentTypesField.text
        )
    }

    Timer {
        id: autoSearchTimer

        interval: root.debounceIntervalMs
        repeat: false
        onTriggered: root.runSearch()
    }

    onRunningChanged: {
        if (root.running || !root.pendingAutoSearch)
            return

        root.pendingAutoSearch = false
        root.requestDebouncedSearch()
    }

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
                    onTextEdited: root.requestDebouncedSearch()

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
                    enabled: root.canSearch()
                    onClicked: root.runSearch()
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
                Layout.alignment: Qt.AlignVCenter

                MutedLabel {
                    text: qsTr("Tipos")
                }

                TextField {
                    id: documentTypesField

                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    placeholderText: qsTr("Ex.: pdf, docx, txt")
                    verticalAlignment: TextInput.AlignVCenter
                    onTextEdited: root.requestDebouncedSearch()

                    color: Theme.text
                    placeholderTextColor: Theme.textMuted
                    selectionColor: Theme.primary
                    selectedTextColor: "white"
                    font.pixelSize: Theme.fontSizeSmall
                    leftPadding: 12
                    rightPadding: 12

                    background: Rectangle {
                        radius: Theme.radius
                        color: Theme.surfaceSunken
                        border.color: documentTypesField.activeFocus ? Theme.primary : Theme.border
                        border.width: 1
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.hasDocumentContentExtractorGap
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    radius: 9
                    color: Theme.surfaceRaised
                    border.color: Theme.warning
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "i"
                        color: Theme.warning
                        font.pixelSize: Theme.fontSizeTiny
                        font.bold: true
                    }
                }

                MutedLabel {
                    Layout.fillWidth: true
                    text: qsTr("PDF, DOCX e formatos semelhantes ainda são filtrados pelo nome; busca no conteúdo depende de extratores futuros.")
                    color: Theme.warning
                    wrapMode: Text.WordWrap
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                FilterChip {
                    id: regex
                    text: qsTr("Regex")
                    onCheckedChanged: root.requestDebouncedSearch()
                }

                FilterChip {
                    id: caseSensitive
                    text: qsTr("Case-sensitive")
                    onCheckedChanged: root.requestDebouncedSearch()
                }

                FilterChip {
                    id: wholeWord
                    text: qsTr("Palavra inteira")
                    onCheckedChanged: root.requestDebouncedSearch()
                }

                FilterChip {
                    id: gitignore
                    text: qsTr("Respeitar .gitignore")
                    checked: true
                    onCheckedChanged: root.requestDebouncedSearch()
                }

                FilterChip {
                    id: includeSubdirectories
                    text: qsTr("Incluir subdiretórios")
                    checked: true
                    onCheckedChanged: root.requestDebouncedSearch()
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

            GridLayout {
                id: metricsColumn

                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: implicitHeight
                columns: 2
                columnSpacing: 8
                rowSpacing: 8

                MetricCard {
                    title: qsTr("Resultados")
                    value: qsTr("%1 visíveis").arg(root.resultCount)
                }

                MetricCard {
                    title: qsTr("Arquivos")
                    value: qsTr("%1 lidos").arg(root.filesScanned)
                }

                MetricCard {
                    title: qsTr("Primeiro")
                    value: root.timeToFirstResult
                }

                MetricCard {
                    title: qsTr("Duração")
                    value: root.searchDuration
                }
            }
        }
    }
}
