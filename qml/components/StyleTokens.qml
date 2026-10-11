import QtQuick

QtObject {
    readonly property color accent: darkMode ? "#9fc9ef" : "#285c8b"
    readonly property color background: darkMode ? "#202a36" : "#edf1f5"
    readonly property color danger: darkMode ? "#ffb7b0" : "#a33129"
    property bool darkMode: true
    readonly property color foreground: darkMode ? "#f3f6fa" : "#172330"
    readonly property color line: darkMode ? "#435366" : "#c3ceda"
    readonly property color raised: darkMode ? "#293544" : "#ffffff"
    property bool reducedMotion: false
    readonly property color secondary: darkMode ? "#b0bdcc" : "#526376"
    readonly property color selection: darkMode ? "#344a60" : "#d9e6f3"
}
