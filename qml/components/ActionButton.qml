import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    property bool destructive: false
    required property StyleTokens tokens

    hoverEnabled: true
    leftPadding: 14
    padding: 10
    rightPadding: 14

    background: Rectangle {
        border.color: control.activeFocus ? control.tokens.accent : control.tokens.line
        border.width: control.activeFocus ? 2 : 1
        color: control.down || control.hovered ? control.tokens.selection : control.tokens.raised
        opacity: control.enabled ? 1 : 0.6
        radius: 8
    }
    contentItem: Text {
        color: control.enabled ? (control.destructive ? control.tokens.danger : control.tokens.foreground) : control.tokens.secondary
        font.pixelSize: 13
        horizontalAlignment: Text.AlignHCenter
        text: control.text
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignVCenter
    }
}
