import QtQuick
import QtTest
import "../../qml/components"

TestCase {
    function test_appearanceAndUtilityContrast() {
        for (const appearance of ["Dark", "Light", "System"]) {
            style.appearance = appearance;
            compare(style.darkMode, appearance === "Dark" || (appearance === "System" && style.systemDark));
            for (const accent of ["#202020", "#f9f9f9", "#0078d4"]) {
                style.systemAccent = accent;
                const foreground = style.luminance(style.quickAccent);
                const background = style.luminance(style.quickBackground);
                verify((Math.max(foreground, background) + 0.05) / (Math.min(foreground, background) + 0.05) >= 3);
            }
        }
    }
    function test_fallbackAndMotion() {
        style.nativeMaterial = true;
        verify(surface.color.a < 0.8);
        style.highContrast = true;
        compare(surface.color.a, 1);
        style.highContrast = false;
        style.nativeMaterial = false;
        style.reducedMotion = true;
        compare(style.duration, 0);
        style.reducedMotion = false;
        style.systemReducedMotion = true;
        compare(style.duration, 0);
        style.systemReducedMotion = false;
    }
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
        tokens: style
        width: 300
    }
}
