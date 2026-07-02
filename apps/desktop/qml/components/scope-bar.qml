import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Rectangle {
    id: root

    property string directory: ""
    property var recentDirectories: []
    property var favoriteDirectories: []
    property bool currentDirectoryFavorite: false
    property bool hasSavedScopes: recentDirectories.length > 0 || favoriteDirectories.length > 0

    signal selectDirectory(string path)
    signal toggleCurrentFavorite()

    Layout.fillWidth: true
    Layout.preferredHeight: hasSavedScopes ? 108 : 38
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

    ColumnLayout {
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

            Label {
                Layout.fillWidth: true
                text: root.directory.length > 0 ? root.directory : qsTr("Nenhum diretório selecionado")
                color: root.directory.length > 0 ? Theme.text : Theme.textMuted
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideMiddle
            }

            Rectangle {
                Layout.preferredHeight: 24
                Layout.preferredWidth: favoriteLabel.implicitWidth + 20
                radius: 12
                color: favoriteMouseArea.containsMouse ? Theme.surfaceRaised : Theme.surfaceSunken
                border.color: root.currentDirectoryFavorite ? Theme.warning : Theme.border
                border.width: 1
                visible: root.directory.length > 0

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

                MouseArea {
                    id: favoriteMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.toggleCurrentFavorite()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.hasSavedScopes
            spacing: 12

            SavedScopeRow {
                Layout.fillWidth: true
                title: qsTr("Favoritos")
                directories: root.favoriteDirectories
                emptyText: qsTr("Nenhum favorito")
                onSelected: path => root.selectDirectory(path)
            }

            SavedScopeRow {
                Layout.fillWidth: true
                title: qsTr("Recentes")
                directories: root.recentDirectories
                emptyText: qsTr("Nenhum recente")
                onSelected: path => root.selectDirectory(path)
            }
        }
    }

    component SavedScopeRow: ColumnLayout {
        id: savedScopeRow

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

            Row {
                id: chipsRow

                spacing: 6
                height: parent.height

                Repeater {
                    model: savedScopeRow.directories

                    delegate: Rectangle {
                        height: 26
                        width: Math.min(220, scopeText.implicitWidth + 20)
                        radius: 13
                        color: scopeMouseArea.containsMouse ? Theme.surfaceRaised : Theme.surfaceSunken
                        border.color: Theme.border
                        border.width: 1

                        Text {
                            id: scopeText

                            anchors.centerIn: parent
                            width: parent.width - 18
                            text: root.shortPath(modelData)
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeTiny
                            elide: Text.ElideMiddle
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
