pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Reposco

ActionButton {
    id: root

    property HistoryController historyController: null
    readonly property bool pending: historyController !== null && historyController.policyPending
    readonly property bool recording: historyController !== null && historyController.captureActive

    Accessible.name: pending ? qsTr("Capture policy updating; open privacy settings") : recording ? qsTr("Clipboard capture active; open privacy settings") : qsTr("Clipboard capture paused; open privacy settings")
    implicitHeight: 26
    leftPadding: 10
    padding: 3
    rightPadding: 10

    background: Rectangle {
        border.color: root.visualFocus ? root.tokens.accent : root.tokens.line
        border.width: root.visualFocus ? 2 : 1
        color: root.hovered ? root.tokens.hover : "transparent"
        radius: 13
    }
    contentItem: RowLayout {
        spacing: 6

        Rectangle {
            border.color: root.pending ? root.tokens.secondary : root.tokens.caution
            border.width: root.recording ? 0 : 1.5
            color: root.recording ? root.tokens.secondary : "transparent"
            implicitHeight: root.recording ? 7 : 9
            implicitWidth: implicitHeight
            radius: 5
        }
        Text {
            color: root.tokens.secondary
            font.pixelSize: 12
            text: root.pending ? qsTr("Updating") : root.recording ? qsTr("Capturing") : qsTr("Paused")
        }
    }
}
