pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Reposco
import "components"

Window {
    id: root

    readonly property bool darkMode: tokens.darkMode
    property var desktopSession: null
    property HistoryController historyController: null
    readonly property string selectedKind: historyList.selectedSlot ? historyList.selectedSlot.kind : "Text"
    property bool settingsOpen: false
    required property StyleTokens tokens

    function confirmCapture() {
        openSettings();
        settings.confirmingCapture = true;
    }
    function confirmClearHistory() {
        openSettings();
        settings.confirmingClear = true;
    }
    function focusCurrentView() {
        if (settingsOpen)
            settings.focusView();
        else
            search.forceActiveFocus();
    }
    function moveSelection(delta) {
        if (!settingsOpen)
            historyList.moveSelection(delta);
    }
    function openSettings() {
        settingsOpen = true;
        if (historyController)
            historyController.cancelReplay();
        settings.focusView();
    }
    function returnToHistory() {
        settingsOpen = false;
        search.forceActiveFocus();
    }

    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    height: 520
    minimumHeight: Math.min(420, Screen.desktopAvailableHeight - 32)
    minimumWidth: Math.min(760, Screen.desktopAvailableWidth - 32)
    objectName: "historyWorkspace"
    title: qsTr("Reposco History")
    visible: false
    width: 820

    onClosing: function (close) {
        close.accepted = false;
        root.hide();
    }
    onVisibleChanged: {
        if (visible)
            reveal.restart();
        else {
            reveal.stop();
            content.opacity = 1;
        }
    }

    Connections {
        function onSearchChanged() {
            search.text = root.historyController.searchQuery;
        }

        target: root.historyController
    }
    NumberAnimation {
        id: reveal

        duration: root.tokens.duration
        easing.type: Easing.OutCubic
        from: 0.85
        property: "opacity"
        target: content
        to: 1
    }
    GlassSurface {
        anchors.fill: parent
        materialActive: root.desktopSession !== null && root.desktopSession.micaActive
        tokens: root.tokens

        ColumnLayout {
            id: content

            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.preferredHeight: 40
                Layout.rightMargin: 10
                objectName: "workspaceTitleBar"
                spacing: 8

                Image {
                    Layout.preferredHeight: 18
                    Layout.preferredWidth: 18
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                    source: "qrc:/assets/branding/reposco-icon-primary.png"
                    sourceSize.height: 64
                }
                Image {
                    Layout.preferredHeight: 15
                    Layout.preferredWidth: 64
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                    source: root.tokens.darkMode ? "qrc:/assets/branding/reposco-wordmark-primary.png" : "qrc:/assets/branding/reposco-wordmark-dark.png"
                    sourceSize.height: 64
                    visible: !root.tokens.highContrast
                }
                Text {
                    color: root.tokens.foreground
                    font.pixelSize: 13
                    text: qsTr("Reposco")
                    visible: root.tokens.highContrast
                }
                Item {
                    Layout.fillWidth: true
                }
                CaptureStatus {
                    historyController: root.historyController
                    objectName: "workspaceStatus"
                    tokens: root.tokens

                    onClicked: root.openSettings()
                }
                ActionButton {
                    Accessible.name: qsTr("Settings")
                    Layout.preferredHeight: 28
                    Layout.preferredWidth: 28
                    flat: true
                    objectName: "workspaceSettings"
                    tokens: root.tokens

                    contentItem: Text {
                        color: root.tokens.secondary
                        font.family: "Segoe Fluent Icons"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        text: "\uE713"
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: root.openSettings()
                }
                ActionButton {
                    Accessible.name: qsTr("Hide History Workspace")
                    Layout.preferredHeight: 28
                    Layout.preferredWidth: 28
                    flat: true
                    objectName: "workspaceClose"
                    tokens: root.tokens

                    contentItem: Text {
                        color: root.tokens.secondary
                        font.family: "Segoe Fluent Icons"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        text: "\uE8BB"
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: root.hide()
                }
            }
            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    visible: !root.settingsOpen

                    PaletteSearch {
                        id: search

                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        boxed: true
                        objectName: "workspaceSearch"
                        tokens: root.tokens

                        Keys.onDownPressed: historyList.moveSelection(1)
                        Keys.onEnterPressed: function (event) {
                            if (root.desktopSession)
                                root.desktopSession.copySelected((event.modifiers & Qt.ShiftModifier) !== 0);
                        }
                        Keys.onReturnPressed: function (event) {
                            if (root.desktopSession)
                                root.desktopSession.copySelected((event.modifiers & Qt.ShiftModifier) !== 0);
                        }
                        Keys.onUpPressed: historyList.moveSelection(-1)
                        onTextChanged: {
                            if (root.historyController && root.historyController.searchQuery !== text)
                                root.historyController.searchQuery = text;
                        }
                    }
                    RowLayout {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        spacing: 8

                        Item {
                            Layout.fillHeight: true
                            Layout.preferredWidth: 304

                            HistoryStack {
                                id: historyList

                                anchors.fill: parent
                                historyController: root.historyController
                                tokens: root.tokens
                                visible: count > 0

                                onSelectionRequested: search.forceActiveFocus()
                            }
                            EmptyState {
                                anchors.fill: parent
                                captureActive: root.historyController !== null && root.historyController.captureActive
                                objectName: "workspaceEmpty"
                                searching: search.text.length > 0
                                tokens: root.tokens
                                visible: historyList.count === 0

                                onSettingsRequested: root.openSettings()
                            }
                        }
                        PreviewPane {
                            id: preview

                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            imageSource: root.historyController ? (root.tokens.darkMode ? root.historyController.darkPreviewImage : root.historyController.lightPreviewImage) : ""
                            kind: root.selectedKind
                            metadata: historyList.selectedSlot ? historyList.selectedSlot.byteSize + " · " + historyList.selectedSlot.age : ""
                            objectName: "contextPreview"
                            preview: root.historyController ? root.historyController.preview : ""
                            rows: root.historyController ? root.historyController.previewRows : []
                            tokens: root.tokens
                            truncated: root.historyController !== null && root.historyController.previewTruncated
                            visible: historyList.count > 0
                        }
                        Item {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            visible: historyList.count === 0
                        }
                    }
                }
                SettingsWindow {
                    id: settings

                    anchors.fill: parent
                    desktopSession: root.desktopSession
                    historyController: root.historyController
                    objectName: "settingsView"
                    tokens: root.tokens
                    visible: root.settingsOpen

                    onBackRequested: root.returnToHistory()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.preferredHeight: 48
                Layout.rightMargin: 16
                spacing: 12
                visible: !root.settingsOpen

                KeyHint {
                    keyText: "Enter"
                    label: qsTr("Copy")
                    tokens: root.tokens
                }
                KeyHint {
                    keyText: "Shift+Enter"
                    label: qsTr("Plain text")
                    tokens: root.tokens
                }
                Text {
                    Layout.fillWidth: true
                    color: root.tokens.caution
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    text: root.historyController ? root.historyController.replayStatus : ""
                    textFormat: Text.PlainText
                }
                ActionButton {
                    enabled: root.historyController !== null && root.historyController.available && !root.historyController.replayBusy
                    flat: true
                    objectName: "workspaceCopyPlain"
                    text: qsTr("Plain text")
                    tokens: root.tokens

                    onClicked: root.desktopSession.copySelected(true)
                }
                ActionButton {
                    enabled: root.historyController !== null && root.historyController.available && !root.historyController.replayBusy
                    objectName: "workspaceCopy"
                    text: qsTr("Copy")
                    tokens: root.tokens

                    onClicked: root.desktopSession.copySelected()
                }
            }
            Item {
                Layout.preferredHeight: 12
                visible: root.settingsOpen
            }
        }
    }
    Shortcut {
        enabled: root.visible && root.active
        sequence: "Escape"

        onActivated: {
            if (root.settingsOpen)
                root.returnToHistory();
        }
    }
    Shortcut {
        enabled: root.visible && root.active
        sequence: "Ctrl+,"

        onActivated: root.openSettings()
    }
    Shortcut {
        enabled: root.visible && root.active && !root.settingsOpen && preview.visible && preview.canShowSource
        sequence: "Alt+P"

        onActivated: preview.showSource = !preview.showSource
    }
}
