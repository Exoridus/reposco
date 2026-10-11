import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ItemDelegate {
    id: root

    readonly property string age: {
        const minutes = Math.max(0, Math.floor((Date.now() - copiedAt) / 60000));
        return minutes < 1 ? qsTr("Just now") : minutes < 60 ? qsTr("%1m ago").arg(minutes) : minutes < 1440 ? qsTr("%1h ago").arg(Math.floor(minutes / 60)) : qsTr("%1d ago").arg(Math.floor(minutes / 1440));
    }
    property bool animate: false
    readonly property string byteSize: payloadBytes < 1024 ? qsTr("%1 B").arg(payloadBytes) : qsTr("%1 KiB").arg(Math.ceil(payloadBytes / 1024))
    required property double copiedAt
    required property string excerpt
    readonly property bool isUrl: /^https?:\/\//i.test(excerpt)
    required property double payloadBytes
    property bool selected: false
    required property StyleTokens tokens

    hoverEnabled: true
    leftPadding: 12
    padding: 8
    rightPadding: 24

    background: Rectangle {
        border.color: root.selected || root.activeFocus ? root.tokens.line : "transparent"
        border.width: 1
        color: root.selected || root.hovered ? root.tokens.selection : "transparent"
        radius: 9

        Behavior on color {
            enabled: root.animate && !root.tokens.reducedMotion

            ColorAnimation {
                duration: 100
            }
        }
    }
    contentItem: RowLayout {
        spacing: 12

        Text {
            Layout.preferredWidth: 18
            color: root.tokens.secondary
            font.pixelSize: 14
            text: root.isUrl ? "↗" : "T"
        }
        Text {
            Layout.fillWidth: true
            color: root.tokens.foreground
            elide: Text.ElideRight
            font.pixelSize: 13
            objectName: "historyExcerpt"
            text: root.excerpt.replace(/\s+/g, " ")
            textFormat: Text.PlainText
        }
        Text {
            color: root.tokens.secondary
            font.pixelSize: 11
            text: (root.isUrl ? qsTr("URL") : qsTr("Text")) + " · " + root.age + " · " + root.byteSize
            textFormat: Text.PlainText
        }
    }
}
