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
            title: qsTr("Abrir resultado selecionado"),
            description: qsTr("Abrir o arquivo do resultado atual"),
            shortcut: qsTr("Enter"),
            available: resultsPane.hasCurrentResult()
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
            resultsPane.openCurrentResult()
            return
        case 6:
            resultsPane.copyCurrentPath()
            return
        case 7:
            resultsPane.copyCurrentOccurrence()
            return
        default:
            return
        }
    }

    FolderDialog {
        id: folderDialog

        title: qsTr("Selecionar diretório ou repositório")
        onAccepted: searchController.selectDirectory(selectedFolder.toString())
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
            compact: root.compact
            resultCount: resultsPane.resultCount
            filesScanned: searchController.filesScanned
            timeToFirstResult: searchController.timeToFirstResult
            searchDuration: searchController.searchDuration
            regexAvailable: searchController.regexAvailable
            onSelectDirectory: folderDialog.open()
            onCancelSearch: searchController.cancel()
            onStartSearch: (
                query,
                regex,
                caseSensitive,
                wholeWord,
                gitignore,
                includeSubdirectories,
                documentTypes
            ) => {
                searchController.startSearch(
                    query,
                    regex,
                    caseSensitive,
                    wholeWord,
                    gitignore,
                    includeSubdirectories,
                    documentTypes
                )
            }
        }

        ScopeBar {
            directory: searchController.directory
            recentDirectories: searchController.recentDirectories
            favoriteDirectories: searchController.favoriteDirectories
            currentDirectoryFavorite: searchController.currentDirectoryFavorite
            onSelectDirectory: path => searchController.selectSavedDirectory(path)
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
                onResultSelected: (filePath, absolutePath, location, preview, highlights) => searchController.loadPreview(
                    absolutePath,
                    location,
                    preview,
                    highlights
                )
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
            running: searchController.running
        }
    }

    Settings {
        category: "main-window"
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
        property alias includeSubdirectoriesEnabled: searchHeader.includeSubdirectoriesEnabled
    }
}
