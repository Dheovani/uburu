import QtQuick
import QtQuick.Controls

import "../theme"

Menu {
    id: root

    padding: 6

    background: Rectangle {
        implicitWidth: 250
        radius: Theme.radius
        color: Theme.surfaceRaised
        border.color: Theme.borderStrong
        border.width: 1
    }

    contentItem: ListView {
        implicitWidth: 250
        implicitHeight: contentHeight
        model: root.contentModel
        interactive: false
        clip: true
    }
}
