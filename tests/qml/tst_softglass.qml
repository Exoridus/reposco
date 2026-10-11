import QtQuick
import QtTest
import "../../qml/components"

TestCase {
    function test_surface() {
        verify(surface.radius > 0);
        verify(surface.border.width > 0);
        verify(surface.color.a > 0.9);
    }

    name: "SoftGlass"
    when: windowShown

    SoftGlass {
        id: surface

        height: 200
        width: 300
    }
}
