import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Rectangle {
    id: root

    property string directory: ""
    property var selectedDirectories: []
    property var includedDirectories: []
    property var excludedDirectories: []
    property var recentDirectories: []
    property var favoriteDirectories: []
    property bool currentDirectoryFavorite: false
    property bool hasSavedScopes: recentDirectories.length > 0 || favoriteDirectories.length > 0
    property bool hasSelectedScopes: selectedDirectories.length > 0
    property bool hasIncludedScopes: includedDirectories.length > 0
    property bool hasExcludedScopes: excludedDirectories.length > 0

    signal selectDirectory(string path)
    signal removeDirectory(string path)
    signal addIncludedDirectory()
    signal removeIncludedDirectory(string root, string relativePath)
    signal addExcludedDirectory()
    signal removeExcludedDirectory(string root, string relativePath)
    signal toggleCurrentFavorite()

    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight

    implicitHeight: contentColumn.implicitHeight + 16

    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    function shortPath(path) {
        if (path.length <= 42)
            return path

        const parts = path.replace(/\\/g, "/").split("/")

        if (parts.length <= 2)
            return path

        return parts[0] + "/…/" + parts[parts.length - 1]
    }

    function scopeEntryRoot(entry) {
        return entry && entry.scopeRoot ? entry.scopeRoot : ""
    }

    function scopeEntryRelativePath(entry) {
        return entry && entry.relativePath ? entry.relativePath : ""
    }

    function scopeEntryDisplayPath(entry) {
        const absolutePath = entry && entry.absolutePath ? entry.absolutePath : ""

        if (absolutePath.length > 0)
            return absolutePath

        return root.scopeEntryRelativePath(entry)
    }

    ColumnLayout {
        id: contentColumn

        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: qsTr("Escopo")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeTiny
                font.bold: true
            }

            InfoIcon {
                text: qsTr("Define as pastas usadas na busca. Favoritos e recentes ajudam a alternar escopos.")
            }

            Label {
                Layout.fillWidth: true
                text: root.selectedDirectories.length > 1
                      ? qsTr("%1 diretórios selecionados").arg(root.selectedDirectories.length)
                      : root.directory.length > 0
                        ? root.directory
                        : qsTr("Nenhum diretório selecionado")
                color: root.hasSelectedScopes ? Theme.text : Theme.textMuted
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideLeft
            }

            Rectangle {
                Layout.preferredHeight: 24
                Layout.preferredWidth: favoriteLabel.implicitWidth + 20
                radius: 12
                color: favoriteMouseArea.containsMouse ? Theme.surfaceRaised : Theme.surfaceSunken
                border.color: root.currentDirectoryFavorite ? Theme.warning : Theme.border
                border.width: 1
                visible: root.directory.length > 0
                Accessible.role: Accessible.Button
                Accessible.name: root.currentDirectoryFavorite ? qsTr("Remover favorito") : qsTr("Adicionar favorito")
                Accessible.description: root.directory

                Text {
                    id: favoriteLabel

                    anchors.centerIn: parent
                    text: root.currentDirectoryFavorite ? qsTr("★ Favorito") : qsTr("☆ Favoritar")
                    color: root.currentDirectoryFavorite ? Theme.warning : Theme.textMuted
                    font.pixelSize: Theme.fontSizeTiny
                    font.bold: true
                }

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }

                ToolTip.visible: favoriteMouseArea.containsMouse
                ToolTip.delay: 450
                ToolTip.timeout: 7000
                ToolTip.text: root.currentDirectoryFavorite
                              ? qsTr("Remover este diretório dos favoritos.")
                              : qsTr("Adicionar este diretório aos favoritos.")

                MouseArea {
                    id: favoriteMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.toggleCurrentFavorite()
                }
            }

            Rectangle {
                Layout.preferredHeight: 24
                Layout.preferredWidth: includeLabel.implicitWidth + 20
                radius: 12
                color: includeMouseArea.containsMouse ? Theme.surfaceRaised : Theme.surfaceSunken
                border.color: Theme.border
                border.width: 1
                visible: root.hasSelectedScopes
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Incluir subpasta")

                Text {
                    id: includeLabel

                    anchors.centerIn: parent
                    text: qsTr("Incluir")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeTiny
                    font.bold: true
                }

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }

                ToolTip.visible: includeMouseArea.containsMouse
                ToolTip.delay: 450
                ToolTip.timeout: 7000
                ToolTip.text: qsTr("Escolha uma subpasta de um escopo selecionado para pesquisar apenas nela.")

                MouseArea {
                    id: includeMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.addIncludedDirectory()
                }
            }

            Rectangle {
                Layout.preferredHeight: 24
                Layout.preferredWidth: excludeLabel.implicitWidth + 20
                radius: 12
                color: excludeMouseArea.containsMouse ? Theme.surfaceRaised : Theme.surfaceSunken
                border.color: Theme.border
                border.width: 1
                visible: root.hasSelectedScopes
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Ignorar subpasta")

                Text {
                    id: excludeLabel

                    anchors.centerIn: parent
                    text: qsTr("Ignorar")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeTiny
                    font.bold: true
                }

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }

                ToolTip.visible: excludeMouseArea.containsMouse
                ToolTip.delay: 450
                ToolTip.timeout: 7000
                ToolTip.text: qsTr("Escolha uma subpasta de um escopo selecionado para ignorar.")

                MouseArea {
                    id: excludeMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.addExcludedDirectory()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.hasSelectedScopes
            spacing: 12

            Label {
                text: qsTr("Selecionados")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeTiny
                font.bold: true
            }

            Flickable {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                contentWidth: selectedChipsRow.implicitWidth
                contentHeight: height
                clip: true
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Diretórios selecionados")

                Row {
                    id: selectedChipsRow

                    spacing: 6
                    height: parent.height

                    Repeater {
                        model: root.selectedDirectories

                        delegate: Rectangle {
                            height: 26
                            width: Math.min(
                                260,
                                selectedScopeText.implicitWidth + removeSelectedScopeText.implicitWidth + 28
                            )
                            radius: 13
                            color: selectedScopeMouseArea.containsMouse
                                   ? Theme.surfaceRaised
                                   : Theme.surfaceSunken
                            border.color: modelData === root.directory ? Theme.primary : Theme.border
                            border.width: 1
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Remover diretório selecionado: %1").arg(modelData)

                            Row {
                                anchors.centerIn: parent
                                spacing: 6

                                Text {
                                    id: selectedScopeText

                                    width: Math.min(210, implicitWidth)
                                    text: root.shortPath(modelData)
                                    color: Theme.text
                                    font.pixelSize: Theme.fontSizeTiny
                                    elide: Text.ElideLeft
                                }

                                Text {
                                    id: removeSelectedScopeText

                                    text: qsTr("Remover")
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeTiny
                                }
                            }

                            HoverHandler {
                                cursorShape: Qt.PointingHandCursor
                            }

                            MouseArea {
                                id: selectedScopeMouseArea

                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: root.removeDirectory(modelData)
                            }
                        }
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            visible: root.hasIncludedScopes
                    || root.hasExcludedScopes
                    || root.hasSavedScopes

            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            ScopeChipRow {
                Layout.fillWidth: true
                Layout.preferredWidth: (parent.width - parent.columnSpacing) / 2
                visible: root.hasIncludedScopes

                title: qsTr("Incluídos")
                emptyText: qsTr("Nenhuma subpasta incluída")
                entries: root.includedDirectories
                removeAccessibleTemplate: qsTr("Remover subpasta incluída: %1")
                onRemoveRequested: (scopeRoot, relativePath) =>
                    root.removeIncludedDirectory(scopeRoot, relativePath)
            }

            ScopeChipRow {
                Layout.fillWidth: true
                Layout.preferredWidth: (parent.width - parent.columnSpacing) / 2
                visible: root.hasExcludedScopes

                title: qsTr("Ignorados")
                emptyText: qsTr("Nenhuma subpasta ignorada")
                entries: root.excludedDirectories
                removeAccessibleTemplate: qsTr("Remover subpasta ignorada: %1")
                onRemoveRequested: (scopeRoot, relativePath) =>
                    root.removeExcludedDirectory(scopeRoot, relativePath)
            }

            SavedScopeRow {
                Layout.fillWidth: true
                Layout.preferredWidth: (parent.width - parent.columnSpacing) / 2
                visible: root.favoriteDirectories.length > 0

                title: qsTr("Favoritos")
                directories: root.favoriteDirectories
                emptyText: qsTr("Nenhum favorito")
                onSelected: path => root.selectDirectory(path)
            }

            SavedScopeRow {
                Layout.fillWidth: true
                Layout.preferredWidth: (parent.width - parent.columnSpacing) / 2
                visible: root.recentDirectories.length > 0

                title: qsTr("Recentes")
                directories: root.recentDirectories
                emptyText: qsTr("Nenhum recente")
                onSelected: path => root.selectDirectory(path)
            }
        }
    }

    component ScopeChipRow: ColumnLayout {
        id: scopeChipRow

        Layout.fillWidth: true
        Layout.alignment: Qt.AlignLeft

        property string title: ""
        property string emptyText: ""
        property string removeAccessibleTemplate: ""
        property var entries: []

        signal removeRequested(string scopeRoot, string relativePath)

        spacing: 4

        Label {
            text: scopeChipRow.title
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeTiny
            font.bold: true
        }

        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            contentWidth: ignoredChipsRow.implicitWidth
            contentHeight: height
            clip: true
            Accessible.role: Accessible.List
            Accessible.name: scopeChipRow.title

            Row {
                id: ignoredChipsRow

                spacing: 6
                height: parent.height

                Repeater {
                    model: scopeChipRow.entries

                    delegate: Rectangle {
                        required property var modelData

                        height: 26
                        width: Math.min(
                            260,
                            ignoredScopeText.implicitWidth + removeIgnoredScopeText.implicitWidth + 28
                        )
                        radius: 13
                        color: ignoredScopeMouseArea.containsMouse
                               ? Theme.surfaceRaised
                               : Theme.surfaceSunken
                        border.color: Theme.border
                        border.width: 1
                        Accessible.role: Accessible.Button
                        Accessible.name: scopeChipRow.removeAccessibleTemplate.arg(
                            root.scopeEntryDisplayPath(modelData)
                        )

                        Row {
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                id: ignoredScopeText

                                width: Math.min(210, implicitWidth)
                                text: root.shortPath(root.scopeEntryDisplayPath(modelData))
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeTiny
                                elide: Text.ElideLeft
                            }

                            Text {
                                id: removeIgnoredScopeText

                                text: qsTr("Remover")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeTiny
                            }
                        }

                        HoverHandler {
                            cursorShape: Qt.PointingHandCursor
                        }

                        MouseArea {
                            id: ignoredScopeMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: scopeChipRow.removeRequested(
                                root.scopeEntryRoot(modelData),
                                root.scopeEntryRelativePath(modelData)
                            )
                        }
                    }
                }

                Text {
                    height: parent.height
                    text: scopeChipRow.emptyText
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSizeTiny
                    verticalAlignment: Text.AlignVCenter
                    visible: scopeChipRow.entries.length === 0
                }
            }
        }
    }

    component SavedScopeRow: ColumnLayout {
        id: savedScopeRow

        Layout.fillWidth: true
        Layout.alignment: Qt.AlignLeft

        property string title: ""
        property string emptyText: ""
        property var directories: []

        signal selected(string path)

        spacing: 4

        Label {
            text: savedScopeRow.title
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeTiny
            font.bold: true
        }

        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            contentWidth: chipsRow.implicitWidth
            contentHeight: height
            clip: true
            Accessible.role: Accessible.List
            Accessible.name: savedScopeRow.title

            Row {
                id: chipsRow

                spacing: 6
                height: parent.height

                Repeater {
                    model: savedScopeRow.directories

                    delegate: Rectangle {
                        required property string modelData

                        height: 26
                        width: Math.min(220, scopeText.implicitWidth + 20)
                        radius: 13
                        color: scopeMouseArea.containsMouse
                               ? Theme.surfaceRaised
                               : Theme.surfaceSunken
                        border.color: Theme.border
                        border.width: 1
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData

                        Text {
                            id: scopeText

                            anchors.centerIn: parent
                            width: parent.width - 18
                            text: root.shortPath(modelData)
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeTiny
                            elide: Text.ElideLeft
                            horizontalAlignment: Text.AlignHCenter
                        }

                        HoverHandler {
                            cursorShape: Qt.PointingHandCursor
                        }

                        MouseArea {
                            id: scopeMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: savedScopeRow.selected(modelData)
                        }
                    }
                }

                Text {
                    height: parent.height
                    text: savedScopeRow.emptyText
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontSizeTiny
                    verticalAlignment: Text.AlignVCenter
                    visible: savedScopeRow.directories.length === 0
                }
            }
        }
    }
}