pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Reposco
import "components"

Window {
    id: root

    property HistoryController historyController: null

    color: "transparent"
    height: 500
    minimumHeight: 400
    minimumWidth: 400
    title: qsTr("Reposco")
    visible: false
    width: 620

    onClosing: function (close) {
        close.accepted = false;
        root.hide();
    }

    SoftGlass {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10

            Image {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                fillMode: Image.PreserveAspectFit
                horizontalAlignment: Image.AlignLeft
                objectName: "titleBar"
                source: "qrc:/assets/branding/reposco-wordmark-primary.png"
            }
            Text {
                Layout.fillWidth: true
                color: "#c4cfdd"
                font.pixelSize: 13
                text: root.historyController ? root.historyController.status : qsTr("Capture unavailable.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: "#c4cfdd"
                font.pixelSize: 12
                text: qsTr("Starting capture stores future copied text locally in encrypted history. Unmarked passwords may still be captured. Pause at any time; resuming does not collect the current clipboard.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            Button {
                enabled: root.historyController !== null && root.historyController.available
                objectName: "captureToggle"
                text: root.historyController && root.historyController.captureActive ? qsTr("Pause capture") : qsTr("Start / resume text capture")

                onClicked: {
                    if (root.historyController)
                        root.historyController.setCaptureActive(!root.historyController.captureActive);
                }
            }
            ListView {
                id: historyList

                Layout.fillHeight: true
                Layout.fillWidth: true
                clip: true
                model: root.historyController ? root.historyController.model : null
                objectName: "historyList"
                spacing: 6

                ScrollBar.vertical: ScrollBar {
                }
                delegate: Rectangle {
                    id: row

                    required property double copiedAt
                    required property string excerpt

                    color: "#283648"
                    height: 66
                    radius: 8
                    width: historyList.width

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        Text {
                            color: "#f5f7fb"
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            objectName: "historyExcerpt"
                            text: row.excerpt.replace(/\n/g, " ")
                            textFormat: Text.PlainText
                            width: parent.width
                        }
                        Text {
                            color: "#c4cfdd"
                            font.pixelSize: 11
                            text: new Date(row.copiedAt).toLocaleTimeString()
                            textFormat: Text.PlainText
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    color: "#c4cfdd"
                    text: qsTr("No text history yet.")
                    textFormat: Text.PlainText
                    visible: historyList.count === 0
                }
            }
            Text {
                Layout.fillWidth: true
                color: "#c4cfdd"
                font.pixelSize: 12
                objectName: "hotkeyStatus"
                text: qsTr("Latest 100 entries. Win+Alt+V opens this palette. Esc hides it.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }
            RowLayout {
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
    }
    Shortcut {
        sequence: "Escape"

        onActivated: root.hide()
    }
}
