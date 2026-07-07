pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Popup {
    id: root

    property string activeSection: "general"
    property string themeMode: "system"
    property string languageMode: "system"

    signal themeModeSelected(string mode)
    signal languageModeSelected(string mode)
    signal copyDiagnosticsRequested()
    signal resetLayoutRequested()

    function openGeneral() {
        activeSection = "general"
        open()
    }

    function openLanguage() {
        activeSection = "language"
        open()
    }

    function openPrivacy() {
        activeSection = "privacy"
        open()
    }

    parent: Overlay.overlay
    width: Math.min(720, parent ? parent.width - 48 : 720)
    height: Math.min(500, parent ? parent.height - 48 : 500)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.surface
        border.color: Theme.borderStrong
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            Layout.leftMargin: 20
            Layout.rightMargin: 14
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: qsTr("Configurações")
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeTitle
                    font.bold: true
                }

                Text {
                    text: qsTr("Preferências locais do Uburu")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeTiny
                }
            }

            Button {
                implicitWidth: 34
                implicitHeight: 30
                hoverEnabled: true
                text: "×"
                Accessible.name: qsTr("Fechar configurações")
                onClicked: root.close()

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }

                contentItem: Text {
                    text: parent.text
                    color: parent.hovered ? Theme.text : Theme.textMuted
                    font.pixelSize: Theme.fontSizeTitle
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 8
                    color: parent.hovered ? Theme.surfaceRaised : "transparent"
                    border.color: parent.hovered ? Theme.border : "transparent"
                    border.width: 1
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ColumnLayout {
                Layout.preferredWidth: 178
                Layout.minimumWidth: 178
                Layout.maximumWidth: 178
                Layout.fillHeight: true
                Layout.margins: 12
                spacing: 6

                SettingsNavButton {
                    text: qsTr("Geral")
                    selected: root.activeSection === "general"
                    onClicked: root.activeSection = "general"
                }

                SettingsNavButton {
                    text: qsTr("Idioma")
                    selected: root.activeSection === "language"
                    onClicked: root.activeSection = "language"
                }

                SettingsNavButton {
                    text: qsTr("Privacidade")
                    selected: root.activeSection === "privacy"
                    onClicked: root.activeSection = "privacy"
                }

                Item {
                    Layout.fillHeight: true
                }

                SettingsNavButton {
                    text: qsTr("Diagnóstico rápido")
                    selected: false
                    onClicked: root.copyDiagnosticsRequested()
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: Theme.border
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 360
                contentWidth: width
                contentHeight: contentColumn.implicitHeight + 32
                clip: true

                ColumnLayout {
                    id: contentColumn

                    width: parent.width
                    spacing: 14

                    SettingsSection {
                        visible: root.activeSection === "general"
                        title: qsTr("Preferências gerais")
                        description: qsTr("Ajustes globais aplicados à experiência local.")

                        SettingsOptionGroup {
                            title: qsTr("Tema")
                            description: qsTr("Escolha a aparência do aplicativo.")

                            SettingsChoiceRow {
                                currentValue: root.themeMode
                                choices: [
                                    { label: qsTr("Sistema"), value: "system" },
                                    { label: qsTr("Escuro"), value: "dark" },
                                    { label: qsTr("Claro"), value: "light" }
                                ]
                                onValueSelected: value => root.themeModeSelected(value)
                            }
                        }

                        SettingsOptionGroup {
                            title: qsTr("Layout")
                            description: qsTr("Restaura o tamanho padrão dos painéis de resultados e preview.")

                            SettingsActionButton {
                                text: qsTr("Restaurar layout")
                                onClicked: root.resetLayoutRequested()
                            }
                        }
                    }

                    SettingsSection {
                        visible: root.activeSection === "language"
                        title: qsTr("Idioma")
                        description: qsTr("Escolha o idioma da interface. A alteração é aplicada ao reiniciar o Uburu.")

                        SettingsOptionGroup {
                            title: qsTr("Idioma da interface")
                            description: qsTr("Use o idioma do sistema ou selecione manualmente.")

                            SettingsChoiceRow {
                                currentValue: root.languageMode
                                choices: [
                                    { label: qsTr("Sistema"), value: "system" },
                                    { label: qsTr("Português (Brasil)"), value: "pt-BR" },
                                    { label: qsTr("English (US)"), value: "en-US" }
                                ]
                                onValueSelected: value => root.languageModeSelected(value)
                            }
                        }
                    }

                    SettingsSection {
                        visible: root.activeSection === "privacy"
                        title: qsTr("Privacidade")
                        description: qsTr("Uburu mantém buscas, índice e diagnósticos localmente, salvo exportação explícita.")

                        SettingsOptionGroup {
                            title: qsTr("Diagnóstico")
                            description: qsTr("Copia um resumo técnico da busca atual sem enviar dados pela rede.")

                            SettingsActionButton {
                                text: qsTr("Copiar diagnóstico")
                                onClicked: root.copyDiagnosticsRequested()
                            }
                        }
                    }
                }
            }
        }
    }

    component SettingsSection: ColumnLayout {
        id: section

        required property string title
        property string description: ""
        default property alias sectionContent: sectionBody.data

        Layout.fillWidth: true
        Layout.leftMargin: 20
        Layout.rightMargin: 20
        Layout.topMargin: 18
        spacing: 12

        Text {
            text: section.title
            color: Theme.text
            font.pixelSize: Theme.fontSizeTitle
            font.bold: true
        }

        Text {
            Layout.fillWidth: true
            text: section.description
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            id: sectionBody

            Layout.fillWidth: true
            spacing: 10
        }
    }

    component SettingsOptionGroup: Rectangle {
        id: group

        required property string title
        property string description: ""
        default property alias groupContent: groupBody.data

        Layout.fillWidth: true
        implicitHeight: groupLayout.implicitHeight + 24
        radius: Theme.radius
        color: Theme.surfaceRaised
        border.color: Theme.border
        border.width: 1

        ColumnLayout {
            id: groupLayout

            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            Text {
                text: group.title
                color: Theme.text
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: group.description
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeTiny
                wrapMode: Text.WordWrap
            }

            RowLayout {
                id: groupBody

                Layout.fillWidth: true
                spacing: 8
            }
        }
    }

    component SettingsChoiceRow: RowLayout {
        id: choiceRow

        required property string currentValue
        required property var choices

        signal valueSelected(string value)

        Repeater {
            model: choiceRow.choices

            delegate: SettingsActionButton {
                required property var modelData

                text: modelData.label
                selected: choiceRow.currentValue === modelData.value
                onClicked: choiceRow.valueSelected(modelData.value)
            }
        }
    }

    component SettingsActionButton: Button {
        id: actionButton

        property bool selected: false

        implicitHeight: 32
        implicitWidth: Math.max(110, contentText.implicitWidth + 26)
        hoverEnabled: true

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        contentItem: Text {
            id: contentText

            text: actionButton.text
            color: actionButton.selected || actionButton.hovered ? Theme.text : Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 9
            color: actionButton.selected ? Theme.primarySoft : actionButton.hovered ? Theme.surface : "transparent"
            border.color: actionButton.selected ? Theme.primary : Theme.border
            border.width: 1
        }
    }

    component SettingsNavButton: Button {
        id: navButton

        property bool selected: false

        Layout.fillWidth: true
        implicitHeight: 34
        hoverEnabled: true

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        contentItem: Text {
            text: navButton.text
            color: navButton.selected || navButton.hovered ? Theme.text : Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
        }

        background: Rectangle {
            radius: 9
            color: navButton.selected ? Theme.primarySoft : navButton.hovered ? Theme.surfaceRaised : "transparent"
            border.color: navButton.selected ? Theme.primary : "transparent"
            border.width: 1
        }
    }
}
