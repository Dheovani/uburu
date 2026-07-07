pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Rectangle {
    id: root

    property bool canSearch: false
    property bool canCancelSearch: false
    property bool canIndex: false
    property bool canCancelIndexing: false
    property bool hasDirectory: false
    property bool hasQuery: false
    property bool hasCurrentResult: false
    property string themeMode: "system"

    signal actionTriggered(string action)

    Layout.fillWidth: true
    Layout.preferredHeight: 20
    color: "transparent"

    function openMenu(anchor, items) {
        menuPopup.activeItems = items
        menuPopup.x = anchor.x
        menuPopup.y = anchor.y + anchor.height + 3
        menuPopup.open()
    }

    function fileItems() {
        return [
            { text: qsTr("Selecionar pasta..."), shortcut: qsTr("Ctrl+O"), enabled: true, action: "selectFolder" },
            { text: qsTr("Reindexar escopo"), shortcut: qsTr("Ctrl+Alt+I"), enabled: canIndex, action: "reindexScope" },
            { text: qsTr("Cancelar indexação"), shortcut: qsTr("Ctrl+Alt+Esc"), enabled: canCancelIndexing, action: "cancelIndexing" },
            { separator: true },
            { text: qsTr("Sair"), shortcut: qsTr("Alt+F4"), enabled: true, action: "quit" }
        ]
    }

    function editItems() {
        return [
            { text: qsTr("Focar busca"), shortcut: qsTr("Ctrl+F"), enabled: true, action: "focusSearch" },
            { text: qsTr("Salvar busca atual"), shortcut: qsTr("Ctrl+S"), enabled: hasQuery, action: "toggleSavedSearch" },
            { text: qsTr("Favoritar pasta atual"), shortcut: qsTr("Ctrl+D"), enabled: hasDirectory, action: "toggleFavoriteDirectory" },
            { separator: true },
            { text: qsTr("Copiar diagnóstico"), shortcut: "", enabled: true, action: "copyDiagnostics" }
        ]
    }

    function searchItems() {
        return [
            { text: qsTr("Iniciar busca"), shortcut: qsTr("Enter"), enabled: canSearch, action: "startSearch" },
            { text: qsTr("Cancelar busca"), shortcut: qsTr("Esc"), enabled: canCancelSearch, action: "cancelSearch" },
            { separator: true },
            { text: qsTr("Abrir resultado selecionado"), shortcut: qsTr("Enter"), enabled: hasCurrentResult, action: "openCurrentResult" },
            { text: qsTr("Próxima ocorrência"), shortcut: qsTr("F4"), enabled: hasCurrentResult, action: "nextResult" },
            { text: qsTr("Ocorrência anterior"), shortcut: qsTr("Shift+F4"), enabled: hasCurrentResult, action: "previousResult" }
        ]
    }

    function viewItems() {
        return [
            { text: qsTr("Paleta de comandos"), shortcut: qsTr("Ctrl+K"), enabled: true, action: "openCommandPalette" },
            { separator: true },
            { text: qsTr("Tema do sistema"), checked: themeMode === "system", enabled: true, action: "themeSystem" },
            { text: qsTr("Tema escuro"), checked: themeMode === "dark", enabled: true, action: "themeDark" },
            { text: qsTr("Tema claro"), checked: themeMode === "light", enabled: true, action: "themeLight" }
        ]
    }

    function settingsItems() {
        return [
            { text: qsTr("Preferências gerais"), shortcut: "", enabled: false, action: "generalPreferences" },
            { text: qsTr("Idioma"), shortcut: "", enabled: false, action: "languagePreferences" },
            { text: qsTr("Privacidade e histórico"), shortcut: "", enabled: false, action: "privacyPreferences" },
            { separator: true },
            { text: qsTr("Diagnóstico rápido"), shortcut: "", enabled: true, action: "copyDiagnostics" }
        ]
    }

    function helpItems() {
        return [
            { text: qsTr("Mostrar comandos"), shortcut: qsTr("Ctrl+K"), enabled: true, action: "openCommandPalette" },
            { text: qsTr("Copiar diagnóstico"), shortcut: "", enabled: true, action: "copyDiagnostics" }
        ]
    }

    Row {
        anchors.fill: parent
        spacing: 2

        MenuButton {
            id: fileButton

            text: qsTr("Arquivo")
            onClicked: root.openMenu(fileButton, root.fileItems())
        }

        MenuButton {
            id: editButton

            text: qsTr("Editar")
            onClicked: root.openMenu(editButton, root.editItems())
        }

        MenuButton {
            id: searchButton

            text: qsTr("Busca")
            onClicked: root.openMenu(searchButton, root.searchItems())
        }

        MenuButton {
            id: viewButton

            text: qsTr("Exibir")
            onClicked: root.openMenu(viewButton, root.viewItems())
        }

        MenuButton {
            id: settingsButton

            text: qsTr("Configurações")
            onClicked: root.openMenu(settingsButton, root.settingsItems())
        }

        MenuButton {
            id: helpButton

            text: qsTr("Ajuda")
            onClicked: root.openMenu(helpButton, root.helpItems())
        }
    }

    Popup {
        id: menuPopup

        property var activeItems: []

        width: 278
        padding: 6
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.borderStrong
            border.width: 1
        }

        contentItem: Column {
            spacing: 4

            Repeater {
                model: menuPopup.activeItems

                delegate: Item {
                    required property var modelData

                    width: menuPopup.width - menuPopup.leftPadding - menuPopup.rightPadding
                    height: modelData.separator ? 9 : 32

                    Rectangle {
                        visible: modelData.separator === true
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: 1
                        color: Theme.border
                    }

                    Rectangle {
                        id: menuItemBackground

                        visible: modelData.separator !== true
                        anchors.fill: parent
                        radius: 8
                        color: menuMouseArea.containsMouse && modelData.enabled ? Theme.surfaceRaised : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 7
                                Layout.preferredHeight: 7
                                radius: 4
                                visible: modelData.checked === true
                                color: Theme.primary
                            }

                            Item {
                                Layout.preferredWidth: 7
                                Layout.preferredHeight: 7
                                visible: modelData.checked !== true
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.text || ""
                                color: modelData.enabled ? Theme.text : Theme.textFaint
                                font.pixelSize: Theme.fontSizeSmall
                                elide: Text.ElideRight
                            }

                            Text {
                                text: modelData.shortcut || ""
                                visible: (modelData.shortcut || "").length > 0
                                color: modelData.enabled ? Theme.textMuted : Theme.textFaint
                                font.pixelSize: Theme.fontSizeTiny
                            }
                        }

                        HoverHandler {
                            cursorShape: modelData.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }

                        MouseArea {
                            id: menuMouseArea

                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: modelData.enabled === true
                            onClicked: {
                                menuPopup.close()
                                root.actionTriggered(modelData.action || "")
                            }
                        }
                    }
                }
            }
        }
    }

    component MenuButton: Button {
        id: menuButton

        implicitHeight: 28
        implicitWidth: contentText.implicitWidth + 22
        hoverEnabled: true

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        contentItem: Text {
            id: contentText

            text: menuButton.text
            color: menuButton.hovered ? Theme.text : Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }

        background: Rectangle {
            radius: 7
            color: menuButton.down || menuButton.hovered ? Theme.surfaceRaised : "transparent"
            border.color: menuButton.down || menuButton.hovered ? Theme.border : "transparent"
            border.width: 1
        }
    }
}
