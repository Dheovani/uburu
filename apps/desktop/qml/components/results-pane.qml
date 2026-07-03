import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Panel {
    id: root

    property alias model: resultList.model
    property int resultCount: resultList.count
    property bool autoSelectFirstResult: true

    signal resultSelected(string filePath, string absolutePath, string location, string preview, var highlights)
    signal resultsCleared()
    signal openFileRequested(string filePath)
    signal openWithRequested(string filePath)
    signal openFolderRequested(string filePath)
    signal copyTextRequested(string text)

    color: Theme.surface

    function occurrenceText(filePath, location, preview) {
        return filePath + ":" + location + "\n" + preview
    }

    function currentResult() {
        return resultList.currentItem
    }

    function selectCurrentResult() {
        const result = currentResult()

        if (!result)
            return

        root.resultSelected(result.filePath, result.absolutePath, result.location, result.preview, result.highlights)
    }

    function selectResultAt(index) {
        if (index < 0 || index >= resultList.count)
            return

        resultList.currentIndex = index
        resultList.positionViewAtIndex(index, ListView.Contain)
        resultList.forceActiveFocus()
        Qt.callLater(root.selectCurrentResult)
    }

    function selectNextResult() {
        if (resultList.count === 0)
            return

        const nextIndex = resultList.currentIndex < 0 ? 0 : Math.min(resultList.currentIndex + 1, resultList.count - 1)
        selectResultAt(nextIndex)
    }

    function selectPreviousResult() {
        if (resultList.count === 0)
            return

        const previousIndex = resultList.currentIndex < 0 ? 0 : Math.max(resultList.currentIndex - 1, 0)
        selectResultAt(previousIndex)
    }

    function hasCurrentResult() {
        return resultList.currentIndex >= 0 && resultList.currentItem !== null
    }

    function openCurrentResult() {
        const result = currentResult()

        if (result)
            root.openFileRequested(result.absolutePath)
    }

    function copyCurrentPath() {
        const result = currentResult()

        if (result)
            root.copyTextRequested(result.absolutePath)
    }

    function copyCurrentOccurrence() {
        const result = currentResult()

        if (result)
            root.copyTextRequested(occurrenceText(result.absolutePath, result.location, result.preview))
    }

    ActionMenu {
        id: resultContextMenu

        property string filePath: ""
        property string absolutePath: ""
        property string location: ""
        property string preview: ""

        ActionMenuItem {
            text: qsTr("Abrir arquivo")
            shortcutText: qsTr("Enter")
            onTriggered: root.openFileRequested(resultContextMenu.absolutePath)
        }

        ActionMenuItem {
            text: qsTr("Abrir com...")
            onTriggered: root.openWithRequested(resultContextMenu.absolutePath)
        }

        ActionMenuItem {
            text: qsTr("Abrir local do arquivo")
            onTriggered: root.openFolderRequested(resultContextMenu.absolutePath)
        }

        ActionMenuSeparator {}

        ActionMenuItem {
            text: qsTr("Copiar caminho")
            shortcutText: qsTr("Ctrl+C")
            onTriggered: root.copyTextRequested(resultContextMenu.absolutePath)
        }

        ActionMenuItem {
            text: qsTr("Copiar ocorrência")
            shortcutText: qsTr("Ctrl+Shift+C")
            onTriggered: root.copyTextRequested(
                root.occurrenceText(
                    resultContextMenu.absolutePath,
                    resultContextMenu.location,
                    resultContextMenu.preview
                )
            )
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: qsTr("Resultados")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
            }

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: resultCountText.implicitWidth + 18
                Layout.preferredHeight: 26
                radius: 13
                color: Theme.surfaceRaised
                border.color: Theme.border
                border.width: 1

                Text {
                    id: resultCountText

                    anchors.centerIn: parent
                    text: qsTr("%1").arg(resultList.count)
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                }
            }
        }

        ListView {
            id: resultList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            cacheBuffer: 1600
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            focus: true
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Lista de resultados")
            Accessible.description: qsTr("Resultados encontrados na busca atual.")

            Keys.onReturnPressed: root.openCurrentResult()
            Keys.onEnterPressed: root.openCurrentResult()
            Keys.onPressed: event => {
                if (event.key === Qt.Key_F4 && (event.modifiers & Qt.ShiftModifier)) {
                    root.selectPreviousResult()
                    event.accepted = true
                    return
                }

                if (event.key === Qt.Key_F4) {
                    root.selectNextResult()
                    event.accepted = true
                    return
                }

                if (event.matches(StandardKey.Copy)) {
                    root.copyCurrentPath()
                    event.accepted = true
                    return
                }

                if ((event.modifiers & Qt.ControlModifier) && (event.modifiers & Qt.ShiftModifier)
                        && event.key === Qt.Key_C) {
                    root.copyCurrentOccurrence()
                    event.accepted = true
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            onCountChanged: {
                if (count === 0) {
                    currentIndex = -1
                    root.resultsCleared()
                    return
                }

                if (root.autoSelectFirstResult && currentIndex < 0)
                    root.selectResultAt(0)
            }

            delegate: ItemDelegate {
                required property int index
                required property string filePath
                required property string absolutePath
                required property string location
                required property string preview
                required property var highlights
                required property bool fileGroupHeader
                required property string fileGroupLabel

                width: ListView.view.width
                height: fileGroupHeader ? 92 : 64
                highlighted: ListView.isCurrentItem
                Accessible.role: Accessible.ListItem
                Accessible.name: filePath
                Accessible.description: qsTr("Ocorrência em %1").arg(location)

                onClicked: {
                    resultList.currentIndex = index
                    resultList.forceActiveFocus()
                    root.resultSelected(filePath, absolutePath, location, preview, highlights)
                }

                onDoubleClicked: {
                    resultList.currentIndex = index
                    resultList.forceActiveFocus()
                    root.openFileRequested(absolutePath)
                }

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        resultList.currentIndex = index
                        resultList.forceActiveFocus()
                        root.resultSelected(filePath, absolutePath, location, preview, highlights)
                        resultContextMenu.filePath = filePath
                        resultContextMenu.absolutePath = absolutePath
                        resultContextMenu.location = location
                        resultContextMenu.preview = preview
                        resultContextMenu.popup()
                    }
                }

                contentItem: Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: fileGroupHeader ? 8 : 10
                    anchors.bottomMargin: 10
                    spacing: 4

                    Text {
                        visible: fileGroupHeader
                        text: fileGroupLabel
                        width: parent.width
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeTiny
                        font.bold: true
                        elide: Text.ElideLeft
                    }

                    Text {
                        text: filePath
                        width: parent.width
                        color: Theme.text
                        font.pixelSize: Theme.fontSize
                        font.bold: true
                        elide: Text.ElideLeft
                    }

                    Text {
                        text: location
                        width: parent.width
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideRight
                    }
                }

                background: Rectangle {
                    radius: Theme.radius
                    color: highlighted ? Theme.surfaceRaised : hovered ? Theme.surfaceRaised : Theme.surfaceSunken
                    border.color: highlighted ? Theme.primary : Theme.border
                    border.width: 1
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: resultList.count === 0
                color: "transparent"

                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 48, 340)
                    spacing: 8

                    Text {
                        text: qsTr("Pronto para buscar")
                        width: parent.width
                        color: Theme.text
                        font.pixelSize: 17
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        text: qsTr("Escolha uma pasta e digite uma consulta para ver os resultados aqui.")
                        width: parent.width
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
