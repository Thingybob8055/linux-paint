# linpaint: Windows 7 Paint for Linux

A native GTK3/cairo re-implementation of `mspaint.exe` 6.1.7600 (Windows 7 RTM, x64),
built by reverse-engineering the binary in `../mspaint.exe`.

```
src/        C sources (≈6.5k lines)
re/         scripts used for the reverse engineering (no Microsoft code or assets)
flatpak/    Flatpak manifest, AppStream metainfo and app icon
default.nix Nix package (wrapGAppsHook3)    shell.nix  dev shell
```

## Build and run

The only build dependencies are a C compiler, `make`, `pkg-config` and the GTK 3 development
files. Install them with your distro's command below, then build:

```sh
make
./linpaint [file]
```

| Distro | Build dependencies |
|---|---|
| Debian, Ubuntu, Mint, Pop!_OS | `sudo apt install build-essential pkg-config libgtk-3-dev` |
| Fedora, RHEL, CentOS Stream | `sudo dnf install gcc make pkgconf-pkg-config gtk3-devel` |
| Arch, Manjaro, EndeavourOS | `sudo pacman -S --needed base-devel gtk3` |
| openSUSE Tumbleweed / Leap | `sudo zypper install gcc make pkg-config gtk3-devel` |
| Void | `sudo xbps-install -S base-devel pkg-config gtk+3-devel` |
| Alpine | `sudo apk add build-base pkgconf gtk+3.0-dev` |
| Gentoo | `sudo emerge --ask x11-libs/gtk+:3` (the toolchain is already present) |
| NixOS | see below |

These runtime packages are optional:

- `xdg-utils`: *Send in e-mail*.
- SANE (`sane-utils` on Debian/Ubuntu, `sane-backends` on Fedora/openSUSE, `sane` on Arch):
  *From scanner or camera*.
- `librsvg` (usually already installed with GTK): opening SVG files.

### Installing

```sh
make install                                # binary + .desktop file into ~/.local
sudo make install PREFIX=/usr/local         # or system-wide
```

To add your own compiler flags, set them in the environment, as in `CFLAGS="-O3" make`. Passing
`make CFLAGS=...` on the command line replaces the GTK include flags and the build fails.

### NixOS

```sh
nix-build              # -> ./result/bin/linpaint (wrapped, recommended)
# or, for development
nix-shell --run make   # -> ./linpaint
```

### Command line

`linpaint --convert IN OUT [png|jpeg|bmp24|bmp8|bmp4|bmp1|gif|tiff]` re-saves an image without
opening a window.

## Flatpak

`flatpak/org.linpaint.Paint.yml` builds against the GNOME 51 runtime. You need `flatpak` and
`flatpak-builder` (from your distro, or `flatpak install flathub org.flatpak.Builder`, which then
runs as `flatpak run org.flatpak.Builder` in place of `flatpak-builder`).

Build and install it for the current user:

```sh
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak-builder --user --install --install-deps-from=flathub --force-clean \
    flatpak/build-dir flatpak/org.linpaint.Paint.yml
flatpak run org.linpaint.Paint
```

### Distributing

**A single-file bundle.** This is the easiest option. Export to a local repo, then pack a
`.flatpak` file you can attach to a GitHub release:

```sh
flatpak-builder --repo=flatpak/repo --force-clean flatpak/build-dir flatpak/org.linpaint.Paint.yml
flatpak build-bundle flatpak/repo linpaint.flatpak org.linpaint.Paint \
    --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo
```

Users install it with `flatpak install --user linpaint.flatpak`. `--runtime-repo` lets Flatpak
fetch the GNOME runtime from Flathub automatically. Bundles don't auto-update. Each release means
building and sharing a new file.

**Your own repository.** Users get updates through `flatpak update`. Build with
`--gpg-sign=KEYID --repo=flatpak/repo`, then host the `flatpak/repo` directory on any static host
(GitHub Pages, S3, a web server). Publish a `.flatpakrepo` file that points at it with the public
GPG key. Users add it once with `flatpak remote-add`.

**Flathub.** This reaches the most users and gives you updates and the store listing. Before
submitting:

1. Change the app ID to one you can prove you own. For this repo that is
   `io.github.Thingybob8055.linux_paint` (Flathub turns `-` into `_`). Rename it in the manifest,
   the metainfo, the three file names under `flatpak/`, and `gtk_application_new()` in
   `src/main.c`.
