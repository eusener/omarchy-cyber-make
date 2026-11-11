pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1232
    height: 790
    minimumWidth: 690
    minimumHeight: 550
    visible: true
    title: "Cyber Make Studio"
    color: base
    font.family: "JetBrains Mono"
    font.pixelSize: 14
    Material.theme: Material.Dark
    Material.accent: accent
    Material.primary: panel
    Material.background: panel
    Material.foreground: ink
    Material.roundedScale: desktopTheme.radius

    // All editor surfaces and highlights follow the theme being edited.
    function paletteColor(name, fallback) {
        const revision = draftVersion
        return studio.value("palette." + name) ?? fallback
    }

    function tint(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }

    readonly property color base: paletteColor("darker_background", "#130114")
    readonly property color panel: paletteColor("dark_background", "#1c071e")
    readonly property color raised: paletteColor("background", "#29122a")
    readonly property color line: paletteColor("muted", "#523f69")
    readonly property color ink: paletteColor("bright_foreground", "#eee8f8")
    readonly property color secondary: paletteColor("foreground", "#dad4e4")
    readonly property color danger: paletteColor("bright_red", "#f9abf3")
    readonly property color accent: paletteColor("accent", "#a4a7ff")
    readonly property color neonPink: paletteColor("bright_red", "#f9abf3")
    readonly property color neonBlue: paletteColor("bright_blue", "#69dcff")
    readonly property color neonGreen: paletteColor("bright_cyan", "#6fe5b6")
    readonly property color neonYellow: paletteColor("bright_green", "#decd62")
    readonly property color accentInk: base
    readonly property var categoryColors: ({
        palette: neonPink, windows: neonBlue, motion: accent,
        shell: neonGreen, identity: neonYellow
    })
    readonly property color categoryAccent: categoryColors[category]
    readonly property bool wide: width >= 1030
    property bool showPreview: false
    property bool closingConfirmed: false
    property string category: "palette"
    property int catalogVersion: 0
    property int draftVersion: 0
    property var invalid: ({})
    property string requestedAction: ""
    readonly property int invalidCount: Object.keys(invalid).length
    readonly property bool unsaved: studio.dirty || invalidCount > 0

    readonly property var pages: [
        { id: "palette", label: qsTr("Palette"), icon: "◈",
          intro: qsTr("Set the colors used by the desktop and themed apps.") },
        { id: "windows", label: qsTr("Windows"), icon: "▣",
          intro: qsTr("Adjust corners, gaps, opacity, blur, and shadows.") },
        { id: "motion", label: qsTr("Animations"), icon: "∿",
          intro: qsTr("Set the duration, curve, and style of Hyprland animations.") },
        { id: "shell", label: qsTr("Omarchy shell"), icon: "▤",
          intro: qsTr("Customize the bar, launcher, menus, notifications, and other surfaces.") },
        { id: "identity", label: qsTr("Wallpaper & icons"), icon: "▧",
          intro: qsTr("Choose a wallpaper and set the icon theme. Add images or videos to the wallpaper folder.") }
    ]
    readonly property var page: pages.find(item => item.id === category)
    readonly property var sections: {
        const revision = catalogVersion
        const lang = desktopTheme.language
        const query = search.text.trim().toLocaleLowerCase()
        const items = studio.fields(category)
        const groups = []
        for (const field of items) {
            if (query && ![field.label, field.id, field.section].some(s => s.toLocaleLowerCase().includes(query)))
                continue
            let group = groups.find(item => item.title === field.section)
            if (!group) {
                group = { title: field.section, items: [] }
                groups.push(group)
            }
            group.items.push(field)
        }
        return groups
    }

    function trackError(id, error, text) {
        const fields = Object.assign({}, invalid)
        if (error)
            fields[id] = { error: error, text: text }
        else
            delete fields[id]
        invalid = fields
    }

    function flushEditor() {
        const editor = activeFocusItem
        if (editor && typeof editor.commitSetting === "function")
            editor.commitSetting()
    }

    function applyDraft() {
        flushEditor()
        if (studio.ready && !studio.busy && invalidCount === 0)
            studio.apply()
    }

    function selectCategory(id) {
        flushEditor()
        category = id
        search.clear()
        showPreview = false
        settingsFlickable.contentY = 0
    }

    function reviewInvalidSetting() {
        const id = Object.keys(invalid)[0]
        if (!id)
            return
        selectCategory(id.split(".")[0])
        Qt.callLater(() => {
            const sections = settingsColumn.children
            for (const section of sections) {
                for (const row of section.children) {
                    if (row.objectName === "setting_" + id) {
                        row.focusEditor()
                        return
                    }
                }
            }
        })
    }

    function runAction(action) {
        invalid = ({})
        if (action === "close") {
            closingConfirmed = true
            close()
        } else if (action === "defaults") {
            studio.defaults()
        } else {
            studio.reload()
        }
    }

    function confirmAction(action) {
        flushEditor()
        requestedAction = action
        if (action === "defaults" || unsaved)
            confirm.open()
        else
            runAction(action)
    }

    onClosing: close => {
        flushEditor()
        if (studio.busy || (unsaved && !closingConfirmed)) {
            close.accepted = false
            if (!studio.busy)
                confirmAction("close")
        }
    }

    Connections {
        target: studio
        function onChanged() { window.draftVersion += 1 }
        function onCatalogChanged() { window.catalogVersion += 1 }
    }

    Shortcut { sequence: "Ctrl+Return"; onActivated: window.applyDraft() }
    Shortcut { sequence: "Ctrl+Shift+Z"; enabled: !window.busyEditing; onActivated: studio.redo() }
    Shortcut { sequence: "Ctrl+Z"; enabled: !window.busyEditing; onActivated: studio.undo() }
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: { window.showPreview = false; search.forceActiveFocus() }
    }
    Shortcut { sequence: "Ctrl+Q"; onActivated: window.close() }
    Shortcut { sequence: "F1"; onActivated: shortcuts.open() }
    Shortcut { sequence: "?"; enabled: !window.busyEditing; onActivated: shortcuts.open() }
    Shortcut { sequence: "Q"; enabled: !window.busyEditing; onActivated: window.close() }
    Shortcut {
        sequence: "Space"
        enabled: window.activeFocusItem === null || window.activeFocusItem === focusRoot
        onActivated: preview.replay()
    }
    readonly property bool busyEditing: activeFocusItem instanceof TextField || activeFocusItem instanceof TextArea

    Dialog {
        id: confirm
        modal: true
        focus: true
        anchors.centerIn: parent
        width: Math.min(window.width - 40, 444)
        title: window.requestedAction === "defaults" ? qsTr("Restore original settings?")
            : qsTr("Discard unsaved changes?")
        contentItem: Label {
            wrapMode: Text.WordWrap
            text: window.requestedAction === "defaults"
                ? qsTr("This clears your draft overrides. Apply the draft to update the desktop.")
                : qsTr("Changes since the last application will be discarded.")
        }
        footer: DialogButtonBox {
            Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            Button {
                text: window.requestedAction === "defaults" ? qsTr("Restore") : qsTr("Discard")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
        onAccepted: window.runAction(window.requestedAction)
    }

    Dialog {
        id: details
        anchors.centerIn: parent
        width: Math.min(window.width - 48, 660)
        height: Math.min(window.height - 48, 340)
        modal: true
        title: qsTr("Application details")
        standardButtons: Dialog.Close
        ScrollView {
            anchors.fill: parent
            TextArea { text: studio.status; readOnly: true; wrapMode: TextEdit.WrapAnywhere }
        }
    }

    Dialog {
        id: shortcuts
        anchors.centerIn: parent
        width: 386
        modal: true
        title: qsTr("Keyboard shortcuts")
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 12
            Repeater {
                model: [
                    [qsTr("Apply theme"), "Ctrl + Enter"],
                    [qsTr("Undo"), "Ctrl + Z"],
                    [qsTr("Redo"), "Ctrl + Shift + Z"],
                    [qsTr("Search this section"), "Ctrl + F"],
                    [qsTr("Close"), "Ctrl + Q"],
                    [qsTr("Keyboard shortcuts"), "F1"]
                ]
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label { Layout.fillWidth: true; text: parent.modelData[0] }
                    Label { text: parent.modelData[1]; color: window.accent; font.family: window.font.family }
                }
            }
        }
    }

    Item {
        id: focusRoot
        anchors.fill: parent
        focus: true
        LayoutMirroring.enabled: Qt.locale(desktopTheme.language).textDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                color: window.panel
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 25
                    anchors.rightMargin: 20
                    spacing: 14
                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: desktopTheme.radius
                        color: window.tint(window.neonPink, 0.12)
                        border.color: window.neonPink
                        Label {
                            anchors.centerIn: parent
                            text: "◈"
                            color: window.neonPink
                            font.pixelSize: 21
                            Accessible.ignored: true
                        }
                    }
                    Label {
                        text: "Cyber Make"
                        font.family: window.font.family
                        font.weight: Font.DemiBold
                        font.pixelSize: 24
                        color: window.neonPink
                    }
                    Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 22; color: window.line }
                    Label {
                        text: qsTr("Theme Studio")
                        color: window.neonBlue
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    ToolButton {
                        visible: !window.wide
                        text: window.showPreview ? qsTr("Settings") : qsTr("Preview")
                        onClicked: {
                            window.flushEditor()
                            window.showPreview = !window.showPreview
                        }
                    }
                    ToolButton {
                        text: "?"
                        Accessible.name: qsTr("Keyboard shortcuts")
                        ToolTip.visible: hovered
                        ToolTip.text: Accessible.name
                        onClicked: shortcuts.open()
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 2
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: window.neonPink }
                        GradientStop { position: 0.5; color: window.accent }
                        GradientStop { position: 1; color: window.neonBlue }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: window.wide ? 226 : 194
                    color: window.panel
                    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: window.line }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        anchors.topMargin: 20
                        spacing: 5

                        Repeater {
                            model: window.pages
                            delegate: ItemDelegate {
                                id: nav
                                required property var modelData
                                readonly property color neon: window.categoryColors[modelData.id]
                                objectName: "navigate_" + modelData.id
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                highlighted: window.category === modelData.id
                                Accessible.name: modelData.label
                                onClicked: window.selectCategory(modelData.id)
                                background: Rectangle {
                                    radius: desktopTheme.radius
                                    color: nav.down ? window.tint(nav.neon, 0.23)
                                        : nav.highlighted ? window.tint(nav.neon, 0.14)
                                        : nav.hovered || nav.visualFocus ? window.tint(nav.neon, 0.08) : "transparent"
                                    border.color: nav.visualFocus || nav.highlighted ? nav.neon : "transparent"
                                    border.width: nav.visualFocus ? 2 : 1
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 2
                                        height: 22
                                        color: nav.neon
                                        visible: nav.highlighted
                                    }
                                }
                                contentItem: RowLayout {
                                    spacing: 11
                                    Label {
                                        text: nav.modelData.icon
                                        font.pixelSize: 19
                                        color: nav.neon
                                        Accessible.ignored: true
                                    }
                                    Label {
                                        text: nav.modelData.label
                                        color: nav.highlighted ? nav.neon : window.ink
                                        font.pixelSize: 13
                                        font.weight: nav.highlighted ? Font.DemiBold : Font.Normal
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Reload saved")
                            flat: true
                            enabled: !studio.busy
                            onClicked: window.confirmAction("reload")
                        }
                        Button {
                            Layout.fillWidth: true
                            text: qsTr("Restore original")
                            flat: true
                            enabled: studio.ready && !studio.busy
                            onClicked: window.confirmAction("defaults")
                        }
                    }
                }

                ColumnLayout {
                    visible: window.wide || !window.showPreview
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: 23
                        Layout.bottomMargin: 14
                        spacing: 5
                        Label {
                            text: window.page.label
                            color: window.categoryAccent
                            font.family: window.font.family
                            font.pixelSize: 28
                            font.weight: Font.DemiBold
                        }
                        Label {
                            Layout.fillWidth: true
                            color: window.secondary
                            text: window.page.intro
                            wrapMode: Text.WordWrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 7
                            spacing: 0
                            TextField {
                                id: search
                                objectName: "search"
                                Layout.fillWidth: true
                                placeholderText: qsTr("Search this section")
                                placeholderTextColor: window.secondary
                                color: window.ink
                                selectionColor: window.accent
                                selectedTextColor: window.accentInk
                                Accessible.name: placeholderText
                                selectByMouse: true
                            }
                            ToolButton {
                                visible: search.text.length > 0
                                text: "×"
                                Accessible.name: qsTr("Clear search")
                                ToolTip.visible: hovered
                                ToolTip.text: Accessible.name
                                onClicked: { search.clear(); search.forceActiveFocus() }
                            }
                            Button {
                                visible: window.category === "identity"
                                enabled: studio.ready && !studio.busy
                                text: qsTr("Open wallpaper folder")
                                onClicked: studio.openWallpaperFolder()
                            }
                        }
                    }

                    ScrollView {
                        id: scroll
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: availableWidth
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                        clip: true
                        readonly property Flickable flickable: contentItem as Flickable

                        ColumnLayout {
                            id: settingsColumn
                            width: scroll.availableWidth
                            spacing: 16

                            Label {
                                Layout.fillWidth: true
                                Layout.margins: 23
                                visible: window.sections.length === 0
                                text: !studio.ready ? qsTr("The theme could not be loaded.")
                                                    : qsTr("No matching settings.")
                                color: window.secondary
                                wrapMode: Text.WordWrap
                            }

                            Repeater {
                                model: window.sections
                                delegate: ColumnLayout {
                                    id: section
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 23
                                    Layout.rightMargin: 17
                                    spacing: 3

                                    Label {
                                        Layout.fillWidth: true
                                        Layout.bottomMargin: 4
                                        text: section.modelData.title
                                        color: window.neonBlue
                                        font.weight: Font.DemiBold
                                        font.pixelSize: 13
                                    }

                                    Repeater {
                                        model: section.modelData.items
                                        delegate: SettingRow {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            field: modelData
                                            host: window
                                            draftVersion: window.draftVersion
                                            invalidData: window.invalid[field.id] || null
                                            enabled: !studio.busy
                                            onFocused: item => {
                                                const view = scroll.flickable
                                                const y = item.mapToItem(view.contentItem, 0, 0).y
                                                if (y < view.contentY)
                                                    view.contentY = y
                                                else if (y + item.height > view.contentY + view.height)
                                                    view.contentY = y + item.height - view.height
                                            }
                                        }
                                    }
                                }
                            }
                            Item { Layout.preferredHeight: 7 }
                        }
                    }
                }

                ThemePreview {
                    id: preview
                    host: window
                    visible: window.wide || window.showPreview
                    Layout.fillHeight: true
                    Layout.fillWidth: !window.wide
                    Layout.preferredWidth: 344
                    draftVersion: window.draftVersion
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 65
                color: window.panel
                Rectangle { width: parent.width; height: 1; color: window.line }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 23
                    anchors.rightMargin: 20
                    spacing: 9
                    Rectangle {
                        Layout.preferredWidth: 7
                        Layout.preferredHeight: 7
                        radius: width / 2
                        color: studio.failed || window.invalidCount ? window.danger : window.neonGreen
                        Accessible.ignored: true
                    }
                    Label {
                        Layout.fillWidth: true
                        objectName: "status"
                        elide: Text.ElideRight
                        color: studio.failed || window.invalidCount ? window.danger : window.secondary
                        text: window.invalidCount ? qsTr("Fix the highlighted settings before applying.")
                            : studio.status ? studio.status
                            : studio.dirty ? qsTr("Unsaved changes")
                            : studio.personalActive ? qsTr("Personal theme is active")
                            : qsTr("Ready to apply the personal theme")
                        Accessible.role: Accessible.StaticText
                    }
                    ToolButton {
                        visible: window.invalidCount > 0
                        text: qsTr("Review setting")
                        onClicked: window.reviewInvalidSetting()
                    }
                    ToolButton {
                        visible: studio.failed && studio.status.length > 0
                        text: qsTr("Details")
                        onClicked: details.open()
                    }
                    ToolButton {
                        text: "↶"
                        font.pixelSize: 19
                        Accessible.name: qsTr("Undo")
                        ToolTip.text: Accessible.name
                        ToolTip.visible: hovered
                        enabled: studio.canUndo && !studio.busy
                        onClicked: studio.undo()
                    }
                    ToolButton {
                        text: "↷"
                        font.pixelSize: 19
                        Accessible.name: qsTr("Redo")
                        ToolTip.text: Accessible.name
                        ToolTip.visible: hovered
                        enabled: studio.canRedo && !studio.busy
                        onClicked: studio.redo()
                    }
                    Button {
                        id: applyButton
                        objectName: "applyTheme"
                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 40
                        text: studio.busy ? qsTr("Applying…") : qsTr("Apply theme")
                        enabled: studio.ready && !studio.busy && window.invalidCount === 0
                        onClicked: window.applyDraft()
                        background: Rectangle {
                            radius: desktopTheme.radius
                            color: !applyButton.enabled ? window.raised
                                : applyButton.down ? Qt.darker(window.neonGreen, 1.16)
                                : applyButton.hovered ? Qt.lighter(window.neonGreen, 1.10) : window.neonGreen
                            border.color: applyButton.visualFocus ? window.ink : window.neonGreen
                            border.width: 2
                        }
                        contentItem: Label {
                            text: applyButton.text
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: applyButton.enabled ? window.accentInk : window.secondary
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }
    }

    readonly property Flickable settingsFlickable: scroll.flickable
}
