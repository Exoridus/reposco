import QtQuick
import QtQuick.Controls.Basic

Switch {
    id: control

    required property StyleTokens tokens

    implicitHeight: 32
    padding: 0
    spacing: 12

    contentItem: Text {
        color: control.enabled ? control.tokens.foreground : control.tokens.secondary
        font.pixelSize: 12
        rightPadding: control.indicator.width + control.spacing
        text: control.text
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
    }
    indicator: Rectangle {
        border.color: control.visualFocus ? control.tokens.accent : control.tokens.line
        border.width: 1
        color: control.checked ? control.tokens.accent : control.tokens.well
        height: 18
        radius: 9
        width: 32
        x: control.width - width
        y: (control.height - height) / 2

        Rectangle {
            color: control.checked ? control.tokens.background : control.tokens.secondary
            height: 12
            radius: 6
            width: 12
            x: control.checked ? 17 : 3
            y: 3

            Behavior on x {
                NumberAnimation {
                    duration: control.tokens.duration
                }
            }
        }
    }
}
