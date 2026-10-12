import QtQuick

QtObject {
    id: root

    readonly property color accent: highContrast ? systemPalette.highlight : darkMode ? "#a9c4da" : "#3b6382"
    property string appearance: "System"
    readonly property color background: highContrast ? systemPalette.window : darkMode ? "#1c1b19" : "#f7f5f0"
    readonly property color caution: highContrast ? foreground : darkMode ? "#f2c66d" : "#8a5a00"
    readonly property color danger: highContrast ? foreground : darkMode ? "#efb0a8" : "#a13730"
    property bool darkMode: appearance === "Dark" || (appearance === "System" && systemDark)
    readonly property int duration: reducedMotion || systemReducedMotion ? 0 : 120
    readonly property color foreground: highContrast ? systemPalette.windowText : darkMode ? "#ede8e0" : "#1f1e1c"
    property bool highContrast: false
    readonly property color hover: highContrast ? systemPalette.button : darkMode ? "#0dfffaf0" : "#0a1f1e1c"
    readonly property color line: highContrast ? foreground : darkMode ? "#17fffaf0" : "#141f1e1c"
    readonly property color meta: highContrast ? foreground : darkMode ? "#948e85" : "#6b665f"
    property bool nativeMaterial: false
    readonly property color quickAccent: highContrast ? systemPalette.highlight : contrastAccent(systemAccent, quickBackground)
    readonly property color quickBackground: highContrast ? systemPalette.window : darkMode ? "#202020" : "#f9f9f9"
    readonly property color quickForeground: highContrast ? foreground : darkMode ? "#ededed" : "#1a1a1a"
    readonly property color quickHover: darkMode ? "#0bffffff" : "#09000000"
    readonly property color quickSecondary: highContrast ? foreground : darkMode ? "#a8a8a8" : "#5d5d5d"
    readonly property color quickSelection: highContrast ? systemPalette.highlight : darkMode ? "#16ffffff" : "#0f000000"
    readonly property color quickTint: darkMode ? "#b8202020" : "#b8f9f9f9"
    readonly property color raised: highContrast ? systemPalette.base : darkMode ? "#252321" : "#ffffff"
    property bool reducedMotion: false
    readonly property color secondary: highContrast ? foreground : darkMode ? "#b4aea5" : "#55514b"
    readonly property color selectedText: highContrast ? systemPalette.highlightedText : foreground
    readonly property color selection: highContrast ? systemPalette.highlight : darkMode ? "#24a9c4da" : "#1c3b6382"
    readonly property color surfaceTint: darkMode ? "#b31c1b19" : "#bdf7f5f0"
    property color systemAccent: darkMode ? "#60cdff" : "#005fb8"
    property bool systemDark: true
    property SystemPalette systemPalette: SystemPalette {
        colorGroup: SystemPalette.Active
    }
    property bool systemReducedMotion: false
    readonly property color well: highContrast ? systemPalette.base : darkMode ? "#0dfffaf0" : "#b8ffffff"

    function contrastAccent(color, background) {
        const backdrop = luminance(background);
        let result = color;
        for (let step = 0; step < 20; ++step) {
            const value = luminance(result);
            if ((Math.max(value, backdrop) + 0.05) / (Math.min(value, backdrop) + 0.05) >= 3)
                break;
            const target = darkMode ? 1 : 0;
            result = Qt.rgba(result.r + (target - result.r) * 0.1, result.g + (target - result.g) * 0.1, result.b + (target - result.b) * 0.1, 1);
        }
        return result;
    }
    function luminance(color) {
        function linear(value) {
            return value <= 0.04045 ? value / 12.92 : Math.pow((value + 0.055) / 1.055, 2.4);
        }
        return 0.2126 * linear(color.r) + 0.7152 * linear(color.g) + 0.0722 * linear(color.b);
    }
    function setAppearance(value) {
        appearance = value;
        darkMode = Qt.binding(function () {
            return root.appearance === "Dark" || (root.appearance === "System" && root.systemDark);
        });
    }
}
