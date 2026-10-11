import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control

    required property StyleTokens tokens

    color: tokens.foreground
    font.pixelSize: 16
    leftPadding: 16
    maximumLength: 256
    placeholderText: qsTr("Search clipboard history...")
    placeholderTextColor: tokens.secondary
    rightPadding: 16
    selectByMouse: true
    selectedTextColor: tokens.foreground
    selectionColor: tokens.selection

    background: Rectangle {
        border.color: control.activeFocus ? control.tokens.accent : control.tokens.line
        border.width: 1
        color: control.tokens.raised
        radius: 11
    }
}
