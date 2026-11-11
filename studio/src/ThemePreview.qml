import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: preview
    required property var host
    required property int draftVersion
    color: host.panel

    function setting(id, fallback) {
        const revision = draftVersion
        return studio.value(id) ?? fallback
    }

    function colorWithAlpha(color, alpha) {
        const solid = Qt.darker(color, 1)
        return Qt.rgba(solid.r, solid.g, solid.b, alpha)
    }

    function borderColor() {
        const value = String(setting("palette.hyprland_active_border", "accent")).split(/\s+/)[0]
        const hex = value.match(/^#([0-9a-f]{6})([0-9a-f]{2})?$/i)
            || value.match(/^rgba?\(([0-9a-f]{6})([0-9a-f]{2})?\)$/i)
        if (hex)
            return colorWithAlpha("#" + hex[1], hex[2] ? parseInt(hex[2], 16) / 255 : 1)
        return setting("palette." + value, accentColor)
    }

    readonly property color accentColor: setting("palette.accent", "#a4a7ff")
    readonly property color backgroundColor: setting("palette.background", "#29122a")
    readonly property color foregroundColor: setting("palette.foreground", "#dad4e4")
    readonly property color selectionColor: setting("palette.selection", "#393a62")
    readonly property int corners: Math.min(30, setting("windows.decoration.rounding", 0))
    readonly property int borderWidth: Math.min(8, setting("windows.general.border_size", 2))
    readonly property bool blur: setting("windows.decoration.blur.enabled", false)
    readonly property real windowOpacity: setting("windows.decoration.active_opacity", 1)
    readonly property real barOpacity: setting("shell.bar.background-alpha", 0.82)
    readonly property bool motion: setting("motion.animations.enabled", true) && setting("motion.windowsIn.enabled", true)
    readonly property real duration: setting("motion.windowsIn.speed", 4.1) * 100
    readonly property string animationStyle: setting("motion.windowsIn.style", "popin 87%")
    readonly property string curve: setting("motion.windowsIn.curve", "easeOutQuint")
    readonly property bool slides: animationStyle.startsWith("slide")
    readonly property int easing: {
        switch (curve) {
        case "linear": return Easing.Linear
        case "easeInOutCubic": return Easing.InOutCubic
        case "easeOutQuint": return Easing.OutQuint
        default: return Easing.OutCubic
        }
    }

    function replay() {
        if (visible && motion && studio.ready)
            entrance.restart()
    }

    Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: host.line }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 19
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: parent.width
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                Label {
                    text: qsTr("Desktop preview")
                    color: host.neonBlue
                    font.family: host.font.family
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
                ToolButton {
                    text: "↻"
                    font.pixelSize: 21
                    enabled: preview.motion && studio.ready
                    Accessible.name: qsTr("Replay window animation")
                    ToolTip.text: Accessible.name
                    ToolTip.visible: hovered
                    onClicked: preview.replay()
                }
            }

            Rectangle {
                id: sample
                objectName: "desktopPreview"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(310, width * 0.83)
                radius: desktopTheme.radius
                color: preview.backgroundColor
                border.color: host.tint(host.neonBlue, 0.65)
                clip: true

                Image {
                    anchors.fill: parent
                    source: studio.wallpaper
                    sourceSize.width: 700
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                }
                Rectangle { anchors.fill: parent; color: "#30120f1d" }

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: Math.max(12, Math.min(34, preview.setting("shell.bar.size-horizontal", 26) * 0.7))
                    color: preview.colorWithAlpha(preview.setting("shell.bar.background", "#391f3b"), preview.barOpacity)
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        spacing: 5
                        Repeater {
                            model: 4
                            Rectangle {
                                required property int index
                                width: index === 0 ? 13 : 4
                                height: 4
                                radius: 2
                                color: index === 0 ? preview.setting("shell.bar.active", "#de92d8") : preview.foregroundColor
                                opacity: index === 0 ? 1 : 0.55
                            }
                        }
                    }
                    Row {
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 5
                        Repeater {
                            model: 3
                            Rectangle {
                                width: 4; height: 4; radius: 2
                                color: preview.setting("shell.bar.text", "#dad4e4")
                            }
                        }
                    }
                }

                Rectangle {
                    id: sampleWindow
                    width: parent.width * 0.7
                    height: parent.height * 0.57
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 9
                    radius: preview.corners
                    color: "transparent"
                    border.width: preview.borderWidth
                    border.color: preview.borderColor()
                    layer.enabled: preview.setting("windows.decoration.shadow.enabled", false)
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: preview.setting("windows.decoration.shadow.color", "#1a1a1a")
                        shadowBlur: Math.min(1, preview.setting("windows.decoration.shadow.range", 4) / 24)
                    }
                    property real offset: 0
                    transform: Translate {
                        x: preview.animationStyle.endsWith("left") ? -sampleWindow.offset
                            : preview.animationStyle.endsWith("right") ? sampleWindow.offset : 0
                        y: preview.animationStyle.endsWith("left") || preview.animationStyle.endsWith("right") ? 0
                            : preview.animationStyle.endsWith("top") ? -sampleWindow.offset : sampleWindow.offset
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: preview.borderWidth
                        radius: Math.max(0, preview.corners - preview.borderWidth)
                        color: preview.colorWithAlpha(preview.backgroundColor, preview.windowOpacity)
                        clip: true

                        Image {
                            id: glass
                            visible: false
                            anchors.fill: parent
                            source: studio.wallpaper
                            sourceSize.width: 440
                            asynchronous: true
                            fillMode: Image.PreserveAspectCrop
                        }
                        MultiEffect {
                            anchors.fill: parent
                            visible: preview.blur && preview.windowOpacity < 1
                            source: glass
                            blurEnabled: visible
                            blurMax: 32
                            blur: Math.min(1, preview.setting("windows.decoration.blur.size", 4)
                                           * preview.setting("windows.decoration.blur.passes", 2) / 24)
                            brightness: preview.setting("windows.decoration.blur.brightness", 1) - 1
                            opacity: 1 - preview.windowOpacity
                            autoPaddingEnabled: false
                        }

                        Column {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10
                            Row {
                                spacing: 5
                                Repeater {
                                    model: ["red", "yellow", "green"]
                                    Rectangle {
                                        required property string modelData
                                        width: 6
                                        height: 6
                                        radius: 3
                                        color: preview.setting("palette." + modelData, "#a4a7ff")
                                    }
                                }
                            }
                            Label {
                                text: qsTr("Sample window")
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: preview.foregroundColor
                            }
                            Rectangle {
                                width: parent.width
                                height: 25
                                radius: Math.min(4, preview.corners)
                                color: preview.selectionColor
                                Label {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 7
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("Selected item")
                                    font.pixelSize: 11
                                    color: preview.foregroundColor
                                }
                            }
                        }
                    }
                }

                ParallelAnimation {
                    id: entrance
                    NumberAnimation {
                        target: sampleWindow
                        property: "scale"
                        from: preview.slides ? 1 : (Number(preview.animationStyle.match(/\d+/)?.[0]) / 100 || 0.8)
                        to: 1
                        duration: preview.duration
                        easing.type: preview.easing
                    }
                    NumberAnimation {
                        target: sampleWindow
                        property: "offset"
                        from: preview.slides ? 60 : 0
                        to: 0
                        duration: preview.duration
                        easing.type: preview.easing
                    }
                    NumberAnimation {
                        target: sampleWindow
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: preview.duration
                        easing.type: preview.easing
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 5
                Repeater {
                    model: ["accent", "red", "yellow", "green", "cyan", "blue", "magenta"]
                    Rectangle {
                        required property string modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 13
                        radius: Math.min(4, desktopTheme.radius)
                        color: preview.setting("palette." + modelData, "#a4a7ff")
                        Accessible.name: modelData + " " + String(color)
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("The preview shows the draft. Apply the theme to see the effects in Hyprland.")
                color: host.secondary
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                lineHeight: 1.25
            }
        }
    }
}
