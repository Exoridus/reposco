import QtQuick
import QtTest
import "../../qml/components"

TestCase {
    function test_richPreviewAndSourceFallback() {
        preview.rows = [["Item", "Count"], ["Alpha", "2"]];
        tryVerify(function () {
            return findChild(preview, "previewTableRow0") !== null;
        });
        preview.showSource = true;
        verify(findChild(preview, "previewText").visible);
        preview.rows = [];
        preview.kind = "Image";
        preview.imageSource = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGPQqLjzHwAExAJ80sraTAAAAABJRU5ErkJggg==";
        tryCompare(findChild(preview, "previewImage"), "status", Image.Ready);
        verify(findChild(preview, "previewImage").visible);
        verify(findChild(preview, "previewImage").height <= preview.height - 35);
        preview.showSource = true;
        verify(!findChild(preview, "previewImage").visible);
    }
    function test_surface() {
        verify(surface.radius > 0);
        verify(surface.border.width > 0);
        verify(surface.color.a > 0.9);
    }

    height: 240
    name: "SoftGlass"
    visible: true
    when: windowShown
    width: 640

    StyleTokens {
        id: style
    }
    PreviewPane {
        id: preview

        height: 134
        preview: "Synthetic source"
        tokens: style
        width: 600
    }
    SoftGlass {
        id: surface

        height: 200
        width: 300
    }
}
