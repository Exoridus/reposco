pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: root

    readonly property bool canShowSource: kind !== "Image" && (rows.length > 0 || imageSource.length > 0)
    property string imageSource: ""
    property string kind: "Text"
    property string metadata: ""
    property string preview: ""
    property var rows: []
    property bool showSource: false
    required property StyleTokens tokens
    property bool truncated: false

    border.color: tokens.line
    border.width: 1
    color: tokens.well
    radius: 6

    onImageSourceChanged: showSource = false
    onPreviewChanged: showSource = false
    onRowsChanged: showSource = false

    Text {
        anchors.left: parent.left
        anchors.margins: 14
        anchors.top: parent.top
        color: root.tokens.secondary
        font.capitalization: Font.SmallCaps
        font.family: "Sitka Small"
        font.letterSpacing: 0.6
        font.pixelSize: 14
        text: root.kind + (root.truncated ? qsTr(" · bounded") : "")
        textFormat: Text.PlainText
    }
    Text {
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.top: parent.top
        anchors.topMargin: 30
        color: root.tokens.meta
        font.pixelSize: 12
        text: root.metadata
        textFormat: Text.PlainText
    }
    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.top: parent.top
        anchors.topMargin: 10
        color: root.tokens.hover
        height: 26
        radius: 5
        visible: root.canShowSource
        width: 126

        Row {
            anchors.fill: parent
            anchors.margins: 2
            spacing: 2

            ActionButton {
                flat: true
                height: parent.height
                padding: 2
                selected: !root.showSource
                text: qsTr("Rendered")
                tokens: root.tokens
                width: 69

                onClicked: root.showSource = false
            }
            ActionButton {
                flat: true
                height: parent.height
                padding: 2
                selected: root.showSource
                text: qsTr("Plain")
                tokens: root.tokens
                width: 51

                onClicked: root.showSource = true
            }
        }
    }
    ScrollView {
        id: scroll

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.bottomMargin: 12
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 48
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
                selectedTextColor: root.tokens.selectedText
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

                        color: index === 0 ? root.tokens.selection : index % 2 ? root.tokens.well : "transparent"
                        height: 30
                        objectName: "previewTableRow" + index
                        radius: 0
                        width: table.width

                        Row {
                            anchors.fill: parent

                            Repeater {
                                model: row.modelData

                                delegate: Text {
                                    required property string modelData

                                    color: row.index === 0 ? root.tokens.selectedText : root.tokens.foreground
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