2. Point the manifest's source at a tagged release instead of the local directory:
   `type: git`, `url: https://github.com/Thingybob8055/linux-paint.git`, `tag: v6.1`, `commit: <sha>`.
3. Add screenshots, and a `<project_license>` once the project has a license, to the metainfo.
   Check the result with `flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest
   <manifest>`.
4. Fork `flathub/flathub`, add the manifest on a branch based on `new-pr`, and open a pull request
   (see <https://docs.flathub.org/docs/for-app-authors/submission>).

### Sandbox limitations

- *Set as desktop background* calls `gsettings`. That only changes the sandbox's private settings,
  so it reports success but the wallpaper stays the same. The fix is the wallpaper portal
  (`org.freedesktop.portal.Wallpaper`).
- *From scanner or camera* is unavailable, because `scanimage` isn't in the runtime.
- Theme → *Auto* can't read GNOME's `color-scheme` from inside the sandbox. It still follows a dark
  GTK theme name, and *Light* and *Dark* work. The fix is to read `org.freedesktop.appearance
  color-scheme` from the Settings portal.
- Open/Save use the GTK file dialog rather than the portal file chooser, so the manifest grants
  `--filesystem=home`. Switching to `GtkFileChooserNative` would allow a tighter sandbox.

## How the original was analysed

1. **PE resources.** The 802 bitmaps, 36 cursors, 152 icons, the compiled ribbon (`UIFILE`) and 16
   PNG media textures were listed and extracted (`re/extract.py`). There are no string, menu or
   dialog tables because those live in `mspaint.exe.mui`.
2. **Symbols.** The CodeView record gives `mspaint.pdb` / `811B1B09BA4247838A859E3C334EA8832`. The
   public PDB from Microsoft's symbol server supplied about 2,100 class and method names
   (`CPBView::OnZoom800`, `CShapeStore::CreateHeart`, `CEraserTool::OnDrag`, …), which map out
   every feature.
3. **Decompilation.** Ghidra 12 headless, with the PDB applied, decompiled all 4,307 application
   functions (`re/DumpAll.java`, `re/DumpSyms.java`).
4. **Data recovery.** Constant tables were read straight from the image (`re/rd.py`). The shape
   geometry was recovered by **emulating** `CShapeStore::Create*` with Unicorn, which stubs
   `operator new`, `AddPrimitive` and `map::operator[]` (`re/emu.py`).

### Recovered facts used verbatim

| What | Source in binary | Value |
|---|---|---|
| Default palette (20) | `CColorGallerySite::GetDynamicItemInfo` immediates | `000000 7F7F7F 880015 ED1C24 FF7F27 FFF200 22B14C 00A2E8 3F48CC A349A4 / FFFFFF C3C3C3 B97A57 FFAEC9 FFC90E EFE4B0 B5E61D 99D9EA 7092BE C8BFE7` + 10 custom slots |
| Zoom presets | `CImgWnd::presetZooms` | 12.5, 25, 50, 100, 200, 300, 400, 500, 600, 700, 800 % |
| Pencil / Brush / Calligraphy | `CPencilTool`, `CBrushTool`, `CCaligraphicBrushTool` ctors | 1-2-3-4 / 1-3-5-8 / 3-5-8-10 px |
| Oil, Crayon, Marker, Watercolor / Natural pencil | `CPBApp::AddTools` | 8-16-30-40 / 4-6-8-10 px |
| Airbrush / Eraser / Shapes | `CAirBrushTool`, `CEraserTool`, `CShapeTool` ctors | 4-8-16-24 / 4-6-8-10 / 1-3-5-8 px |
| Max custom stroke width | `CImgTool::SetStrokeWidth` (`< 0x33`) | 50 px (Ctrl +/−) |
| Airbrush algorithm | `CAirBrushTool::OnDrag` | 10 random pixels per tick inside a circle of diameter `(size+1)&~1` (min 4) |
| Eraser | `CEraserTool::OnDrag` | left: paint colour 2; right: replace colour 1 → colour 2 only |
| 23 shapes | `CShapeStore::Create*` (emulated) | exact line/Bézier/arc primitive lists in a −50…50 box (`src/shapes.c`) |
| Rounded-rect radius | `CPrimitiveRoundedRect::AddToPGSPath` | `min(16px, w/2, h/2)` |
| Shape transform | `CDrawShapeOperation::CreateWorldTransformMatrix` | scale → rotate → flip → translate (live shapes can be rotated/flipped) |
| Gridline colours | `CImgWnd::DrawGrid` | halftone `#C0C0C0` / `#808080` |
| Settings keys | strings | `ShowRulers ShowStatusBar ShowGrid BMPWidth BMPHeight …` (stored in `~/.config/linpaint/settings.ini`) |
| Saved-size limit | `CPBApp::InitInstance` | sizes above 5000 px are ignored on start |

## Feature parity

**Implemented**

- **Ribbon:** File menu, Home, View, and a contextual Text tab; Quick Access Toolbar (Save, Undo, Redo).
- **Clipboard:** Paste, Paste from, Cut, Copy. Pasting a larger image grows the canvas.
- **Image:** Rectangular and free-form selection, Select all, Invert selection, Delete, Transparent selection, Crop, Resize and Skew (percentage or pixels, aspect lock, ±89° skew), Rotate 90/180, Flip horizontal/vertical, Invert colours (Ctrl+Shift+I). These apply to the selection when there is one.
- **Selections:** move, stretch with 8 handles, arrow-key nudge, Ctrl-drag to copy, Shift-drag to smear. The vacated area fills with colour 2.
- **Tools:** Pencil, Fill (exact-colour flood), Text, Eraser (both modes), Colour picker (returns to the previous tool), Magnifier (left to zoom in, right to zoom out, hover preview).
- **Brushes:** Brush, Calligraphy 1 and 2, Airbrush, Oil, Crayon, Marker, Natural pencil, Watercolor.
- **Shapes:** all 23 (Line, Curve with two bends, Oval, Rectangle, Rounded rectangle, Polygon, Triangle, Right triangle, Diamond, Pentagon, Hexagon, 4 arrows, 4/5/6-point stars, 3 callouts, Heart, Lightning).
  - Shapes stay live after drawing: resize via handles, move, nudge, rotate/flip, and recolour or resize the outline before committing.
  - Shift constrains to square/circle, or to 45° for lines.
  - Outline and Fill each offer 7 media.
  - Shapes are anti-aliased.
- **Colours:** Size gallery; Colour 1 and 2 (right mouse button draws with colour 2 throughout); Edit colours, which fills the 10-slot custom row and is remembered.
- **Text:** font, size, Bold/Italic/Underline/Strikethrough, opaque or transparent background. Selection, caret navigation, and clipboard work inside the box.
- **View:** Zoom in/out/100%, zoom slider, Ctrl+wheel; Rulers; Gridlines; Status bar (cursor position, selection size, image size, file size); Full screen; Thumbnail window.
- **Theme (Linux addition):** View → Theme → Auto / Light / Dark. Auto follows the desktop: GNOME's `color-scheme`, or a dark GTK theme such as Adwaita-dark or Breeze-Dark, detected from the theme's text colour. Light and Dark force Adwaita / Adwaita-dark. The choice is saved as `Theme=` in `settings.ini`.
- **File:** New, Open, Save, Save as (PNG, JPEG, 24-bit/256/16-colour/monochrome BMP, GIF, TIFF), Print, Page setup, Print preview, Set as desktop background (Fill/Tile/Centre, through GNOME `gsettings`), Properties (units in/cm/px, black-and-white conversion, Default), Recent pictures, drag-and-drop open, save-changes prompt.
- **Canvas:** resize by dragging the edge handles; 50-level undo/redo.
- **Shortcuts:** Ctrl+N/O/S/P/Z/Y/X/C/V/A/E/W/R/G, Ctrl+Shift+X, F11, F12, Ctrl+PgUp/PgDn, Ctrl +/−, Delete, Esc.

**Adapted for Linux**

- *From scanner or camera* (WIA) runs SANE's `scanimage`.
- *Send in e-mail* (MAPI) runs `xdg-email --attach`.
- *Help* opens the About dialog.

**Approximated**

- The artistic media textures are generated procedurally rather than copied from Microsoft's PNG resources, so they look similar, not identical.
- Gridline spacing per zoom level and the default canvas size are close to the original but not byte-exact.
- Text formatting applies to the whole text box.

**Not implemented**

- OLE embedding/server (`COleServerDoc`).
- Tablet pressure and multi-touch gestures.
- Crash-recovery autosave (`RecoveryData`).
- Ribbon keytips and QAT customisation.
- Print scaling options ("Adjust to %", "Fit to N pages"); images larger than the page are always shrunk to fit.
