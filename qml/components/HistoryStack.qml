pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Reposco

ListView {
    id: root

    property HistoryController historyController: null
    readonly property HistorySlot selectedSlot: currentItem as HistorySlot
    required property StyleTokens tokens
    property bool utilityMode: false

    signal selectionRequested

    function choose(row) {
        const selected = Math.max(0, Math.min(count - 1, row));
        if (historyController)
            historyController.selectRow(selected);
        else
            currentIndex = selected;
        positionViewAtIndex(selected, ListView.Contain);
        selectionRequested();
    }
    function moveSelection(delta) {
        if (count > 0)
            choose(currentIndex + delta);
    }

    boundsBehavior: Flickable.StopAtBounds
    clip: true
    currentIndex: historyController ? historyController.selectedRow : count > 0 ? 0 : -1
    model: historyController ? historyController.model : null
    objectName: "historyList"
    spacing: 0

    ScrollBar.vertical: ScrollBar {
        width: 4
    }
    delegate: HistorySlot {
        required property var entryId
        required property int index

        height: root.utilityMode ? 38 : 36
        selected: root.currentIndex === index
        tokens: root.tokens
        utilityMode: root.utilityMode
        width: root.width

        onClicked: root.choose(index)
    }
    displaced: Transition {
        enabled: !root.utilityMode && root.tokens.duration > 0

        NumberAnimation {
            duration: 90
            easing.type: Easing.OutQuad
            properties: "x,y"
        }
    }
    move: Transition {
        enabled: !root.utilityMode && root.tokens.duration > 0

        NumberAnimation {
            duration: 90
            easing.type: Easing.OutQuad
            properties: "x,y"
        }
    }
}
