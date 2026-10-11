import QtQuick
import QtQuick.Controls.Basic

CheckBox {
    id: control

    required property StyleTokens tokens

    padding: 6
    spacing: 12

    contentItem: Text {
        color: control.tokens.foreground
        font.pixelSize: 13
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
    }
    indicator: Rectangle {
        border.color: control.activeFocus ? control.tokens.accent : control.tokens.line
        border.width: control.activeFocus ? 2 : 1
        color: control.checked ? control.tokens.selection : control.tokens.raised
        height: 22
        radius: 6
        width: 22
        x: control.leftPadding
        y: (control.height - height) / 2

        Text {
            anchors.centerIn: parent
            color: control.tokens.foreground
            font.pixelSize: 16
            text: control.checked ? "✓" : ""
        }
    }
}
