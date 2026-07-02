import QtQuick
import QtQuick.Controls

import "../theme"

MenuSeparator {
    implicitHeight: 11
    topPadding: 5
    bottomPadding: 5

    contentItem: Rectangle {
        implicitHeight: 1
        color: Theme.border
    }
}
