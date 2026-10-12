pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ItemDelegate {
    id: root

    readonly property string age: {
        const minutes = Math.max(0, Math.floor((Date.now() - copiedAt) / 60000));
        return minutes < 1 ? qsTr("Just now") : minutes < 60 ? qsTr("%1m").arg(minutes) : minutes < 1440 ? qsTr("%1h").arg(Math.floor(minutes / 60)) : qsTr("%1d").arg(Math.floor(minutes / 1440));
    }
    property bool animate: false
    readonly property string byteSize: payloadBytes < 1024 ? qsTr("%1 B").arg(payloadBytes) : qsTr("%1 KiB").arg(Math.ceil(payloadBytes / 1024))
    required property double copiedAt
    required property string excerpt
    readonly property bool isUrl: /^https?:\/\//i.test(excerpt)
    required property string kind
    required property double payloadBytes
    property bool selected: false
    required property StyleTokens tokens
    property bool utilityMode: false

    Accessible.name: kind + ": " + excerpt
    Accessible.selected: selected
    hoverEnabled: true
    leftPadding: 12
    padding: 6
    rightPadding: 12

    background: Rectangle {
        border.color: root.tokens.highContrast ? root.tokens.foreground : "transparent"
        border.width: root.tokens.highContrast ? 1 : 0
        color: root.selected ? (root.utilityMode ? root.tokens.quickSelection : root.tokens.selection) : root.hovered ? (root.utilityMode ? root.tokens.quickHover : root.tokens.hover) : "transparent"
        radius: 4

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: root.utilityMode ? root.tokens.quickAccent : root.tokens.accent
            height: 16
            radius: 1.5
            visible: root.selected
            width: 3
        }
    }
    contentItem: RowLayout {
        spacing: 10

        Text {
            Layout.preferredWidth: 16
            color: root.selected && root.tokens.highContrast ? root.tokens.selectedText : root.utilityMode ? root.tokens.quickSecondary : root.tokens.secondary
            font.family: "Segoe Fluent Icons"
            font.pixelSize: 14
            text: root.kind === "Image" ? "\uEB9F" : root.kind === "Table" ? "\uE80A" : root.kind === "Markdown" ? "\uE8A5" : root.isUrl ? "\uE71B" : "\uE8A4"
        }
        Text {
            Layout.fillWidth: true
            color: root.selected && root.tokens.highContrast ? root.tokens.selectedText : root.utilityMode ? root.tokens.quickForeground : root.tokens.foreground
            elide: Text.ElideRight
            font.family: "Segoe UI Variable Text"
            font.pixelSize: 13
            objectName: "historyExcerpt"
            text: root.excerpt.replace(/\s+/g, " ")
            textFormat: Text.PlainText
        }
        Text {
            color: root.selected && root.tokens.highContrast ? root.tokens.selectedText : root.utilityMode ? root.tokens.quickSecondary : root.tokens.meta
            font.pixelSize: 12
            text: root.age
            textFormat: Text.PlainText
        }
    }
}
