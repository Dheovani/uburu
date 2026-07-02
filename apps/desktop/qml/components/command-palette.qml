import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Popup {
    id: root

    property var commands: []

    signal commandTriggered(int commandIndex)

    modal: true
    focus: true
    padding: 0
    width: Math.min(parent ? parent.width - 72 : 680, 680)
    height: Math.min(parent ? parent.height - 96 : 520, 520)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? 64 : 0

    function openPalette() {
        queryField.text = ""
        open()
        queryField.forceActiveFocus()
    }

    function filteredCommands() {
        const query = queryField.text.trim().toLowerCase()
        const matches = []

        for (let commandIndex = 0; commandIndex < commands.length; ++commandIndex) {
            const command = commands[commandIndex]
            const title = command.title || ""
            const description = command.description || ""
            const shortcut = command.shortcut || ""
            const searchableText = (title + " " + description + " " + shortcut).toLowerCase()

            if (query.length === 0 || searchableText.indexOf(query) !== -1) {
                matches.push({
                    title,
                    description,
                    shortcut,
                    available: command.available,
                    commandIndex
                })
            }
        }

        return matches
    }

    function triggerCurrentCommand() {
        const command = commandList.currentItem

        if (!command || !command.commandEnabled)
            return

        close()
        root.commandTriggered(command.commandIndex)
    }

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.surface
        border.color: Theme.borderStrong
        border.width: 1
    }

    Overlay.modal: Rectangle {
        color: "#99070a12"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                EyebrowLabel {
                    text: qsTr("COMANDOS")
                }

                Label {
                    text: qsTr("Paleta de comandos")
                    color: Theme.text
                    font.pixelSize: 20
                    font.bold: true
                }
            }

            Rectangle {
                Layout.preferredWidth: shortcutText.implicitWidth + 18
                Layout.preferredHeight: 26
                radius: 13
                color: Theme.surfaceRaised
                border.color: Theme.border
                border.width: 1

                Text {
                    id: shortcutText

                    anchors.centerIn: parent
                    text: qsTr("Ctrl+K")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeTiny
                    font.bold: true
                }
            }
        }

        TextField {
            id: queryField

            Layout.fillWidth: true
            Layout.preferredHeight: 42
            placeholderText: qsTr("Digite um comando")
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.text
            placeholderTextColor: Theme.textMuted
            selectionColor: Theme.primary
            selectedTextColor: "white"
            font.pixelSize: 15
            leftPadding: 14
            rightPadding: 14

            Keys.onDownPressed: {
                commandList.incrementCurrentIndex()
                event.accepted = true
            }

            Keys.onUpPressed: {
                commandList.decrementCurrentIndex()
                event.accepted = true
            }

            Keys.onReturnPressed: root.triggerCurrentCommand()
            Keys.onEnterPressed: root.triggerCurrentCommand()
            Keys.onEscapePressed: root.close()

            background: Rectangle {
                radius: Theme.radius
                color: Theme.surfaceSunken
                border.color: queryField.activeFocus ? Theme.primary : Theme.border
                border.width: 1
            }
        }

        ListView {
            id: commandList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: root.filteredCommands()
            currentIndex: count > 0 ? 0 : -1
            boundsBehavior: Flickable.StopAtBounds

            delegate: ItemDelegate {
                required property int index
                required property int commandIndex
                required property string title
                required property string description
                required property string shortcut
                required property bool available

                property bool commandEnabled: available

                width: commandList.width
                height: 58
                highlighted: ListView.isCurrentItem
                enabled: commandEnabled

                onClicked: {
                    commandList.currentIndex = index
                    root.triggerCurrentCommand()
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: title
                            color: commandEnabled ? Theme.text : Theme.textFaint
                            font.pixelSize: Theme.fontSize
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: description
                            color: commandEnabled ? Theme.textMuted : Theme.textFaint
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                    }

                    Text {
                        visible: shortcut.length > 0
                        text: shortcut
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontSizeTiny
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                background: Rectangle {
                    radius: Theme.radius
                    color: highlighted ? Theme.primarySoft : hovered ? Theme.surfaceRaised : Theme.surfaceSunken
                    border.color: highlighted ? Theme.primary : Theme.border
                    border.width: 1
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: commandList.count === 0
                color: "transparent"

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Nenhum comando encontrado")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }
    }
}
