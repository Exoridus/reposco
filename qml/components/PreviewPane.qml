import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: root

    property string preview: ""
    required property StyleTokens tokens

    border.color: tokens.line
    border.width: 1
    color: tokens.raised
    radius: 12

    Text {
        anchors.left: parent.left
        anchors.margins: 14
        anchors.top: parent.top
        color: root.tokens.secondary
        font.letterSpacing: 1.5
        font.pixelSize: 10
        text: qsTr("PREVIEW")
    }
    ScrollView {
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.bottomMargin: 12
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 35
        clip: true

        TextArea {
            background: null
            color: root.tokens.foreground
            font.pixelSize: 13
            objectName: "previewText"
            padding: 0
            readOnly: true
            selectByMouse: true
            selectedTextColor: root.tokens.foreground
            selectionColor: root.tokens.selection
            text: root.preview
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
        }
    }
}
