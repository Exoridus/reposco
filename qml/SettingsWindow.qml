pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Reposco
import "components"

Window {
    id: root

    property bool confirmingClear: false
    property var desktopSession: null
    property HistoryController historyController: null
    property int pendingRetentionDays: 90
    property string ruleStatus: ""
    required property StyleTokens tokens

    function applyPolicy(six, eight) {
        if (historyController)
            historyController.setCapturePolicy(six, eight, historyController.excludedApplications);
    }

    color: tokens.background
    height: Math.min(720, Screen.desktopAvailableHeight - 60)
    minimumHeight: 480
    minimumWidth: 480
    title: qsTr("Reposco settings")
    visible: false
    width: Math.min(620, Screen.desktopAvailableWidth - 40)

    onConfirmingClearChanged: {
        if (confirmingClear)
            Qt.callLater(function () {
                settingsScrollBar.position = 1;
            });
    }

    ScrollView {
        id: scroll

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.fill: parent
        anchors.margins: 28
        clip: true

        ScrollBar.vertical: ScrollBar {
            id: settingsScrollBar
        }

        ColumnLayout {
            spacing: 14
            width: scroll.availableWidth

            Text {
                color: root.tokens.foreground
                font.pixelSize: 25
                font.weight: Font.DemiBold
                text: qsTr("Your history, your rules")
            }
            Text {
                color: root.tokens.secondary
                font.letterSpacing: 1.2
                font.pixelSize: 11
                text: qsTr("CAPTURE & PRIVACY")
            }
            Text {
                Layout.fillWidth: true
                color: root.tokens.secondary
                font.pixelSize: 13
                text: root.historyController ? root.historyController.status : qsTr("Capture unavailable.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: root.tokens.foreground
                font.pixelSize: 13
                text: qsTr("Capture stores future copied text locally in encrypted history. Windows secret markers and your filters are respected. Unmarked passwords, including browser-extension writes, can still be captured. Starting or resuming does not import the current clipboard.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            ActionButton {
                enabled: root.historyController !== null && root.historyController.available
                objectName: "captureToggle"
                text: root.historyController && root.historyController.captureActive ? qsTr("Pause capture") : qsTr("Enable / resume capture")
                tokens: root.tokens

                onClicked: {
                    if (root.historyController)
                        root.historyController.setCaptureActive(!root.historyController.captureActive);
                }
            }
            PolicySwitch {
                Layout.fillWidth: true
                checked: root.historyController ? root.historyController.excludeSixDigitCodes : true
                enabled: root.historyController !== null && root.historyController.available
                text: qsTr("Ignore standalone six-digit codes (recommended)")
                tokens: root.tokens

                onToggled: root.applyPolicy(checked, root.historyController.excludeEightDigitCodes)
            }
            Text {
                Layout.fillWidth: true
                color: root.tokens.secondary
                font.pixelSize: 12
                text: qsTr("A complete six-digit number, with optional surrounding whitespace, is skipped before hashing or storage. This also skips legitimate six-digit numbers. Your Windows clipboard is never changed.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            PolicySwitch {
                Layout.fillWidth: true
                checked: root.historyController !== null && root.historyController.excludeEightDigitCodes
                enabled: root.historyController !== null && root.historyController.available
                text: qsTr("Also ignore standalone eight-digit codes")
                tokens: root.tokens

                onToggled: root.applyPolicy(root.historyController.excludeSixDigitCodes, checked)
            }
            Rectangle {
                Layout.fillWidth: true
                color: root.tokens.line
                implicitHeight: 1
            }
            Text {
                color: root.tokens.secondary
                font.letterSpacing: 1.2
                font.pixelSize: 11
                text: qsTr("APPLICATION EXCLUSIONS")
            }
            Text {
                Layout.fillWidth: true
                color: root.tokens.secondary
                font.pixelSize: 12
                text: qsTr("One full .exe path per line (up to 32). Clipboard-owner paths are checked before reading text. When exclusions exist, an unknown owner is skipped. A browser extension cannot be identified reliably; browsers are not excluded by default.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            TextArea {
                id: paths

                Layout.fillWidth: true
                Layout.preferredHeight: 90
                color: root.tokens.foreground
                font.pixelSize: 12
                padding: 10
                placeholderText: qsTr("C:/Program Files/Example/Vault.exe")
                placeholderTextColor: root.tokens.secondary
                selectedTextColor: root.tokens.foreground
                selectionColor: root.tokens.selection
                text: root.historyController ? root.historyController.excludedApplications.join("\n") : ""
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap

                background: Rectangle {
                    border.color: paths.activeFocus ? root.tokens.accent : root.tokens.line
                    color: root.tokens.raised
                    radius: 9
                }

                onTextChanged: {
                    if (text.length > 16384)
                        text = text.slice(0, 16384);
                }
            }
            ActionButton {
                enabled: root.historyController !== null && root.historyController.available
                text: qsTr("Save application rules")
                tokens: root.tokens

                onClicked: {
                    const rules = paths.text.split(/\r?\n/).map(value => value.trim()).filter(value => value.length > 0);
                    const saved = root.historyController.setCapturePolicy(root.historyController.excludeSixDigitCodes, root.historyController.excludeEightDigitCodes, rules);
                    root.ruleStatus = saved ? qsTr("Rules saved.") : qsTr("Use at most 32 absolute .exe paths, each no longer than 512 characters.");
                }
            }
            Text {
                Layout.fillWidth: true
                color: root.tokens.secondary
                font.pixelSize: 12
                text: root.ruleStatus
                textFormat: Text.PlainText
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }
            Rectangle {
                Layout.fillWidth: true
                color: root.tokens.line
                implicitHeight: 1
            }
            Text {
                color: root.tokens.secondary
                font.letterSpacing: 1.2
                font.pixelSize: 11
                text: qsTr("APPEARANCE & SHORTCUT")
            }
            PolicySwitch {
                checked: root.tokens.darkMode
                text: qsTr("Dark appearance")
                tokens: root.tokens

                onToggled: root.tokens.darkMode = checked
            }
            PolicySwitch {
                checked: root.tokens.reducedMotion
                text: qsTr("Reduce motion")
                tokens: root.tokens

                onToggled: root.tokens.reducedMotion = checked
            }
            Text {
                Layout.fillWidth: true
                color: root.tokens.secondary
                font.pixelSize: 12
                text: qsTr("Win+Alt+V opens History. Esc closes it. The palette currently supports viewing and search; paste/replay is not implemented. Appearance choices apply to this session. The surface uses a dependable solid glass-style fallback.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: root.tokens.secondary
                font.pixelSize: 12
                text: root.desktopSession ? root.desktopSession.shortcut + "  " + root.desktopSession.shortcutStatus : qsTr("Shortcut unavailable")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            RowLayout {
                ActionButton {
                    text: "Win+Alt+V"
                    tokens: root.tokens

                    onClicked: root.desktopSession.setShortcut(text)
                }
                ActionButton {
                    text: "Ctrl+Alt+V"
                    tokens: root.tokens

                    onClicked: root.desktopSession.setShortcut(text)
                }
                ActionButton {
                    text: "Ctrl+Shift+V"
                    tokens: root.tokens

                    onClicked: root.desktopSession.setShortcut(text)
                }
            }
            Rectangle {
                Layout.fillWidth: true
                color: root.tokens.line
                implicitHeight: 1
            }
            Text {
                color: root.tokens.secondary
                font.letterSpacing: 1.2
                font.pixelSize: 11
                text: qsTr("RETENTION & RECOVERY")
            }
            Text {
                Layout.fillWidth: true
                color: root.tokens.secondary
                font.pixelSize: 12
                text: qsTr("History retains up to 1,000 items, 90 days and 1 GiB of original text. Search shows the latest 100 matches; previews show up to 8,192 characters. Item and byte caps are fixed. Retention is configurable below; reducing it deletes older entries. ASCII search is case-insensitive; other Unicode text matches literally.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            Text {
                color: root.tokens.secondary
                font.pixelSize: 12
                text: root.historyController ? qsTr("Current retention: %1 days").arg(root.historyController.retentionDays) : ""
            }
            RowLayout {
                ActionButton {
                    text: qsTr("7 days")
                    tokens: root.tokens

                    onClicked: root.pendingRetentionDays = 7
                }
                ActionButton {
                    text: qsTr("30 days")
                    tokens: root.tokens

                    onClicked: root.pendingRetentionDays = 30
                }
                ActionButton {
                    text: qsTr("90 days")
                    tokens: root.tokens

                    onClicked: root.pendingRetentionDays = 90
                }
            }
            ActionButton {
                enabled: root.historyController !== null && root.historyController.available
                text: qsTr("Apply %1 days and remove older entries").arg(root.pendingRetentionDays)
                tokens: root.tokens

                onClicked: root.historyController.setRetentionDays(root.pendingRetentionDays, true)
            }
            ActionButton {
                destructive: true
                enabled: root.historyController !== null && root.historyController.available
                text: qsTr("Clear history...")
                tokens: root.tokens

                onClicked: root.confirmingClear = true
            }
            Rectangle {
                Layout.fillWidth: true
                border.color: root.tokens.line
                color: root.tokens.raised
                implicitHeight: confirmation.implicitHeight + 28
                radius: 12
                visible: root.confirmingClear

                ColumnLayout {
                    id: confirmation

                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        color: root.tokens.foreground
                        font.pixelSize: 13
                        text: qsTr("Delete all stored history? Capture will pause. Original text, comparison hashes and previews are removed from Reposco. The Windows clipboard stays unchanged. Logical deletion cannot guarantee forensic erasure from SSDs, backups or process memory.")
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        ActionButton {
                            text: qsTr("Cancel")
                            tokens: root.tokens

                            onClicked: root.confirmingClear = false
                        }
                        ActionButton {
                            destructive: true
                            enabled: root.historyController !== null && root.historyController.available
                            text: qsTr("Delete all history")
                            tokens: root.tokens

                            onClicked: {
                                root.historyController.clearHistory(true);
                                root.confirmingClear = false;
                            }
                        }
                    }
                }
            }
            RowLayout {
                Layout.topMargin: 8

                ActionButton {
                    text: qsTr("Done")
                    tokens: root.tokens

                    onClicked: root.hide()
                }
                ActionButton {
                    text: qsTr("Quit Reposco")
                    tokens: root.tokens

                    onClicked: Qt.quit()
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
