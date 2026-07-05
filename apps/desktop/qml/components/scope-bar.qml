pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Item {
    id: root

    property string directory: ""
    property var selectedDirectories: []
    property var includedDirectories: []
    property var excludedDirectories: []
    property var recentDirectories: []
    property var favoriteDirectories: []
    property bool currentDirectoryFavorite: false
    property bool hasSavedScopes: recentDirectories.length > 0 || favoriteDirectories.length > 0

    signal selectDirectory(string path)
    signal browseDirectory()
    signal removeDirectory(string path)
    signal addIncludedDirectory()
    signal removeIncludedDirectory(string root, string relativePath)
    signal addExcludedDirectory()
    signal removeExcludedDirectory(string root, string relativePath)
    signal toggleCurrentFavorite()
    signal toggleFavorite(string path)

    Layout.fillWidth: true
    Layout.preferredHeight: 40

    function shortPath(path) {
        if (path.length <= 78)
            return path

        const parts = path.replace(/\\/g, "/").split("/")

        if (parts.length <= 2)
            return path

        return parts[0] + "/.../" + parts[parts.length - 1]
    }

    function isFavorite(path) {
        return root.favoriteDirectories.indexOf(path) !== -1
    }

    function scopeFilterText(entries, prefix) {
        const paths = []

        for (const entry of entries) {
            if (entry.scopeRoot !== root.directory)
                continue

            paths.push(prefix + entry.absolutePath)
        }

        return paths
    }

    function scopeDisplayText() {
        if (root.directory.length === 0)
            return ""

        const scopedPaths = scopeFilterText(root.includedDirectories, "+")
                            .concat(scopeFilterText(root.excludedDirectories, "-"))

        if (scopedPaths.length === 0)
            return root.directory

        return root.directory + " (" + scopedPaths.join(",") + ")"
    }

    function editableScopePath(text) {
        const normalizedText = text.trim()
        const generatedText = root.scopeDisplayText()

        if (generatedText.length > 0 && normalizedText === generatedText)
            return root.directory

        if (root.directory.length > 0 && normalizedText.startsWith(root.directory + " ("))
            return root.directory

        return normalizedText
    }

    function refreshScopeText() {
        scopeField.text = root.scopeDisplayText()
    }

    function acceptScopeText() {
        const path = root.editableScopePath(scopeField.text)

        if (path.length === 0)
            return

        scopePopup.close()
        root.selectDirectory(path)
    }

    function useScope(path) {
        scopeField.text = path
        scopePopup.close()
        root.selectDirectory(path)
    }

    onDirectoryChanged: root.refreshScopeText()
    onIncludedDirectoriesChanged: root.refreshScopeText()
    onExcludedDirectoriesChanged: root.refreshScopeText()

    RowLayout {
        anchors.fill: parent
        spacing: 10

        MutedLabel {
            text: qsTr("Escopo")
            width: 60
        }

        InfoIcon {
            text: qsTr("Define a pasta usada na busca. Digite um caminho, pressione Enter ou escolha favoritos e recentes.")
        }

        TextField {
            id: scopeField

            Layout.fillWidth: true
            Layout.preferredHeight: 34
            text: root.scopeDisplayText()
            placeholderText: qsTr("Escolha ou digite um escopo")
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true
            color: Theme.text
            placeholderTextColor: Theme.textMuted
            selectionColor: Theme.primary
            selectedTextColor: "white"
            font.pixelSize: Theme.fontSizeSmall
            leftPadding: 12
            rightPadding: 126
            Accessible.name: qsTr("Escopo de busca")
            Accessible.description: qsTr("Digite um caminho ou escolha uma pasta recente ou favorita.")
            onAccepted: root.acceptScopeText()
            onActiveFocusChanged: {
                if (activeFocus)
                    scopePopup.open()
            }
            onPressed: scopePopup.open()

            background: Rectangle {
                radius: Theme.radius
                color: Theme.surfaceSunken
                border.color: scopeField.activeFocus || scopePopup.opened ? Theme.primary : Theme.border
                border.width: 1
            }


            Row {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                InlineScopeButton {
                    icon: "folder"
                    toolTipText: qsTr("Selecionar pasta")
                    onClicked: root.browseDirectory()
                }

                InlineScopeButton {
                    icon: "include"
                    enabled: root.directory.length > 0
                    toolTipText: qsTr("Incluir subpasta")
                    onClicked: root.addIncludedDirectory()
                }

                InlineScopeButton {
                    icon: "ignore"
                    enabled: root.directory.length > 0
                    toolTipText: qsTr("Ignorar subpasta")
                    onClicked: root.addExcludedDirectory()
                }

                InlineScopeButton {
                    icon: scopePopup.opened ? "chevronUp" : "chevronDown"
                    toolTipText: scopePopup.opened ? qsTr("Fechar escopos") : qsTr("Mostrar escopos")
                    onClicked: scopePopup.opened ? scopePopup.close() : scopePopup.open()
                }
            }

            Popup {
                id: scopePopup

                x: 0
                y: scopeField.height + 6
                width: scopeField.width
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
                    spacing: 8

                    MutedLabel {
                        text: qsTr("Favoritos")
                        visible: root.favoriteDirectories.length > 0
                    }

                    Repeater {
                        model: root.favoriteDirectories

                        delegate: ScopeDropdownRow {
                            required property string modelData

                            width: scopePopup.contentItem.width
                            path: modelData
                            favorite: root.isFavorite(modelData)
                            onSelected: root.useScope(modelData)
                            onFavoriteToggled: root.toggleFavorite(modelData)
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.border
                        visible: root.favoriteDirectories.length > 0 && root.recentDirectories.length > 0
                    }

                    MutedLabel {
                        text: qsTr("Recentes")
                        visible: root.recentDirectories.length > 0
                    }

                    Repeater {
                        model: root.recentDirectories

                        delegate: ScopeDropdownRow {
                            required property string modelData

                            width: scopePopup.contentItem.width
                            path: modelData
                            favorite: root.isFavorite(modelData)
                            onSelected: root.useScope(modelData)
                            onFavoriteToggled: root.toggleFavorite(modelData)
                        }
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Nenhum escopo recente ou favorito")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                        visible: !root.hasSavedScopes
                    }
                }
            }
        }

    }

    Component.onCompleted: root.refreshScopeText()

    component ScopeDropdownRow: Rectangle {
        id: scopeRow

        property string path: ""
        property bool favorite: false

        signal selected()
        signal favoriteToggled()

        height: 36
        radius: 10
        color: rowMouseArea.containsMouse ? Theme.surfaceRaised : "transparent"
        border.color: "transparent"
        border.width: 1
        Accessible.role: Accessible.Button
        Accessible.name: path

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: favoriteButton.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: root.shortPath(scopeRow.path)
            color: Theme.text
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideLeft
        }

        Rectangle {
            id: favoriteButton

            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            radius: 14
            color: favoriteMouseArea.containsMouse ? Theme.surfaceSunken : "transparent"
            Accessible.role: Accessible.Button
            Accessible.name: scopeRow.favorite ? qsTr("Remover favorito") : qsTr("Adicionar favorito")

            Canvas {
                id: favoriteIcon

                anchors.centerIn: parent
                width: 16
                height: 16
                antialiasing: true

                onPaint: {
                    const context = getContext("2d")
                    const centerX = width / 2
                    const centerY = height / 2 + 0.5
                    const outerRadius = 6.6
                    const innerRadius = 2.8
                    const pointCount = 5
                    const startAngle = -Math.PI / 2

                    context.clearRect(0, 0, width, height)
                    context.beginPath()

                    for (let point = 0; point < pointCount * 2; ++point) {
                        const radius = point % 2 === 0 ? outerRadius : innerRadius
                        const angle = startAngle + point * Math.PI / pointCount
                        const x = centerX + Math.cos(angle) * radius
                        const y = centerY + Math.sin(angle) * radius

                        if (point === 0) {
                            context.moveTo(x, y)
                        } else {
                            context.lineTo(x, y)
                        }
                    }

                    context.closePath()
                    context.lineJoin = "round"
                    context.lineWidth = 1.6
                    context.strokeStyle = scopeRow.favorite ? Theme.warning : Theme.textMuted
                    context.fillStyle = scopeRow.favorite ? Theme.warning : "transparent"

                    if (scopeRow.favorite) {
                        context.fill()
                    }

                    context.stroke()
                }

                Connections {
                    target: scopeRow

                    function onFavoriteChanged() {
                        favoriteIcon.requestPaint()
                    }
                }
            }

            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }

            MouseArea {
                id: favoriteMouseArea

                anchors.fill: parent
                hoverEnabled: true
                onClicked: mouse => {
                    mouse.accepted = true
                    scopeRow.favoriteToggled()
                }
            }
        }

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        MouseArea {
            id: rowMouseArea

            anchors.left: parent.left
            anchors.right: favoriteButton.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            hoverEnabled: true
            onClicked: scopeRow.selected()
        }
    }

    component InlineScopeButton: Rectangle {
        id: inlineButton

        property string icon: ""
        property string toolTipText: ""

        signal clicked()

        width: 28
        height: 28
        radius: 8
        color: inlineMouseArea.containsMouse ? Theme.surfaceRaised : "transparent"
        opacity: enabled ? 1.0 : 0.45
        Accessible.role: Accessible.Button
        Accessible.name: toolTipText

        Canvas {
            id: iconCanvas

            anchors.centerIn: parent
            width: 16
            height: 16
            antialiasing: true

            onPaint: {
                const context = getContext("2d")
                const accent = inlineButton.enabled ? Theme.primary : Theme.textMuted
                const muted = inlineButton.enabled ? Theme.textMuted : Theme.textFaint

                context.clearRect(0, 0, width, height)
                context.lineCap = "round"
                context.lineJoin = "round"
                context.lineWidth = 1.6

                if (inlineButton.icon === "folder") {
                    context.strokeStyle = accent
                    context.beginPath()
                    context.moveTo(1.5, 5.5)
                    context.lineTo(6.0, 5.5)
                    context.lineTo(7.4, 7.0)
                    context.lineTo(14.5, 7.0)
                    context.lineTo(13.4, 13.0)
                    context.lineTo(2.5, 13.0)
                    context.closePath()
                    context.stroke()
                    return
                }

                if (inlineButton.icon === "include" || inlineButton.icon === "ignore") {
                    context.strokeStyle = muted
                    context.strokeRect(2.0, 3.0, 12.0, 10.0)
                    context.strokeStyle = inlineButton.icon === "include" ? Theme.primary : Theme.warning
                    context.beginPath()
                    context.moveTo(5.0, 8.0)
                    context.lineTo(11.0, 8.0)
                    if (inlineButton.icon === "include") {
                        context.moveTo(8.0, 5.0)
                        context.lineTo(8.0, 11.0)
                    }
                    context.stroke()
                    return
                }

                context.strokeStyle = muted
                context.beginPath()
                if (inlineButton.icon === "chevronUp") {
                    context.moveTo(4.0, 10.0)
                    context.lineTo(8.0, 6.0)
                    context.lineTo(12.0, 10.0)
                } else {
                    context.moveTo(4.0, 6.0)
                    context.lineTo(8.0, 10.0)
                    context.lineTo(12.0, 6.0)
                }
                context.stroke()
            }

            Connections {
                target: inlineButton

                function onIconChanged() {
                    iconCanvas.requestPaint()
                }

                function onEnabledChanged() {
                    iconCanvas.requestPaint()
                }
            }
        }

        HoverHandler {
            cursorShape: inlineButton.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        }

        ToolTip.visible: inlineMouseArea.containsMouse && inlineButton.toolTipText.length > 0
        ToolTip.delay: 450
        ToolTip.text: inlineButton.toolTipText

        MouseArea {
            id: inlineMouseArea

            anchors.fill: parent
            enabled: inlineButton.enabled
            hoverEnabled: true
            onClicked: mouse => {
                mouse.accepted = true
                inlineButton.clicked()
            }
        }
    }
}
