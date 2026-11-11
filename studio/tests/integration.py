import json
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

STUDIO = pathlib.Path(__file__).resolve().parents[1]
BINARY = pathlib.Path(os.environ.get("CYBER_MAKE_STUDIO_BIN", STUDIO / "build/cyber-make-studio"))
SOURCE = STUDIO.parent


class StudioTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.env = dict(os.environ, HOME=str(self.root), XDG_CONFIG_HOME=str(self.root / ".config"),
                        XDG_STATE_HOME=str(self.root / ".local/state"),
                        XDG_RUNTIME_DIR=str(self.root))
        self.profile = self.root / "profile.json"
        self.output = self.root / "output"

    def run_studio(self, command, values, success=True):
        self.profile.write_text(json.dumps(values))
        result = subprocess.run(
            [str(BINARY), command, "--json", "--source", str(SOURCE), "--profile",
             str(self.profile), "--output", str(self.output)],
            text=True, capture_output=True, env=self.env, timeout=30, check=False)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr + result.stdout)
        return json.loads(result.stdout if success else result.stderr)

    def test_exports_theme_with_typed_overrides(self):
        import tomllib

        self.run_studio("export", {
            "palette.accent": "#12AB34",
            "shell.bar.background-alpha": 0.6,
            "shell.menu.selected-border-width": "2 4",
            "shell.notifications.border-width-top": 4.5,
            "shell.polkit.scrim-alpha": 0.4,
            "windows.decoration.rounding": 17,
            "windows.general.gaps_in": "2 4 6 8",
            "windows.general.float_gaps": "-1",
            "windows.general.gaps_workspaces": 9,
            "windows.decoration.blur.enabled": True,
            "motion.windowsIn.speed": 2.5,
            "identity.intro": False,
        })
        self.assertEqual(tomllib.loads((self.output / "colors.toml").read_text())["accent"], "#12ab34")
        bar = tomllib.loads((self.output / "shell.bar.toml").read_text())["bar"]
        self.assertEqual(bar["background-alpha"], 0.6)
        self.assertEqual(bar["size-horizontal"], 26)
        menu = tomllib.loads((self.output / "shell.menu.toml").read_text())["menu"]
        self.assertEqual(menu["selected-border-width"], "2 4")
        notifications = tomllib.loads((self.output / "shell.notifications.toml").read_text())["notifications"]
        self.assertEqual(notifications["border-width-top"], 4.5)
        polkit = tomllib.loads((self.output / "shell.polkit.toml").read_text())["polkit"]
        self.assertEqual(polkit["scrim-alpha"], 0.4)
        self.assertEqual(polkit["accent"], "#12ab34")
        lua = (self.output / "cyber-make-appearance.lua").read_text()
        for value in ("rounding = 17", "enabled = true", 'leaf = "windowsIn"', "speed = 2.5"):
            self.assertIn(value, lua)
        for value in ("top = 2", "right = 4", "bottom = 6", "left = 8",
                      "float_gaps = -1", "gaps_workspaces = 9"):
            self.assertIn(value, lua)
        self.assertTrue((self.output / "backgrounds/1-WALLPAPER00.jpg").is_file())
        self.assertFalse((self.output / "backgrounds/intros").exists())
        self.assertFalse((self.output / "hyprland.lua").exists())
        self.assertFalse((self.root / ".config").exists())

    def test_invalid_input_does_not_create_output(self):
        for values in (
            {"windows.decoration.rounding": 101},
            {"windows.decoration.blur.enabled": "yes"},
            {"motion.windowsIn.style": 'slide" }); dofile("bad")'},
            {"palette.accent": "magenta"},
            {"palette.hyprland_active_border": "invalid alias"},
            {"shell.bar.background-alpha": 1.1},
            {"identity.wallpaper": "../image.png"},
            {"unknown": 5},
        ):
            with self.subTest(values=values):
                self.assertFalse(self.run_studio("export", values, False)["ok"])
                self.assertFalse(self.output.exists())

    def test_does_not_replace_existing_export(self):
        self.output.mkdir()
        marker = self.output / "personal"
        marker.write_text("keep")
        self.run_studio("export", {}, False)
        self.assertEqual(marker.read_text(), "keep")

    def setup_omarchy(self):
        self.current = self.root / ".local/state/omarchy/current"
        self.current.mkdir(parents=True)
        self.name = self.current / "theme.name"
        self.name.write_text("cyber-make\n")
        self.themes = self.root / ".config/omarchy/themes"
        self.themes.mkdir(parents=True)
        (self.themes / "cyber-make").symlink_to(SOURCE, target_is_directory=True)
        self.variant = self.themes / "cyber-make-personal"
        self.hyprland = self.root / ".config/hypr/hyprland.lua"
        self.hyprland.parent.mkdir()
        self.hyprland.write_text("-- User configuration\n")

        commands = self.root / "bin"
        commands.mkdir()
        hyprctl = commands / "hyprctl"
        hyprctl.write_text("""#!/bin/sh
if [ "$1" = configerrors ] && [ -n "${STUDIO_TEST_REJECT_ROUNDING:-}" ]; then
    config="$HOME/.local/state/omarchy/current/theme/cyber-make-appearance.lua"
    if [ -f "$config" ] && grep -q "rounding = $STUDIO_TEST_REJECT_ROUNDING," "$config"; then
        printf '%s\n' 'simulated live compositor rejection'
    fi
fi
exit 0
""")
        hyprctl.chmod(0o755)
        shell = commands / "omarchy-shell"
        shell.write_text("#!/bin/sh\nexit 0\n")
        shell.chmod(0o755)
        self.env["PATH"] = str(commands) + os.pathsep + self.env["PATH"]
        self.env["OMARCHY_THEME_HEADLESS"] = "1"

    def test_applies_with_omarchy_and_keeps_settings_scoped(self):
        import tomllib

        self.setup_omarchy()
        self.run_studio("apply", {
            "palette.accent": "#65eac9",
            "shell.bar.background-alpha": 0.65,
            "windows.decoration.rounding": 9,
            "windows.decoration.shadow.offset": "2 -3.5",
            "windows.decoration.glow.enabled": True,
            "windows.decoration.motion_blur.enabled": True,
            "motion.animations.workspace_wraparound": True,
        })
        self.assertEqual(self.name.read_text().strip(), "cyber-make-personal")
        self.assertEqual((self.current / "theme/colors.toml").read_text(),
                         (self.variant / "colors.toml").read_text())
        shell = tomllib.loads((self.current / "theme/shell.toml").read_text())
        self.assertEqual(shell["bar"]["background-alpha"], 0.65)
        self.assertEqual(shell["launcher"]["background-alpha"], 0.78)
        self.assertTrue(self.hyprland.read_text().startswith("-- User configuration\n"))
        self.assertEqual(len(list(self.hyprland.parent.glob("*.bak"))), 1)

        if shutil.which("lua"):
            assertion = ('hl = {config = function(c) print(c.decoration.rounding) end}; '
                         'dofile(arg[1])')
            def run_loader():
                return subprocess.run(["lua", "-e", assertion, "-", str(self.hyprland)],
                                      text=True, capture_output=True, env=self.env, input="",
                                      timeout=10, check=True).stdout
            self.assertEqual(run_loader().strip(), "9")
            self.name.write_text("cyber-make\n")
            self.assertEqual(run_loader(), "")
            self.name.write_text("cyber-make-personal\n")

        first = self.variant.resolve()
        self.run_studio("apply", {})
        self.assertFalse(first.exists())
        self.assertFalse((self.current / "theme/cyber-make-appearance.lua").exists())
        self.assertEqual(len(list(self.hyprland.parent.glob("*.bak"))), 1)

    def test_rolls_back_failed_first_application_and_failed_revision(self):
        self.setup_omarchy()
        self.env["STUDIO_TEST_REJECT_ROUNDING"] = "29"
        before = self.hyprland.read_bytes()
        error = self.run_studio("apply", {"windows.decoration.rounding": 29}, False)
        self.assertIn("simulated live compositor rejection", error["error"])
        self.assertEqual(self.name.read_text().strip(), "cyber-make")
        self.assertFalse(self.variant.is_symlink())
        self.assertEqual(self.hyprland.read_bytes(), before)

        self.run_studio("apply", {"windows.decoration.rounding": 12})
        first = self.variant.resolve()
        amended = self.hyprland.read_bytes()
        self.run_studio("apply", {"windows.decoration.rounding": 29}, False)
        self.assertEqual(self.variant.resolve(), first)
        self.assertEqual(self.name.read_text().strip(), "cyber-make-personal")
        self.assertEqual(self.hyprland.read_bytes(), amended)
        self.assertIn("rounding = 12", (self.current / "theme/cyber-make-appearance.lua").read_text())

    def test_reloads_wallpaper_when_personal_image_reuses_current_filename(self):
        self.setup_omarchy()
        # Match the shell's path cache: ordinary set ignores an unchanged path,
        # while setInstant invalidates the decoded image as well.
        shell = self.root / "bin/omarchy-shell"
        shell.write_text("""#!/usr/bin/env python3
import os, pathlib, shutil, sys
home = pathlib.Path(os.environ["HOME"])
args = sys.argv[1:]
if len(args) == 4 and args[:2] == ["-q", "background"]:
    cached = home / "displayed-path"
    if args[2] == "setInstant" or not cached.exists() or cached.read_text() != args[3]:
        shutil.copyfile(args[3], home / "displayed-image")
        cached.write_text(args[3])
""")
        self.run_studio("apply", {})
        rendered = self.root / "displayed-image"
        self.assertEqual(rendered.read_bytes(), (SOURCE / "backgrounds/1-WALLPAPER00.jpg").read_bytes())
        original_path = (self.root / "displayed-path").read_text()

        backgrounds = self.root / ".config/omarchy/backgrounds/cyber-make-personal"
        backgrounds.mkdir(parents=True)
        replacement = backgrounds / "1-WALLPAPER00.jpg"
        shutil.copyfile(SOURCE / "preview.png", replacement)
        self.run_studio("apply", {"identity.wallpaper": replacement.name})
        self.assertEqual((self.root / "displayed-path").read_text(), original_path)
        self.assertEqual(rendered.read_bytes(), replacement.read_bytes())

    def test_leaves_unmanaged_theme_alone(self):
        self.setup_omarchy()
        self.variant.mkdir()
        keep = self.variant / "personal"
        keep.write_text("keep")
        self.run_studio("apply", {}, False)
        self.assertEqual(keep.read_text(), "keep")
        self.assertEqual(self.name.read_text().strip(), "cyber-make")

    def test_preserves_wallpaper_and_exports_personal_assets(self):
        self.setup_omarchy()
        backgrounds = self.root / ".config/omarchy/backgrounds/cyber-make-personal"
        backgrounds.mkdir(parents=True)
        first = backgrounds / "01-fundo.png"
        second = backgrounds / "02-fundo.png"
        for path in (first, second):
            shutil.copyfile(SOURCE / "preview.png", path)
        background = self.current / "background"
        background.symlink_to(second)

        self.run_studio("apply", {"palette.accent": "#829dff"})
        self.assertEqual(background.resolve(), second)
        self.run_studio("apply", {"palette.accent": "#b562fa"})
        self.assertEqual(background.resolve(), second)
        self.env["STUDIO_TEST_REJECT_ROUNDING"] = "29"
        self.run_studio("apply", {"windows.decoration.rounding": 29, "identity.wallpaper": first.name}, False)
        self.assertEqual(background.resolve(), second)

        self.run_studio("export", {"identity.wallpaper": first.name})
        self.assertEqual((self.output / "backgrounds" / first.name).read_bytes(), first.read_bytes())


if __name__ == "__main__":
    unittest.main()
