pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Reposco
import "components"

FocusScope {
    id: root

    readonly property bool canChangePolicy: historyController !== null && historyController.available && !historyController.policyPending
    property string confirmationAction: ""
    property bool confirmingCapture: false
    property bool confirmingClear: false
    property var desktopSession: null
    property HistoryController historyController: null
    property int pendingRetentionDays: historyController ? historyController.retentionDays : 90
    property string ruleStatus: ""
    property bool showApplications: false
    property bool showPrivacy: false
    required property StyleTokens tokens

    signal backRequested

    function applyPolicy(six, eight) {
        if (historyController)
            historyController.setCapturePolicy(six, eight, historyController.excludedApplications);
    }
    function confirm(action) {
        confirmationAction = action;
        confirmation.open();
    }
    function focusView() {
        back.forceActiveFocus();
    }

    onConfirmingCaptureChanged: {
        if (confirmingCapture)
            confirm("capture");
    }
    onConfirmingClearChanged: {
        if (confirmingClear)
            confirm("clear");
    }
    onVisibleChanged: {
        if (!visible)
            confirmation.close();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            spacing: 10

            ActionButton {
                id: back

                Accessible.name: qsTr("Back to history")
                flat: true
                objectName: "settingsBack"
                text: qsTr("‹  History")
                tokens: root.tokens

                onClicked: root.backRequested()
            }
            Text {
                Layout.fillWidth: true
                color: root.tokens.foreground
                font.family: "Sitka Text"
                font.pixelSize: 20
                text: qsTr("Settings")
            }
            Text {
                color: root.tokens.secondary
                font.pixelSize: 12
                text: qsTr("Esc to return")
            }
        }
        ScrollView {
            id: scroll

            Layout.fillHeight: true
            Layout.fillWidth: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                spacing: 5
                width: scroll.availableWidth - 10

                Text {
                    Layout.bottomMargin: 5
                    color: root.tokens.secondary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    text: qsTr("Capture & Privacy")
                }
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        color: root.tokens.foreground
                        font.pixelSize: 12
                        text: root.historyController && root.historyController.captureActive ? qsTr("Capture is active") : qsTr("Capture is paused")
                    }
                    ActionButton {
                        enabled: root.canChangePolicy
                        objectName: "captureToggle"
                        text: root.historyController && root.historyController.captureActive ? qsTr("Pause") : qsTr("Resume...")
                        tokens: root.tokens

                        onClicked: {
                            if (root.historyController.captureActive)
                                root.historyController.setCaptureActive(false);
                            else
                                root.confirmingCapture = true;
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    color: root.tokens.secondary
                    font.pixelSize: 12
                    text: root.historyController ? root.historyController.status : qsTr("Capture unavailable")
                    textFormat: Text.PlainText
                    visible: !root.historyController || !root.historyController.available || root.historyController.policyPending
                    wrapMode: Text.WordWrap
                }
                PolicySwitch {
                    Layout.fillWidth: true
                    checked: root.historyController ? root.historyController.excludeSixDigitCodes : true
                    enabled: root.canChangePolicy
                    text: qsTr("Exclude six-digit OTP codes")
                    tokens: root.tokens

                    onToggled: root.applyPolicy(checked, root.historyController.excludeEightDigitCodes)
                }
                PolicySwitch {
                    Layout.fillWidth: true
                    checked: root.historyController !== null && root.historyController.excludeEightDigitCodes
                    enabled: root.canChangePolicy
                    text: qsTr("Also exclude eight-digit codes")
                    tokens: root.tokens

                    onToggled: root.applyPolicy(root.historyController.excludeSixDigitCodes, checked)
                }
                RowLayout {
                    Layout.fillWidth: true

                    ActionButton {
                        flat: true
                        text: root.showApplications ? qsTr("Application exclusions  −") : qsTr("Application exclusions  +")
                        tokens: root.tokens

                        onClicked: root.showApplications = !root.showApplications
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        flat: true
                        text: root.showPrivacy ? qsTr("Privacy details  −") : qsTr("Privacy details  +")
                        tokens: root.tokens

                        onClicked: root.showPrivacy = !root.showPrivacy
                    }
                }
                Text {
                    Layout.fillWidth: true
                    color: root.tokens.secondary
                    font.pixelSize: 12
                    text: qsTr("Secret markers and complete standalone codes are skipped before storage. Legitimate six- or eight-digit numbers are skipped too. Exclusion never changes the Windows clipboard. Unmarked passwords can still be captured; browser extensions cannot be identified reliably.")
                    textFormat: Text.PlainText
                    visible: root.showPrivacy
                    wrapMode: Text.WordWrap
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.showApplications

                    Text {
                        Layout.fillWidth: true
                        color: root.tokens.secondary
                        font.pixelSize: 12
                        text: qsTr("One full .exe path per line, up to 32. Unknown clipboard owners are skipped when rules exist. Browsers are not excluded by default.")
                        wrapMode: Text.WordWrap
                    }
                    TextArea {
                        id: paths

                        Layout.fillWidth: true
                        Layout.preferredHeight: 72
                        color: root.tokens.foreground
                        font.pixelSize: 12
                        objectName: "applicationPaths"
                        padding: 9
                        placeholderText: qsTr("C:/Program Files/Example/Vault.exe")
                        placeholderTextColor: root.tokens.secondary
                        selectedTextColor: root.tokens.selectedText
                        selectionColor: root.tokens.selection
                        text: root.historyController ? root.historyController.excludedApplications.join("\n") : ""
                        textFormat: TextEdit.PlainText
                        wrapMode: TextEdit.Wrap

                        background: Rectangle {
                            border.color: paths.activeFocus ? root.tokens.accent : root.tokens.line
                            color: root.tokens.well
                            radius: 7
                        }

                        onTextChanged: {
                            if (text.length > 16384)
                                text = text.slice(0, 16384);
                        }
                    }
                    ActionButton {
                        enabled: root.canChangePolicy
                        text: qsTr("Save exclusions")
                        tokens: root.tokens

                        onClicked: {
                            const rules = paths.text.split(/\r?\n/).map(value => value.trim()).filter(value => value.length > 0);
                            const saved = root.historyController.setCapturePolicy(root.historyController.excludeSixDigitCodes, root.historyController.excludeEightDigitCodes, rules);
                            root.ruleStatus = saved ? "" : qsTr("Use at most 32 absolute .exe paths, each up to 512 characters.");
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        color: root.tokens.danger
                        font.pixelSize: 12
                        text: root.ruleStatus
                        textFormat: Text.PlainText
                        visible: text.length > 0
                        wrapMode: Text.WordWrap
                    }
                }
                Rectangle {
                    Layout.bottomMargin: 9
                    Layout.fillWidth: true
                    Layout.topMargin: 9
                    color: root.tokens.line
                    implicitHeight: 1
                }
                Text {
                    Layout.bottomMargin: 5
                    color: root.tokens.secondary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    text: qsTr("Appearance & Interaction")
                }
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        color: root.tokens.foreground
                        font.pixelSize: 12
                        text: qsTr("Appearance")
                    }
                    Repeater {
                        model: ["System", "Light", "Dark"]

                        delegate: ActionButton {
                            required property string modelData

                            selected: root.tokens.appearance === modelData
                            text: modelData
                            tokens: root.tokens

                            onClicked: root.tokens.setAppearance(modelData)
                        }
                    }
                }
                PolicySwitch {
                    Layout.fillWidth: true
                    checked: root.tokens.reducedMotion
                    text: qsTr("Reduce motion")
                    tokens: root.tokens

                    onToggled: root.tokens.reducedMotion = checked
                }
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        color: root.tokens.foreground
                        font.pixelSize: 12
                        text: qsTr("Global hotkey")
                    }
                    Repeater {
                        model: ["Win+Alt+V", "Ctrl+Alt+V", "Ctrl+Shift+V"]

                        delegate: ActionButton {
                            required property string modelData

                            enabled: root.desktopSession !== null
                            selected: root.desktopSession !== null && root.desktopSession.shortcut === modelData
                            text: modelData
                            tokens: root.tokens

                            onClicked: root.desktopSession.setShortcut(modelData)
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    color: root.tokens.secondary
                    font.pixelSize: 12
                    text: root.desktopSession ? root.desktopSession.shortcutStatus : qsTr("Shortcut unavailable")
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    color: root.tokens.secondary
                    font.pixelSize: 12
                    text: qsTr("Appearance, motion and hotkey choices apply to this session.")
                    wrapMode: Text.WordWrap
                }
                Rectangle {
                    Layout.bottomMargin: 9
                    Layout.fillWidth: true
                    Layout.topMargin: 9
                    color: root.tokens.line
                    implicitHeight: 1
                }
                Text {
                    Layout.bottomMargin: 5
                    color: root.tokens.secondary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    text: qsTr("Storage")
                }
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        color: root.tokens.foreground
                        font.pixelSize: 12
                        text: root.historyController ? qsTr("Retention · %1 days").arg(root.historyController.retentionDays) : qsTr("Retention")
                    }
                    Repeater {
                        model: [7, 30, 90]

                        delegate: ActionButton {
                            required property int modelData

                            enabled: root.canChangePolicy
                            selected: root.pendingRetentionDays === modelData
                            text: qsTr("%1 days").arg(modelData)
                            tokens: root.tokens

                            onClicked: root.pendingRetentionDays = modelData
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4

                    Text {
                        Layout.fillWidth: true
                        color: root.tokens.secondary
                        font.pixelSize: 12
                        text: qsTr("Up to 1,000 items · 1 GiB")
                    }
                    ActionButton {
                        enabled: root.canChangePolicy
                        text: qsTr("Apply & remove older...")
                        tokens: root.tokens
                        visible: root.historyController !== null && root.pendingRetentionDays !== root.historyController.retentionDays

                        onClicked: root.confirm("retention")
                    }
                    ActionButton {
                        destructive: true
                        enabled: root.canChangePolicy
                        text: qsTr("Clear history...")
                        tokens: root.tokens

                        onClicked: root.confirmingClear = true
                    }
                }
            }
        }
    }
    Dialog {
        id: confirmation

        closePolicy: Popup.CloseOnEscape
        modal: true
        padding: 20
        width: Math.min(440, root.width - 24)
        x: (root.width - width) / 2
        y: Math.max(0, (root.height - height) / 2)

        background: Rectangle {
            border.color: root.tokens.line
            color: root.tokens.raised
            radius: 12
        }
        contentItem: ColumnLayout {
            spacing: 14

            Text {
                Layout.fillWidth: true
                color: root.tokens.foreground
                font.family: "Sitka Text"
                font.pixelSize: 20
                text: root.confirmationAction === "capture" ? qsTr("Start capturing?") : root.confirmationAction === "clear" ? qsTr("Clear all history?") : qsTr("Change retention?")
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: root.tokens.secondary
                font.pixelSize: 12
                text: root.confirmationAction === "capture" ? qsTr("Future copies are stored locally in encrypted history. Secret markers and your filters are respected, but unmarked passwords can still be saved. Resuming does not import the current clipboard.") : root.confirmationAction === "clear" ? qsTr("Capture will pause. Stored content and previews are deleted; the Windows clipboard stays unchanged. Logical deletion does not guarantee forensic erasure from SSDs or backups.") : qsTr("Keep %1 days and delete older entries?").arg(root.pendingRetentionDays)
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight

                ActionButton {
                    text: qsTr("Cancel")
                    tokens: root.tokens

                    onClicked: confirmation.close()
                }
                ActionButton {
                    destructive: root.confirmationAction === "clear"
                    enabled: root.canChangePolicy
                    objectName: "confirmSettingsAction"
                    text: root.confirmationAction === "capture" ? qsTr("Start capture") : root.confirmationAction === "clear" ? qsTr("Delete all history") : qsTr("Apply retention")
                    tokens: root.tokens

                    onClicked: {
                        if (root.confirmationAction === "capture")
                            root.historyController.setCaptureActive(true);
                        else if (root.confirmationAction === "clear")
                            root.historyController.clearHistory(true);
                        else
                            root.historyController.setRetentionDays(root.pendingRetentionDays, true);
                        confirmation.close();
                    }
                }
            }
        }

        onClosed: {
            root.confirmingCapture = false;
            root.confirmingClear = false;
        }
    }
}
