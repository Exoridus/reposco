import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control

    property bool boxed: false
    property bool leadingIcon: true
    required property StyleTokens tokens
    property bool utilityMode: false

    Accessible.name: qsTr("Search clipboard history")
    color: utilityMode ? tokens.quickForeground : tokens.foreground
    font.family: "Segoe UI Variable Text"
    font.pixelSize: 15
    leftPadding: leadingIcon ? 38 : 0
    maximumLength: 256
    placeholderText: qsTr("Search clipboard history")
    placeholderTextColor: utilityMode ? tokens.quickSecondary : tokens.secondary
    rightPadding: 10
    selectByMouse: true
    selectedTextColor: tokens.selectedText
    selectionColor: tokens.selection

    background: Rectangle {
        border.color: control.boxed || control.tokens.highContrast ? control.tokens.line : "transparent"
        border.width: control.boxed || control.tokens.highContrast ? 1 : 0
        color: control.boxed ? control.tokens.hover : "transparent"
        radius: 6

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            color: control.tokens.secondary
            font.family: "Segoe Fluent Icons"
            font.pixelSize: 14
            text: "\uE721"
            visible: control.leadingIcon
        }
    }
}
