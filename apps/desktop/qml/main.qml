import QtQuick
import QtQuick.Controls
import QtCore
import QtQuick.Dialogs
import QtQuick.Layouts

import "components"
import "theme"

ApplicationWindow {
    id: root

    width: 1180
    height: 740
    minimumWidth: 860
    minimumHeight: 620
    visible: true
    title: qsTr("Uburu — Busca avançada")
    color: Theme.window

    property bool compact: width < 980
    property int resultsPanePreferredWidth: 430
    property int resultsPanePreferredHeight: 260
    property int maximumStoredSearches: 8
    property var recentSearches: []
    property var savedSearches: []
    property var commandPaletteItems: [
        {
            title: qsTr("Focar busca"),
            description: qsTr("Ir para o campo principal de pesquisa"),
            shortcut: qsTr("Ctrl+F"),
            available: true
        },
        {
            title: qsTr("Selecionar pasta"),
            description: qsTr("Escolher outro diretório ou repositório"),
            shortcut: qsTr("Ctrl+O"),
            available: true
        },
        {
            title: qsTr("Iniciar busca"),
            description: qsTr("Executar a consulta atual"),
            shortcut: qsTr("Enter"),
            available: searchHeader.canSearch()
        },
        {
            title: qsTr("Cancelar busca"),
            description: qsTr("Interromper a busca em andamento"),
            shortcut: qsTr("Esc"),
            available: searchController.running
        },
        {
            title: qsTr("Favoritar pasta atual"),
            description: qsTr("Alternar favorito para o diretório selecionado"),
            shortcut: qsTr("Ctrl+D"),
            available: searchController.directory.length > 0
        },
        {
            title: qsTr("Copiar diagnóstico da busca"),
            description: qsTr("Copiar status, contadores e tempos da busca atual"),
            shortcut: "",
            available: true
        },
        {
            title: root.isSearchSaved(searchHeader.queryText)
                   ? qsTr("Remover busca salva")
                   : qsTr("Salvar busca atual"),
            description: qsTr("Alternar busca atual na lista de buscas salvas"),
            shortcut: qsTr("Ctrl+S"),
            available: searchHeader.queryText.length > 0
        },
        {
            title: qsTr("Executar última busca salva"),
            description: qsTr("Carregar e executar a busca salva mais recente"),
            shortcut: "",
            available: root.savedSearches.length > 0
        },
        {
            title: qsTr("Executar última busca recente"),
            description: qsTr("Carregar e executar a busca recente mais nova"),
            shortcut: "",
            available: root.recentSearches.length > 0
        },
        {
            title: qsTr("Abrir resultado selecionado"),
            description: qsTr("Abrir o arquivo do resultado atual"),
            shortcut: qsTr("Enter"),
            available: resultsPane.hasCurrentResult()
        },
        {
            title: qsTr("Próxima ocorrência"),
            description: qsTr("Selecionar a próxima ocorrência visível"),
            shortcut: qsTr("F4"),
            available: resultsPane.resultCount > 0
        },
        {
            title: qsTr("Ocorrência anterior"),
            description: qsTr("Selecionar a ocorrência visível anterior"),
            shortcut: qsTr("Shift+F4"),
            available: resultsPane.resultCount > 0
        },
        {
            title: qsTr("Copiar caminho do resultado"),
            description: qsTr("Copiar o caminho absoluto do resultado atual"),
            shortcut: qsTr("Ctrl+C"),
            available: resultsPane.hasCurrentResult()
        },
        {
            title: qsTr("Copiar ocorrência do resultado"),
            description: qsTr("Copiar caminho, localização e trecho da ocorrência atual"),
            shortcut: qsTr("Ctrl+Shift+C"),
            available: resultsPane.hasCurrentResult()
        }
    ]

    function runPaletteCommand(commandIndex) {
        switch (commandIndex) {
        case 0:
            searchHeader.focusSearch()
            return
        case 1:
            folderDialog.open()
            return
        case 2:
            searchHeader.runSearch()
            return
        case 3:
            searchController.cancel()
            return
        case 4:
            searchController.toggleCurrentDirectoryFavorite()
            return
        case 5:
            searchController.copyToClipboard(root.searchDiagnosticText())
            return
        case 6:
            root.toggleSavedSearch(searchHeader.queryText)
            return
        case 7:
            root.useStoredSearch(root.savedSearches[0] || "")
            return
        case 8:
            root.useStoredSearch(root.recentSearches[0] || "")
            return
        case 9:
            resultsPane.openCurrentResult()
            return
        case 10:
            resultsPane.selectNextResult()
            return
        case 11:
            resultsPane.selectPreviousResult()
            return
        case 12:
            resultsPane.copyCurrentPath()
            return
        case 13:
            resultsPane.copyCurrentOccurrence()
            return
        default:
            return
        }
    }

    function normalizeThemeMode(mode) {
        return mode === "system" || mode === "dark" || mode === "light" ? mode : "system"
    }

    function searchDiagnosticText() {
        return [
            qsTr("Status: %1").arg(searchController.status),
            qsTr("Resultados visíveis: %1").arg(resultsPane.resultCount),
            qsTr("Arquivos lidos: %1").arg(searchController.filesScanned),
            qsTr("Tempo até primeiro resultado: %1").arg(searchController.timeToFirstResult),
            qsTr("Duração total: %1").arg(searchController.searchDuration)
        ].join("\n")
    }

    function normalizedSearchText(query) {
        return query.trim()
    }

    function parseStoredList(text) {
        if (!text || text.length === 0)
            return []

        try {
            const parsed = JSON.parse(text)

            if (!Array.isArray(parsed))
                return []

            return parsed.filter(value => typeof value === "string" && value.length > 0)
        } catch (error) {
            return []
        }
    }

    function withSearchAtFront(searches, query) {
        const normalizedQuery = normalizedSearchText(query)

        if (normalizedQuery.length === 0)
            return searches

        const nextSearches = searches.filter(search => search !== normalizedQuery)
        nextSearches.unshift(normalizedQuery)

        return nextSearches.slice(0, root.maximumStoredSearches)
    }

    function isSearchSaved(query) {
        const normalizedQuery = normalizedSearchText(query)

        return normalizedQuery.length > 0 && root.savedSearches.indexOf(normalizedQuery) !== -1
    }

    function persistSearchMemory() {
        mainWindowSettings.recentSearches = JSON.stringify(root.recentSearches)
        mainWindowSettings.savedSearches = JSON.stringify(root.savedSearches)
    }

    function recordSearch(query) {
        root.recentSearches = withSearchAtFront(root.recentSearches, query)
        root.persistSearchMemory()
    }

    function toggleSavedSearch(query) {
        const normalizedQuery = normalizedSearchText(query)

        if (normalizedQuery.length === 0)
            return

        if (root.isSearchSaved(normalizedQuery))
            root.savedSearches = root.savedSearches.filter(search => search !== normalizedQuery)
        else
            root.savedSearches = withSearchAtFront(root.savedSearches, normalizedQuery)

        root.persistSearchMemory()
    }

    function useStoredSearch(query) {
        searchHeader.queryText = query
        searchHeader.focusSearch()
        searchHeader.runSearch()
    }

    FolderDialog {
        id: folderDialog

        title: qsTr("Selecionar diretório ou repositório")
        onAccepted: searchController.selectDirectory(selectedFolder.toString())
    }

    FolderDialog {
        id: excludedDirectoryDialog

        title: qsTr("Selecionar subpasta para ignorar")
        onAccepted: searchController.addExcludedDirectory(selectedFolder.toString())
    }

    FolderDialog {
        id: includedDirectoryDialog

        title: qsTr("Selecionar subpasta para incluir")
        onAccepted: searchController.addIncludedDirectory(selectedFolder.toString())
    }

    Shortcut {
        sequences: ["Ctrl+K", "Ctrl+Shift+P"]
        onActivated: commandPalette.openPalette()
    }

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: searchHeader.focusSearch()
    }

    Shortcut {
        sequence: "Ctrl+O"
        onActivated: folderDialog.open()
    }

    Shortcut {
        sequence: "Ctrl+D"
        enabled: searchController.directory.length > 0
        onActivated: searchController.toggleCurrentDirectoryFavorite()
    }

    Shortcut {
        sequence: "Ctrl+S"
        enabled: searchHeader.queryText.length > 0
        onActivated: root.toggleSavedSearch(searchHeader.queryText)
    }

    Shortcut {
        sequence: "F4"
        enabled: resultsPane.resultCount > 0
        onActivated: resultsPane.selectNextResult()
    }

    Shortcut {
        sequence: "Shift+F4"
        enabled: resultsPane.resultCount > 0
        onActivated: resultsPane.selectPreviousResult()
    }

    Shortcut {
        sequence: "Esc"
        enabled: searchController.running
        onActivated: searchController.cancel()
    }

    CommandPalette {
        id: commandPalette

        parent: Overlay.overlay
        commands: root.commandPaletteItems
        onCommandTriggered: commandIndex => root.runPaletteCommand(commandIndex)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        SearchHeader {
            id: searchHeader

            directory: searchController.directory
            running: searchController.running
            cancelling: searchController.cancelling
            compact: root.compact
            resultCount: resultsPane.resultCount
            filesScanned: searchController.filesScanned
            timeToFirstResult: searchController.timeToFirstResult
            searchDuration: searchController.searchDuration
            regexAvailable: searchController.regexAvailable
            recentSearches: root.recentSearches
            savedSearches: root.savedSearches
            currentSearchSaved: root.isSearchSaved(queryText)
            onOpenCommandPalette: commandPalette.openPalette()
            onSelectDirectory: folderDialog.open()
            onCancelSearch: searchController.cancel()
            onSelectSearch: query => root.useStoredSearch(query)
            onToggleCurrentSearchSaved: root.toggleSavedSearch(searchHeader.queryText)
            onStartSearch: (
                query,
                regex,
                caseSensitive,
                wholeWord,
                gitignore,
                includeHidden,
                includeBinary,
                includeSubdirectories,
                documentTypes
            ) => {
                searchController.startSearch(
                    query,
                    regex,
                    caseSensitive,
                    wholeWord,
                    gitignore,
                    includeHidden,
                    includeBinary,
                    includeSubdirectories,
                    documentTypes
                )
                root.recordSearch(query)
            }
        }

        ScopeBar {
            directory: searchController.directory
            selectedDirectories: searchController.selectedDirectories
            includedDirectories: searchController.includedDirectories
            excludedDirectories: searchController.excludedDirectories
            recentDirectories: searchController.recentDirectories
            favoriteDirectories: searchController.favoriteDirectories
            currentDirectoryFavorite: searchController.currentDirectoryFavorite
            onSelectDirectory: path => searchController.selectSavedDirectory(path)
            onRemoveDirectory: path => searchController.removeSelectedDirectory(path)
            onAddIncludedDirectory: includedDirectoryDialog.open()
            onRemoveIncludedDirectory: (scopeRoot, relativePath) => searchController.removeIncludedDirectory(
                scopeRoot,
                relativePath
            )
            onAddExcludedDirectory: excludedDirectoryDialog.open()
            onRemoveExcludedDirectory: (scopeRoot, relativePath) => searchController.removeExcludedDirectory(
                scopeRoot,
                relativePath
            )
            onToggleCurrentFavorite: searchController.toggleCurrentDirectoryFavorite()
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: root.compact ? Qt.Vertical : Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 8
                implicitHeight: 8
                color: "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: root.compact ? 64 : 2
                    height: root.compact ? 2 : 64
                    radius: 1
                    color: Theme.border
                }
            }

            ResultsPane {
                id: resultsPane

                SplitView.preferredWidth: root.compact ? parent.width : root.resultsPanePreferredWidth
                SplitView.preferredHeight: root.compact ? root.resultsPanePreferredHeight : parent.height
                model: searchController.results
                onWidthChanged: {
                    if (!root.compact && width > 0)
                        root.resultsPanePreferredWidth = Math.round(width)
                }
                onHeightChanged: {
                    if (root.compact && height > 0)
                        root.resultsPanePreferredHeight = Math.round(height)
                }
                onResultsCleared: searchController.clearPreview()
                onResultSelected: (
                    filePath,
                    absolutePath,
                    location,
                    preview,
                    highlights
                ) => searchController.loadPreview(absolutePath, location, preview, highlights)
                onOpenFileRequested: filePath => searchController.openFile(filePath)
                onOpenWithRequested: filePath => searchController.openWith(filePath)
                onOpenFolderRequested: filePath => searchController.openContainingFolder(filePath)
                onCopyTextRequested: text => searchController.copyToClipboard(text)
            }

            PreviewPane {
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                filePath: searchController.previewFilePath
                location: searchController.previewLocation
                preview: searchController.previewText
                previewHtml: searchController.previewHtml
                loading: searchController.previewLoading
            }
        }

        StatusLine {
            status: searchController.status
            indexingStatus: searchController.indexingStatus
            running: searchController.running
            cancelling: searchController.cancelling
        }
    }

    Settings {
        id: mainWindowSettings

        category: "main-window"
        property string themeMode: "system"
        property string recentSearches: "[]"
        property string savedSearches: "[]"
        property alias windowX: root.x
        property alias windowY: root.y
        property alias windowWidth: root.width
        property alias windowHeight: root.height
        property alias resultsWidth: root.resultsPanePreferredWidth
        property alias resultsHeight: root.resultsPanePreferredHeight
        property alias documentTypes: searchHeader.documentTypes
        property alias regexEnabled: searchHeader.regexEnabled
        property alias caseSensitiveEnabled: searchHeader.caseSensitiveEnabled
        property alias wholeWordEnabled: searchHeader.wholeWordEnabled
        property alias respectGitignoreEnabled: searchHeader.respectGitignoreEnabled
        property alias includeHiddenEnabled: searchHeader.includeHiddenEnabled
        property alias includeBinaryEnabled: searchHeader.includeBinaryEnabled
        property alias includeSubdirectoriesEnabled: searchHeader.includeSubdirectoriesEnabled
    }

    Component.onCompleted: {
        Theme.mode = root.normalizeThemeMode(mainWindowSettings.themeMode)
        root.recentSearches = root.parseStoredList(mainWindowSettings.recentSearches)
        root.savedSearches = root.parseStoredList(mainWindowSettings.savedSearches)
    }

    Connections {
        target: Theme

        function onModeChanged() {
            mainWindowSettings.themeMode = Theme.mode
        }
    }
}
