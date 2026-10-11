import QtQuick

Rectangle {
    id: root

    required property StyleTokens tokens

    border.color: tokens.line
    border.width: 1
    radius: 22

    gradient: Gradient {
        GradientStop {
            color: root.tokens.darkMode ? "#2b3745" : "#f7f9fc"
            position: 0
        }
        GradientStop {
            color: root.tokens.background
            position: 1
        }
    }
}
