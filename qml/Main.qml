pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Reposco
import "components"

Window {
    id: root

    property alias darkMode: paletteStyle.darkMode
    property var desktopSession: null
    property HistoryController historyController: null
    property alias reducedMotion: paletteStyle.reducedMotion
    readonly property string selectedExcerpt: historyList.selectedSlot ? historyList.selectedSlot.excerpt : ""
    readonly property string selectedKind: historyList.selectedSlot ? historyList.selectedSlot.kind : "Text"

    function expandHistory() {
        if (desktopSession)
            desktopSession.showWorkspace();
    }
    function focusCurrentView() {
        search.forceActiveFocus();
    }
    function moveSelection(delta) {
        historyList.moveSelection(delta);
    }
    function openSettings() {
        if (desktopSession)
            desktopSession.showSettings();
    }

    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint
    height: 334
    minimumHeight: Math.min(250, Screen.desktopAvailableHeight - 32)
    minimumWidth: Math.min(440, Screen.desktopAvailableWidth - 32)
    title: qsTr("Reposco")
    visible: false
    width: 600

    onClosing: function (close) {
        close.accepted = false;
        root.hide();
    }

    StyleTokens {
        id: paletteStyle

        highContrast: root.desktopSession !== null && root.desktopSession.highContrast
        nativeMaterial: root.desktopSession !== null && root.desktopSession.acrylicActive
        objectName: "paletteStyle"
        systemAccent: root.desktopSession ? root.desktopSession.systemAccent : (darkMode ? "#60cdff" : "#005fb8")
        systemDark: root.desktopSession === null || root.desktopSession.systemDark
        systemReducedMotion: root.desktopSession !== null && root.desktopSession.systemReducedMotion
    }
    Connections {
        function onSearchChanged() {
            search.text = root.historyController.searchQuery;
        }

        target: root.historyController
    }
    SoftGlass {
        anchors.fill: parent
        solidColor: paletteStyle.quickBackground
        tintColor: paletteStyle.quickTint
        tokens: paletteStyle

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.preferredHeight: 52
                Layout.rightMargin: 10
                spacing: 10

                Image {
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 20
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                    source: "qrc:/assets/branding/reposco-icon-primary.png"
                    sourceSize.height: 64
                    sourceSize.width: 64
                }
                PaletteSearch {
                    id: search

                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    leadingIcon: false
                    objectName: "paletteSearch"
                    tokens: paletteStyle
                    utilityMode: true

                    Keys.onDownPressed: historyList.moveSelection(1)
                    Keys.onEnterPressed: function (event) {
                        if (root.desktopSession)
                            root.desktopSession.replaySelected((event.modifiers & Qt.ControlModifier) !== 0, (event.modifiers & Qt.ShiftModifier) !== 0);
                    }
                    Keys.onReturnPressed: function (event) {
                        if (root.desktopSession)
                            root.desktopSession.replaySelected((event.modifiers & Qt.ControlModifier) !== 0, (event.modifiers & Qt.ShiftModifier) !== 0);
                    }
                    Keys.onUpPressed: historyList.moveSelection(-1)
                    onTextChanged: {
                        if (root.historyController && root.historyController.searchQuery !== text)
                            root.historyController.searchQuery = text;
                    }
                }
                CaptureStatus {
                    historyController: root.historyController
                    tokens: paletteStyle

                    onClicked: root.openSettings()
                }
                ActionButton {
                    Accessible.name: qsTr("Open History Workspace")
                    Layout.preferredHeight: 28
                    Layout.preferredWidth: 28
                    flat: true
                    objectName: "expandButton"
                    tokens: paletteStyle

                    contentItem: Text {
                        color: paletteStyle.quickSecondary
                        font.family: "Segoe Fluent Icons"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        text: "\uE740"
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: root.expandHistory()
                }
                ActionButton {
                    Accessible.name: qsTr("Open Settings in History Workspace")
                    Layout.preferredHeight: 28
                    Layout.preferredWidth: 28
                    flat: true
                    objectName: "settingsButton"
                    tokens: paletteStyle

                    contentItem: Text {
                        color: paletteStyle.quickSecondary
                        font.family: "Segoe Fluent Icons"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        text: "\uE713"
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: root.openSettings()
                }
            }
            Rectangle {
                Layout.fillWidth: true
                color: paletteStyle.line
                implicitHeight: 1
            }
            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.margins: 6

                HistoryStack {
                    id: historyList

                    anchors.fill: parent
                    historyController: root.historyController
                    tokens: paletteStyle
                    utilityMode: true
                    visible: count > 0

                    onSelectionRequested: search.forceActiveFocus()
                }
                EmptyState {
                    anchors.fill: parent
                    captureActive: root.historyController !== null && root.historyController.captureActive
                    objectName: "emptyState"
                    searching: search.text.length > 0
                    tokens: paletteStyle
                    utilityMode: true
                    visible: historyList.count === 0

                    onSettingsRequested: root.openSettings()
                }
            }
            Rectangle {
                Layout.fillWidth: true
                color: paletteStyle.line
                implicitHeight: 1
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.preferredHeight: 40
                Layout.rightMargin: 16
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    color: paletteStyle.caution
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    text: root.historyController ? root.historyController.replayStatus : ""
                    textFormat: Text.PlainText
                    visible: text.length > 0
                }
                KeyHint {
                    keyText: "Enter"
                    label: qsTr("Paste")
                    tokens: paletteStyle
                    visible: !root.historyController || root.historyController.replayStatus.length === 0
                }
                KeyHint {
                    keyText: "Shift+Enter"
                    label: qsTr("Plain")
                    tokens: paletteStyle
                    visible: !root.historyController || root.historyController.replayStatus.length === 0
                }
                KeyHint {
                    keyText: "Ctrl+Enter"
                    label: qsTr("Copy")
                    tokens: paletteStyle
                    visible: !root.historyController || root.historyController.replayStatus.length === 0
                }
                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }
    Shortcut {
        enabled: root.visible && root.active
        sequence: "Escape"

        onActivated: root.hide()
    }
    Shortcut {
        enabled: root.visible && root.active
        sequence: "Ctrl+,"

        onActivated: root.openSettings()
    }
}
