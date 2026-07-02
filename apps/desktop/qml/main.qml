import QtQuick
import QtQuick.Controls
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

    FolderDialog {
        id: folderDialog

        title: qsTr("Selecionar diretório ou repositório")
        onAccepted: searchController.selectDirectory(selectedFolder.toString())
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        SearchHeader {
            directory: searchController.directory
            running: searchController.running
            compact: root.compact
            resultCount: resultsPane.resultCount
            filesScanned: searchController.filesScanned
            timeToFirstResult: searchController.timeToFirstResult
            searchDuration: searchController.searchDuration
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

                SplitView.preferredWidth: root.compact ? parent.width : 430
                SplitView.preferredHeight: root.compact ? 260 : parent.height
                model: searchController.results
                onResultsCleared: searchController.clearPreview()
                onResultSelected: (filePath, absolutePath, location, preview) => searchController.loadPreview(
                    absolutePath,
                    location,
                    preview
                )
                onOpenFileRequested: filePath => searchController.openFile(filePath)
                onOpenFolderRequested: filePath => searchController.openContainingFolder(filePath)
                onCopyTextRequested: text => searchController.copyToClipboard(text)
            }

            PreviewPane {
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                filePath: searchController.previewFilePath
                location: searchController.previewLocation
                preview: searchController.previewText
                loading: searchController.previewLoading
            }
        }

        StatusLine {
            status: searchController.status
            running: searchController.running
        }
    }
}
