import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property bool captureActive: false
    property bool searching: false
    required property StyleTokens tokens
    property bool utilityMode: false

    signal settingsRequested

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 10

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: 28
            Layout.preferredWidth: 28
            border.color: root.tokens.line
            color: root.tokens.raised
            radius: 6

            Image {
                anchors.fill: parent
                anchors.margins: 3
                fillMode: Image.PreserveAspectFit
                source: "qrc:/assets/branding/reposco-icon-primary.png"
                sourceSize.height: 64
                sourceSize.width: 64
            }
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            color: root.tokens.foreground
            font.family: root.utilityMode ? "Segoe UI Variable Text" : "Sitka Subheading"
            font.pixelSize: root.utilityMode ? 13 : 20
            font.weight: Font.DemiBold
            text: root.searching ? qsTr("No matching history") : qsTr("Nothing saved yet")
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            color: root.tokens.secondary
            font.pixelSize: 12
            text: root.searching ? qsTr("Try a different search.") : root.captureActive ? qsTr("Your next copied content will appear here.") : qsTr("Choose what Reposco captures in Settings.")
        }
        ActionButton {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Open settings")
            tokens: root.tokens
            visible: !root.searching && !root.captureActive

            onClicked: root.settingsRequested()
        }
    }
}
