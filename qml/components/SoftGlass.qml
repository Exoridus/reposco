import QtQuick

Rectangle {
    property bool materialActive: tokens.nativeMaterial
    property color solidColor: tokens.background
    property color tintColor: tokens.surfaceTint
    required property StyleTokens tokens

    antialiasing: true
    border.color: tokens.line
    border.width: 1
    color: materialActive && !tokens.highContrast ? tintColor : solidColor
    radius: 8
}
