import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../theme"

Panel {
    id: root

    property string filePath: ""
    property string location: ""
    property string preview: ""
    property string previewHtml: ""
    property bool loading: false
    property int tabWidthSpaces: 2

    color: Theme.windowRaised

    function selectedPreviewLineIndex() {
        const lines = root.preview.split("\n")

        for (let lineIndex = 0; lineIndex < lines.length; ++lineIndex) {
            if (lines[lineIndex].startsWith(">"))
                return lineIndex
        }

        return -1
    }

    function scrollPreviewToSelectedLine() {
        const lineIndex = selectedPreviewLineIndex()

        if (lineIndex < 0)
            return

        const flickable = previewScrollView.contentItem

        if (!flickable || flickable.contentHeight === undefined || flickable.contentY === undefined)
            return

        const lineHeight = Math.max(previewFontMetrics.height, 1)
        const preferredTopMargin = previewScrollView.availableHeight / 3
        const targetY = Math.max(0, lineIndex * lineHeight - preferredTopMargin)
        const maximumY = Math.max(0, flickable.contentHeight - flickable.height)
        flickable.contentY = Math.min(targetY, maximumY)
    }

    onPreviewChanged: previewScrollTimer.restart()
    onPreviewHtmlChanged: previewScrollTimer.restart()

    Timer {
        id: previewScrollTimer

        interval: 0
        repeat: false
        onTriggered: root.scrollPreviewToSelectedLine()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            EyebrowLabel {
                text: qsTr("PREVIEW")
            }

            Label {
                Layout.fillWidth: true
                text: root.filePath.length > 0 ? root.filePath : qsTr("Pré-visualização")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideLeft
            }

            MutedLabel {
                Layout.fillWidth: true
                text: root.loading
                      ? qsTr("Carregando pré-visualização...")
                      : root.location.length > 0
                      ? root.location
                      : qsTr("Selecione um resultado para inspecionar o contexto.")
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surfaceSunken
            border.color: Theme.border
            border.width: 1
            clip: true

            ScrollView {
                id: previewScrollView

                anchors.fill: parent
                anchors.margins: 1

                FontMetrics {
                    id: previewFontMetrics

                    font.family: "Consolas"
                    font.pixelSize: 13
                }

                TextArea {
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    placeholderText: qsTr("O conteúdo do arquivo aparecerá aqui")
                    text: root.previewHtml.length > 0 ? root.previewHtml : root.preview
                    textFormat: root.previewHtml.length > 0 ? TextEdit.RichText : TextEdit.PlainText

                    color: Theme.text
                    placeholderTextColor: Theme.textFaint
                    selectionColor: Theme.primary
                    selectedTextColor: "white"
                    font.family: "Consolas"
                    font.pixelSize: 13
                    tabStopDistance: previewFontMetrics.advanceWidth(" ") * root.tabWidthSpaces
                    leftPadding: 16
                    rightPadding: 16
                    topPadding: 16
                    bottomPadding: 16

                    background: Rectangle {
                        color: "transparent"
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: root.loading
                color: "#66000000"

                BusyIndicator {
                    anchors.centerIn: parent
                    running: root.loading
                }
            }
        }
    }
}
