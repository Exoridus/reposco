pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Reposco
import "components"

Window {
    id: root

    property alias darkMode: paletteStyle.darkMode
    property var desktopSession: null
    property HistoryController historyController: null
    property alias reducedMotion: paletteStyle.reducedMotion

    function confirmClearHistory() {
        openSettings();
        settings.confirmingClear = true;
    }
    function moveSelection(delta) {
        if (historyList.count > 0) {
            historyList.currentIndex = Math.max(0, Math.min(historyList.count - 1, historyList.currentIndex + delta));
            historyList.positionViewAtIndex(historyList.currentIndex, ListView.Contain);
        }
    }
    function openSettings() {
        settings.show();
        settings.raise();
        settings.requestActivate();
    }

    color: "transparent"
    height: historyList.count > 0 ? 308 + Math.min(4, historyList.count) * 38 : 310
    minimumHeight: 280
    minimumWidth: 440
    title: qsTr("Reposco")
    visible: false
    width: 680

    onClosing: function (close) {
        close.accepted = false;
        root.hide();
    }

    StyleTokens {
        id: paletteStyle
    }
    SettingsWindow {
        id: settings

        desktopSession: root.desktopSession
        historyController: root.historyController
        objectName: "settingsWindow"
        tokens: paletteStyle
    }
    Connections {
        function onModelReset() {
            historyList.currentIndex = historyList.count > 0 ? 0 : -1;
            if (root.historyController)
                root.historyController.selectRow(historyList.currentIndex);
        }

        target: root.historyController ? root.historyController.model : null
    }
    Connections {
        function onSearchChanged() {
            if (root.historyController)
                search.text = root.historyController.searchQuery;
        }
        function onStateChanged() {
            if (root.historyController && root.historyController.available)
                root.historyController.selectRow(historyList.currentIndex);
        }

        target: root.historyController
    }
    GlassSurface {
        anchors.fill: parent
        tokens: paletteStyle

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 10

            RowLayout {
                id: titleBar

                Layout.fillWidth: true
                Layout.preferredHeight: 26
                objectName: "titleBar"
                spacing: 10

                Image {
                    Layout.preferredHeight: 23
                    Layout.preferredWidth: 23
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/assets/branding/reposco-icon-primary.png"
                    sourceSize.height: 64
                    sourceSize.width: 64
                }
                Image {
                    Layout.preferredHeight: 23
                    Layout.preferredWidth: 90
                    fillMode: Image.PreserveAspectFit
                    source: paletteStyle.darkMode ? "qrc:/assets/branding/reposco-wordmark-primary.png" : "qrc:/assets/branding/reposco-wordmark-dark.png"
                }
                Text {
                    color: paletteStyle.secondary
                    font.pixelSize: 12
                    text: qsTr("History")
                }
                Item {
                    Layout.fillWidth: true
                }
                Text {
                    color: paletteStyle.secondary
                    font.pixelSize: 11
                    text: root.historyController && root.historyController.captureActive ? qsTr("Capturing") : qsTr("Paused")
                }
                ActionButton {
                    objectName: "settingsButton"
                    padding: 5
                    text: qsTr("Settings")
                    tokens: paletteStyle

                    onClicked: root.openSettings()
                }
            }
            PaletteSearch {
                id: search

                Layout.fillWidth: true
                Layout.preferredHeight: 42
                objectName: "paletteSearch"
                tokens: paletteStyle

                Keys.onDownPressed: root.moveSelection(1)
                Keys.onEnterPressed: function (event) {
                    if (root.desktopSession) {
                        root.desktopSession.replaySelected((event.modifiers & Qt.ControlModifier) !== 0, (event.modifiers & Qt.ShiftModifier) !== 0);
                        event.accepted = true;
                    }
                }
                Keys.onEscapePressed: root.hide()
                Keys.onReturnPressed: function (event) {
                    if (root.desktopSession) {
                        root.desktopSession.replaySelected((event.modifiers & Qt.ControlModifier) !== 0, (event.modifiers & Qt.ShiftModifier) !== 0);
                        event.accepted = true;
                    }
                }
                Keys.onUpPressed: root.moveSelection(-1)
                onTextChanged: {
                    if (root.historyController)
                        root.historyController.searchQuery = text;
                }
            }
            PreviewPane {
                Layout.fillWidth: true
                Layout.preferredHeight: 134
                imageSource: root.historyController ? (paletteStyle.darkMode ? root.historyController.darkPreviewImage : root.historyController.lightPreviewImage) : ""
                kind: root.historyController ? root.historyController.previewKind : "Text"
                preview: root.historyController ? root.historyController.preview : ""
                rows: root.historyController ? root.historyController.previewRows : []
                tokens: paletteStyle
                truncated: root.historyController !== null && root.historyController.previewTruncated
                visible: historyList.count > 0
            }
            ListView {
                id: historyList

                Layout.fillHeight: true
                Layout.fillWidth: true
                clip: true
                currentIndex: count > 0 ? 0 : -1
                model: root.historyController ? root.historyController.model : null
                objectName: "historyList"
                spacing: 3
                visible: count > 0

                ScrollBar.vertical: ScrollBar {
                }
                delegate: HistorySlot {
                    required property var entryId
                    required property int index

                    animate: root.visible
                    height: 35
                    selected: historyList.currentIndex === index
                    tokens: paletteStyle
                    width: historyList.width

                    onClicked: {
                        historyList.currentIndex = index;
                        search.forceActiveFocus();
                    }
                }

                onCurrentIndexChanged: {
                    if (root.historyController)
                        root.historyController.selectRow(currentIndex);
                }
            }
            EmptyState {
                Layout.fillHeight: true
                Layout.fillWidth: true
                captureActive: root.historyController !== null && root.historyController.captureActive
                objectName: "emptyState"
                searching: search.text.length > 0
                tokens: paletteStyle
                visible: historyList.count === 0

                onSettingsRequested: root.openSettings()
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 20

                Text {
                    Layout.fillWidth: true
                    color: paletteStyle.secondary
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    text: root.historyController && root.historyController.replayStatus.length > 0 ? root.historyController.replayStatus : qsTr("↑ ↓ Select    Enter Paste    Ctrl+Enter Copy    Esc Close")
                }
            }
        }
    }
    Shortcut {
        enabled: root.visible && root.active
        sequence: "Escape"

        onActivated: root.hide()
    }
}
