import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: root

    property string imageSource: ""
    property string kind: "Text"
    property string preview: ""
    property var rows: []
    property bool showSource: false
    required property StyleTokens tokens
    property bool truncated: false

    border.color: tokens.line
    border.width: 1
    color: tokens.raised
    radius: 12

    onImageSourceChanged: showSource = false
    onPreviewChanged: showSource = false
    onRowsChanged: showSource = false

    Text {
        anchors.left: parent.left
        anchors.margins: 14
        anchors.top: parent.top
        color: root.tokens.secondary
        font.letterSpacing: 1.2
        font.pixelSize: 10
        text: qsTr("PREVIEW") + " · " + root.kind + (root.truncated ? qsTr(" · bounded") : "")
        textFormat: Text.PlainText
    }
    ActionButton {
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.top: parent.top
        anchors.topMargin: 5
        padding: 4
        text: root.showSource ? qsTr("Preview") : qsTr("Plain text")
        tokens: root.tokens
        visible: root.rows.length > 0 || root.imageSource.length > 0

        onClicked: root.showSource = !root.showSource
    }
    ScrollView {
        id: scroll

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.bottomMargin: 12
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 35
        clip: true
        contentHeight: body.implicitHeight
        contentWidth: availableWidth

        Item {
            id: body

            implicitHeight: root.showSource || (root.rows.length === 0 && root.imageSource.length === 0) ? source.implicitHeight : root.rows.length > 0 ? table.implicitHeight : image.height
            width: scroll.availableWidth

            TextArea {
                id: source

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
                visible: root.showSource || (root.rows.length === 0 && root.imageSource.length === 0)
                width: parent.width
                wrapMode: TextEdit.Wrap
            }
            Image {
                id: image

                cache: false
                fillMode: Image.PreserveAspectFit
                height: root.kind === "Image" ? scroll.availableHeight : implicitWidth > 0 ? width * implicitHeight / implicitWidth : 0
                objectName: "previewImage"
                source: root.imageSource
                visible: !root.showSource && root.rows.length === 0 && root.imageSource.length > 0
                width: parent.width
            }
            Column {
                id: table

                spacing: 1
                visible: !root.showSource && root.rows.length > 0
                width: parent.width

                Repeater {
                    model: root.rows

                    delegate: Rectangle {
                        id: row

                        required property int index
                        required property var modelData

                        color: index === 0 ? root.tokens.selection : root.tokens.background
                        height: 28
                        objectName: "previewTableRow" + index
                        radius: 4
                        width: table.width

                        Row {
                            anchors.fill: parent

                            Repeater {
                                model: row.modelData

                                delegate: Text {
                                    required property string modelData

                                    color: root.tokens.foreground
                                    elide: Text.ElideRight
                                    font.pixelSize: 12
                                    height: row.height
                                    leftPadding: 8
                                    rightPadding: 8
                                    text: modelData
                                    textFormat: Text.PlainText
                                    verticalAlignment: Text.AlignVCenter
                                    width: row.width / Math.max(1, row.modelData.length)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
