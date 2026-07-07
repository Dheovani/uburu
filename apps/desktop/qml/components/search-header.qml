pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Panel {
    id: root

    property string directory: ""
    property bool running: false
    property bool cancelling: false
    property bool compact: false
    property int resultCount: 0
    property int filesScanned: 0
    property string timeToFirstResult: "—"
    property string searchDuration: "—"
    property bool regexAvailable: true
    property bool hasDocumentContentExtractorGap: hasUnsupportedDocumentContentTypes(documentTypesField.text)
    property bool hasSearchMemory: recentSearches.length > 0 || savedSearches.length > 0 || queryText.length > 0
    property bool autoSearchEnabled: true
    property int debounceIntervalMs: 450
    property bool pendingAutoSearch: false
    property var recentSearches: []
    property var savedSearches: []
    property var selectedDirectories: []
    property var includedDirectories: []
    property var excludedDirectories: []
    property var recentDirectories: []
    property var favoriteDirectories: []
    property bool currentSearchSaved: false
    property bool currentDirectoryFavorite: false
    property alias queryText: searchField.text
    property alias documentTypes: documentTypesField.text
    property alias regexEnabled: regex.checked
    property alias caseSensitiveEnabled: caseSensitive.checked
    property alias wholeWordEnabled: wholeWord.checked
    property alias respectGitignoreEnabled: gitignore.checked
    property alias includeHiddenEnabled: includeHidden.checked
    property alias includeBinaryEnabled: includeBinary.checked
    property alias includeSubdirectoriesEnabled: includeSubdirectories.checked

    signal selectDirectory()
    signal addIncludedDirectory()
    signal addExcludedDirectory()
    signal startSearch(string query,
                       bool regex,
                       bool caseSensitive,
                       bool wholeWord,
                       bool gitignore,
                       bool includeHidden,
                       bool includeBinary,
                       bool includeSubdirectories,
                       string documentTypes)
    signal cancelSearch()
    signal selectSearch(string query)
    signal selectScopeDirectory(string path)
    signal toggleFavoriteDirectory(string path)
    signal toggleCurrentSearchSaved()

    Layout.fillWidth: true
    Layout.preferredHeight: (compact ? 266 : 226) + Math.max(0, filterFlow.implicitHeight - 34)
                            + (hasDocumentContentExtractorGap ? 30 : 0)
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

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function shortSearch(text) {
        if (text.length <= 80)
            return text

        return text.slice(0, 77) + "..."
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
            root.regexAvailable && regex.checked,
            caseSensitive.checked,
            wholeWord.checked,
            gitignore.checked,
            includeHidden.checked,
            includeBinary.checked,
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

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: qsTr("Uburu")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeTitle
                        font.bold: true
                    }

                    EyebrowLabel {
                        text: qsTr("BUSCA LOCAL")
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
                        text: root.cancelling
                              ? qsTr("Cancelando")
                              : root.running
                                ? qsTr("Buscando")
                                : qsTr("Pronto")
                        color: root.cancelling ? Theme.warning : Theme.textMuted
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
                    onTextEdited: {
                        root.requestDebouncedSearch()

                        if (root.hasSearchMemory)
                            searchMemoryPopup.open()
                    }
                    onActiveFocusChanged: {
                        if (activeFocus && root.hasSearchMemory)
                            searchMemoryPopup.open()
                    }
                    Accessible.name: qsTr("Consulta de busca")
                    Accessible.description: qsTr(
                        "Digite o texto ou expressão regular que será pesquisado nos arquivos."
                    )

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

                    Popup {
                        id: searchMemoryPopup

                        x: 0
                        y: searchField.height + 6
                        width: searchField.width
                        padding: 8
                        modal: false
                        focus: false
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                        background: Rectangle {
                            radius: Theme.radius
                            color: Theme.surface
                            border.color: Theme.border
                            border.width: 1
                        }

                        contentItem: Column {
                            spacing: 6

                            SearchMemoryRow {
                                width: parent.width
                                visible: root.queryText.length > 0
                                text: root.currentSearchSaved
                                      ? qsTr("Remover busca salva")
                                      : qsTr("Salvar busca atual")
                                detail: root.queryText
                                accessibleName: text
                                emphasized: true
                                onClicked: {
                                    searchMemoryPopup.close()
                                    root.toggleCurrentSearchSaved()
                                }
                            }

                            MutedLabel {
                                text: qsTr("Buscas salvas")
                                visible: root.savedSearches.length > 0
                            }

                            Repeater {
                                model: root.savedSearches

                                delegate: SearchMemoryRow {
                                    required property string modelData

                                    width: searchMemoryPopup.contentItem.width
                                    text: root.shortSearch(modelData)
                                    detail: qsTr("Salva")
                                    accessibleName: qsTr("Busca salva: %1").arg(modelData)
                                    onClicked: {
                                        searchMemoryPopup.close()
                                        root.selectSearch(modelData)
                                    }
                                }
                            }

                            MutedLabel {
                                text: qsTr("Buscas recentes")
                                visible: root.recentSearches.length > 0
                            }

                            Repeater {
                                model: root.recentSearches

                                delegate: SearchMemoryRow {
                                    required property string modelData

                                    width: searchMemoryPopup.contentItem.width
                                    text: root.shortSearch(modelData)
                                    detail: qsTr("Recente")
                                    accessibleName: qsTr("Busca recente: %1").arg(modelData)
                                    onClicked: {
                                        searchMemoryPopup.close()
                                        root.selectSearch(modelData)
                                    }
                                }
                            }
                        }
                    }
                }

                PrimaryButton {
                    id: searchButton

                    text: qsTr("Buscar")
                    enabled: root.canSearch()
                    onClicked: root.runSearch()
                }

                SecondaryButton {
                    text: root.cancelling ? qsTr("Cancelando") : qsTr("Cancelar")
                    enabled: root.running && !root.cancelling
                    onClicked: root.cancelSearch()
                }
            }

            ScopeBar {
                directory: root.directory
                selectedDirectories: root.selectedDirectories
                includedDirectories: root.includedDirectories
                excludedDirectories: root.excludedDirectories
                recentDirectories: root.recentDirectories
                favoriteDirectories: root.favoriteDirectories
                currentDirectoryFavorite: root.currentDirectoryFavorite
                onBrowseDirectory: root.selectDirectory()
                onSelectDirectory: path => root.selectScopeDirectory(path)
                onToggleFavorite: path => root.toggleFavoriteDirectory(path)
                onAddIncludedDirectory: root.addIncludedDirectory()
                onAddExcludedDirectory: root.addExcludedDirectory()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Layout.alignment: Qt.AlignVCenter

                MutedLabel {
                    text: qsTr("Tipos")
                }

                InfoIcon {
                    text: qsTr("Filtre por extensões separadas por vírgula ou espaço, como txt, cpp ou md. Formatos como PDF e DOCX ainda dependem de extratores futuros para busca no conteúdo.")
                }

                TextField {
                    id: documentTypesField

                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    placeholderText: qsTr("Ex.: pdf, docx, txt")
                    verticalAlignment: TextInput.AlignVCenter
                    onTextEdited: root.requestDebouncedSearch()
                    Accessible.name: qsTr("Tipos de documento")
                    Accessible.description: qsTr("Filtre por extensões separadas por vírgula ou espaço.")

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

            Flow {
                id: filterFlow

                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight
                spacing: 6

                FilterChip {
                    id: regex
                    text: qsTr("Regex")
                    enabled: root.regexAvailable
                    toolTipText: root.regexAvailable
                                 ? qsTr("Interpreta a consulta como expressão regular. Use quando precisar de padrões; para texto simples, deixe desligado para manter a busca mais direta.")
                                 : qsTr("Regex indisponível neste build porque o backend PCRE2 não foi encontrado.")
                    onCheckedChanged: root.requestDebouncedSearch()
                }

                FilterChip {
                    id: caseSensitive
                    text: qsTr("Diferenciar maiúsculas")
                    toolTipText: qsTr("Diferencia letras maiúsculas e minúsculas. Quando desligado, Verdade e verdade são tratados como equivalentes.")
                    onCheckedChanged: root.requestDebouncedSearch()
                }

                FilterChip {
                    id: wholeWord
                    text: qsTr("Palavra inteira")
                    toolTipText: qsTr("Retorna apenas ocorrências isoladas por limites de palavra, evitando correspondências dentro de outras palavras.")
                    onCheckedChanged: root.requestDebouncedSearch()
                }

                FilterChip {
                    id: gitignore
                    text: qsTr("Respeitar .gitignore")
                    checked: true
                    toolTipText: qsTr("Ignora arquivos e diretórios cobertos por regras .gitignore, como build, node_modules e outros artefatos do repositório.")
                    onCheckedChanged: root.requestDebouncedSearch()
                }

                FilterChip {
                    id: includeHidden
                    text: qsTr("Incluir ocultos")
                    toolTipText: qsTr("Inclui arquivos ocultos. Deixe desligado para evitar caches e metadados.")
                    onCheckedChanged: root.requestDebouncedSearch()
                }

                FilterChip {
                    id: includeBinary
                    text: qsTr("Incluir binários")
                    toolTipText: qsTr("Tenta ler binários. Pode reduzir a velocidade e gerar resultados pouco úteis.")
                    onCheckedChanged: root.requestDebouncedSearch()
                }

                FilterChip {
                    id: includeSubdirectories
                    text: qsTr("Incluir subdiretórios")
                    checked: true
                    toolTipText: qsTr("Pesquisa também dentro das subpastas do diretório selecionado. Desligue para limitar a busca apenas à pasta atual.")
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

    component SearchMemoryRow: Rectangle {
        id: searchMemoryRow

        property string text: ""
        property string detail: ""
        property string accessibleName: text
        property bool emphasized: false

        signal clicked()

        height: 42
        radius: 10
        color: chipMouseArea.containsMouse ? Theme.surfaceRaised : "transparent"
        border.color: emphasized ? Theme.primary : "transparent"
        border.width: 1
        Accessible.role: Accessible.Button
        Accessible.name: accessibleName

        Column {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.topMargin: 5
            anchors.bottomMargin: 5
            spacing: 2

            Text {
                width: parent.width
                text: searchMemoryRow.text
                color: Theme.text
                font.pixelSize: Theme.fontSizeSmall
                font.bold: searchMemoryRow.emphasized
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: searchMemoryRow.detail
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeTiny
                elide: Text.ElideRight
            }
        }

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        MouseArea {
            id: chipMouseArea

            anchors.fill: parent
            hoverEnabled: true
            onClicked: searchMemoryRow.clicked()
        }
    }
}
