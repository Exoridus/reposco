import QtQuick
import QtQuick.Controls
import "components"

Window {
    id: root

    color: "transparent"
    height: 400
    title: qsTr("Reposco")
    visible: false
    width: 620

    onClosing: function (close) {
        close.accepted = false;
        root.hide();
    }

    SoftGlass {
        anchors.fill: parent

        Column {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 20

            Text {
                color: "#f5f7fb"
                font.bold: true
                font.pixelSize: 26
                objectName: "titleBar"
                text: qsTr("Reposco")
                textFormat: Text.PlainText
                width: parent.width
            }
            Text {
                color: "#c4cfdd"
                font.pixelSize: 16
                text: qsTr("Your clipboard, kept close.")
                textFormat: Text.PlainText
                width: parent.width
            }
            Rectangle {
                color: "#283648"
                height: 116
                radius: 12
                width: parent.width

                Text {
                    anchors.fill: parent
                    anchors.margins: 18
                    color: "#f5f7fb"
                    text: qsTr("Engineering preview\nClipboard capture is not enabled in this build.")
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                }
            }
            Text {
                color: "#c4cfdd"
                objectName: "hotkeyStatus"
                text: qsTr("Win+Alt+V opens this palette. Esc hides it.")
                textFormat: Text.PlainText
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Button {
                text: qsTr("Hide palette")

                onClicked: root.hide()
            }
            Button {
                text: qsTr("Quit Reposco")

                onClicked: Qt.quit()
            }
        }
    }
    Shortcut {
        sequence: "Escape"

        onActivated: root.hide()
    }
}
