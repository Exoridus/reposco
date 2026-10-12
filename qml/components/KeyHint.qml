pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    required property string keyText
    required property string label
    required property StyleTokens tokens

    spacing: 6

    Rectangle {
        color: root.tokens.selection
        implicitHeight: 20
        implicitWidth: key.implicitWidth + 10
        radius: 3

        Text {
            id: key

            anchors.centerIn: parent
            color: root.tokens.foreground
            font.pixelSize: 12
            font.weight: Font.DemiBold
            text: root.keyText
            textFormat: Text.PlainText
        }
    }
    Text {
        color: root.tokens.secondary
        font.pixelSize: 12
        text: root.label
        textFormat: Text.PlainText
    }
}
