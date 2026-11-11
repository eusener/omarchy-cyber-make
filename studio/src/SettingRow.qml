import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: row
    required property var field
    required property var host
    required property int draftVersion
    property var invalidData
    property bool changedValue: { const revision = draftVersion; return studio.overridden(field.id) }
    property var currentValue: { const revision = draftVersion; return studio.value(field.id) }
    property string colorValue: field.kind === 0 ? String(currentValue || "#000000") : "#000000"
    property string issue: invalidData ? invalidData.error : ""
    signal focused(Item item)
    objectName: "setting_" + field.id
    implicitHeight: form.implicitHeight + 6
    radius: Math.min(6, desktopTheme.radius)
    readonly property bool editing: editor.activeFocus || pick.activeFocus || switcher.activeFocus || swatch.activeFocus
    color: editing ? host.tint(host.neonBlue, 0.07) : host.panel
    border.color: issue ? host.danger : editing ? host.neonBlue
        : changedValue ? host.tint(host.neonGreen, 0.65) : "transparent"
    Material.accent: host.neonBlue

    function format(value) {
        if (value === undefined || value === null)
            return ""
        if (field.kind === 4)
            return Number(value).toLocaleString(Qt.locale(desktopTheme.language), 'f', 4)
                .replace(/0+$/, "").replace(/[.,]$/, "")
        if (field.kind === 3)
            return Number(value).toLocaleString(Qt.locale(desktopTheme.language), 'f', 0)
        return String(value)
    }

    function setValue(value) {
        const error = studio.setValue(field.id, value)
        host.trackError(field.id, error, String(value))
        return !error
    }

    function reset() {
        editor.pending = false
        host.trackError(field.id, "", "")
        studio.reset(field.id)
        editor.syncValue()
    }

    function focusEditor() {
        if (editor.visible)
            editor.forceActiveFocus()
        else if (switcher.visible)
            switcher.forceActiveFocus()
        else
            pick.forceActiveFocus()
    }

    ColorDialog {
        id: colors
        title: row.field.label
        onAccepted: row.setValue(selectedColor.toString())
    }

    ColumnLayout {
        id: form
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        anchors.topMargin: 3
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            spacing: 7
            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 75
                Layout.leftMargin: 3
                text: row.field.label
                wrapMode: Text.Wrap
                font.pixelSize: 13
                color: host.ink
            }

            ToolButton {
                id: swatch
                visible: row.field.kind === 0
                Layout.preferredWidth: 34
                Layout.preferredHeight: 37
                Accessible.name: qsTr("Choose color for %1").arg(row.field.label)
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: {
                    colors.selectedColor = row.colorValue
                    colors.open()
                }
                onActiveFocusChanged: if (activeFocus) row.focused(row)
                contentItem: Rectangle {
                    anchors.fill: parent
                    anchors.margins: 8
                    radius: Math.min(4, desktopTheme.radius)
                    color: row.colorValue
                    border.color: swatch.visualFocus ? host.accent : host.line
                }
            }

            TextField {
                id: editor
                objectName: "editor_" + row.field.id
                property bool pending: false
                Layout.preferredWidth: row.field.kind === 0 ? 120 : (row.field.kind === 3 || row.field.kind === 4 ? 108 : 180)
                Layout.maximumWidth: row.width * 0.43
                Layout.preferredHeight: 44
                visible: row.field.kind !== 2 && row.field.kind !== 5
                font.family: host.font.family
                font.pixelSize: 13
                color: row.issue ? host.danger : row.changedValue ? host.neonGreen : host.neonBlue
                selectionColor: host.accent
                selectedTextColor: host.accentInk
                selectByMouse: true
                Accessible.name: row.field.label
                Accessible.description: row.issue
                onTextEdited: pending = true
                onActiveFocusChanged: if (activeFocus) row.focused(row)
                onEditingFinished: commitSetting()
                Component.onCompleted: syncValue()

                function syncValue() {
                    if (!pending)
                        text = row.invalidData ? row.invalidData.text : row.format(row.currentValue)
                }

                function commitSetting() {
                    if (!pending || !visible)
                        return
                    pending = false
                    let value = text.trim()
                    if (row.field.kind === 3 || row.field.kind === 4) {
                        try {
                            value = Number.fromLocaleString(Qt.locale(desktopTheme.language), value)
                        } catch (_) {
                            value = Number(value)
                        }
                        if (text.trim() === "" || !Number.isFinite(value)) {
                            host.trackError(row.field.id, qsTr("Enter a number between %1 and %2.")
                                .arg(row.format(row.field.min)).arg(row.format(row.field.max)), text)
                            return
                        }
                    }
                    const error = studio.setValue(row.field.id, value)
                    if (error && (row.field.kind === 3 || row.field.kind === 4))
                        host.trackError(row.field.id, qsTr("Enter a number between %1 and %2.")
                            .arg(row.format(row.field.min)).arg(row.format(row.field.max)), text)
                    else if (error && row.field.kind === 0)
                        host.trackError(row.field.id, qsTr("Enter a color in #RRGGBB format."), text)
                    else
                        host.trackError(row.field.id, error, text)
                    syncValue()
                }
            }

            Switch {
                id: switcher
                visible: row.field.kind === 2
                checked: visible && Boolean(row.currentValue)
                Material.accent: host.neonGreen
                Accessible.name: row.field.label
                onToggled: row.setValue(checked)
                onActiveFocusChanged: if (activeFocus) row.focused(row)
            }

            ComboBox {
                id: pick
                objectName: "combo_" + row.field.id
                visible: row.field.kind === 5
                Layout.preferredWidth: 182
                Layout.maximumWidth: row.width * 0.46
                Layout.preferredHeight: 45
                model: visible ? row.field.choices : []
                currentIndex: visible ? model.indexOf(String(row.currentValue ?? "")) : -1
                displayText: choiceText(currentText)
                Accessible.name: row.field.label
                onActivated: index => row.setValue(model[index])
                onActiveFocusChanged: if (activeFocus) row.focused(row)
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: pick.width
                    text: pick.choiceText(modelData)
                    highlighted: pick.highlightedIndex === index
                }
                function choiceText(value) {
                    if (value === "")
                        return qsTr("Default")
                    if (row.field.id === "palette.mode")
                        return value === "dark" ? qsTr("Dark") : qsTr("Light")
                    return value
                }
            }

            ToolButton {
                text: "↶"
                Material.foreground: host.neonPink
                Layout.preferredWidth: 36
                Layout.preferredHeight: 38
                enabled: row.changedValue || row.issue.length > 0
                Accessible.name: qsTr("Reset %1").arg(row.field.label)
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: row.reset()
                onActiveFocusChanged: if (activeFocus) row.focused(row)
            }
        }

        Label {
            visible: row.issue.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: 3
            Layout.bottomMargin: 5
            color: host.danger
            text: row.issue
            wrapMode: Text.Wrap
            font.pixelSize: 12
        }
    }

    onCurrentValueChanged: editor.syncValue()
    onInvalidDataChanged: editor.syncValue()
}
