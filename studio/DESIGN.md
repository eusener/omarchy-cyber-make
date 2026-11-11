# Cyber Make Studio

## Product

A desktop settings editor for the Cyber Make theme. Preview and edit the draft in the same window, then apply it to a personal copy of the theme.

## Visual direction

Use the theme wallpaper as the preview canvas and a real swatch strip for the semantic palette. Extend the Cyber Make palette into the editor: deep plum surfaces, neon pink branding, electric blue fields and section labels, and mint green commit actions. The category rail uses a distinct theme highlight for each section, with tinted selection fills and bright borders. The category rail, scrolling form, and desktop preview share straight edges and fine borders. The preview is the only area that uses the wallpaper or imitates desktop chrome.

## Tokens

| Token | Value | Runtime owner |
| --- | --- | --- |
| Canvas | `darker_background` (#130114) | colors.toml → Backend → Main.qml |
| Panel | `dark_background` (#1c071e) | colors.toml → Backend → Main.qml |
| Raised surface | `background` (#29122a) | colors.toml → Backend → Main.qml |
| Border | `muted` (#523f69) | colors.toml → Backend → Main.qml |
| Text | `bright_foreground` (#eee8f8) | colors.toml → Backend → Main.qml |
| Secondary text | `foreground` (#dad4e4) | colors.toml → Backend → Main.qml |
| Error and neon pink | `bright_red` (#f9abf3) | colors.toml → Backend → Main.qml |
| UI accent | `accent` (#a4a7ff) | colors.toml → Backend → Main.qml |
| Electric blue | `bright_blue` (#69dcff) | colors.toml → Backend → Main.qml |
| Mint green | `bright_cyan` (#6fe5b6) | colors.toml → Backend → Main.qml |
| Neon yellow | `bright_green` (#decd62) | colors.toml → Backend → Main.qml |
| UI rounding | Current Hyprland rounding, capped for controls | DesktopTheme |
| Preview colors and appearance | Theme files plus draft overrides | Backend and ThemePreview.qml |

The theme palette is the canonical color source. Main.qml maps semantic editor roles from Backend values, including draft overrides, and supplies them to SettingRow and ThemePreview. Undo, redo, and reset update these roles alongside the desktop preview. QML fallbacks match the original Cyber Make palette for an unavailable source. Do not take the editor accent from another active desktop theme.

Use **JetBrains Mono** throughout: 24px semibold branding, 28px semibold page titles, 18px preview titles, 13px section labels and fields, and 14px body copy. Controls inherit the window font, including dialogs, menus, switches, and value editors. The Arch package requires `ttf-jetbrains-mono`. Keep the category rail wide enough for monospaced Portuguese labels (226px on desktop, 194px on narrow windows).

Neon accents use the theme's bright colors without shifting their hues. Apply them to headings, category icons, selected borders, focused fields, edited values, and the Apply button. Keep paragraphs pale and backgrounds dark for readability. Pink denotes palette, blue windows, lavender animations, mint shell, and yellow wallpaper/icons. Focus has an outline; selection has both a border and rail marker; errors have text in addition to color. Qt Material supplies shared hover, pressed, disabled, popup, and scrollbar states. On narrower windows, the header switches between the form and the preview. Motion runs only when the user requests an animation preview.

## Interaction

Qt Quick Controls Material owns buttons, switches, combo boxes, dialogs, and scrollbars. SettingRow owns labels, value validation feedback, and per-setting resets. Main owns navigation, focus, and the application status. Backend owns draft history and application state. Persistent changes occur only on Apply. Draft undo, redo, and restore use the same history. An application failure leaves the draft available for correction or retry. Use the Omarchy locale and honor RTL layout direction.
