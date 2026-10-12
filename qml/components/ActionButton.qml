import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    property bool destructive: false
    property bool selected: false
    required property StyleTokens tokens

    hoverEnabled: true
    leftPadding: 12
    padding: 7
    rightPadding: 12

    background: Rectangle {
        border.color: control.visualFocus ? control.tokens.accent : control.flat ? "transparent" : control.tokens.line
        border.width: 1
        color: control.selected || control.down ? control.tokens.selection : control.hovered ? control.tokens.hover : control.flat ? "transparent" : control.tokens.well
        opacity: control.enabled ? 1 : 0.5
        radius: 7
    }
    contentItem: Text {
        color: control.selected ? control.tokens.selectedText : control.enabled ? (control.destructive ? control.tokens.danger : control.tokens.foreground) : control.tokens.secondary
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
        text: control.text
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignVCenter
    }
}
