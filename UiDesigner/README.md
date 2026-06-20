# UiDesigner

Renders the Del Sol Clock's screens to BMP on the desktop, so screen UI can be
developed and reviewed without the ESP32.

The screen code is **shared** with the firmware: it lives in
`../firmware/lib/display/` and is compiled here against the real Adafruit GFX
library, so the rendered output is pixel-identical to the device.

## Render the screens

```powershell
pwsh -File tools/generate_on_save.ps1
```

Outputs to `out/`: one BMP per screen plus `all_screens.bmp` (a labeled grid of
all of them). Requires Visual Studio 2022 (C++ toolchain) and `ninja.exe` in
`.deps/`.

## Iterate

Open the **repo root** in VS Code and install the *Run on Save*
(`emeraldwalk.RunOnSave`) extension. Saving any screen source under
`firmware/lib/display/` or `runner/` auto-rebuilds and re-renders.

- Screen drawing code: `../firmware/lib/display/src/<group>_screen.cpp`
- Sample data each screen renders with: `../firmware/lib/display/src/demo_screens.cpp`
- BMP assets the screens reference: `data/`

## Manual build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target render_screens
./build/Release/render_screens.exe   # run from this directory (resolves data/ + out/)
```

## Layout

- `runner/` — entry point + the desktop (file-backed) image loader
- `tools/` — build/configure/render scripts (`vsenv.ps1` sets up the VS env)
- `data/` — BMP assets for rendering
- `out/`, `build/`, `.deps/`, `reference/` — generated/local (gitignored)
