# Font Conversion Tool

The Adafruit GFX Library uses its own font format, and provides a tool for converting fonts to this format. This directory automates converting the TTF/OTF fonts in `input/` into Adafruit GFX headers in `output/`.

`go.sh` installs dependencies and runs `convert_fonts.py`, which (beyond the raw
headers) also generates a `Fonts::` namespace with an `enum class Font` and a
`GetFont(Font, int size)` lookup — see `instructions.md` for that design.

## Prerequisites

- Ubuntu or WSL Ubuntu
- Internet connection (for cloning dependencies)
- sudo access (for installing packages)
- Python 3 with venv module (`sudo apt install python3 python3-venv`)

## Usage

```bash
./go.sh
```

This converts every font in `input/` and writes the headers to `output/`
(both paths are fixed). To add a font, drop a `.ttf` or `.otf` into `input/`
and re-run `./go.sh`.

> Note: `input/` (source fonts) is committed; `output/` and `_deps/` are
> generated and gitignored. The firmware keeps its own copy of the fonts it
> actually uses in `firmware/include/fonts/`.

## What it does

1. **Environment Setup**: Verifies you're running on Ubuntu/WSL
2. **Dependency Management**: 
   - Installs `libfreetype6-dev` and `pkg-config` if needed
   - Creates a Python virtual environment in `_deps/python_venv/` (if not already present)
   - Installs `otf2ttf` package in the virtual environment (for OTF support)
   - Clones Adafruit GFX Library to `_deps/` folder (if not already present)
   - Builds the `fontconvert` tool (if not already built)
3. **Font Processing**: 
   - Converts any `.otf` files to temporary `.ttf` files using `otf2ttf`
   - Processes all `.ttf` files (original and converted) in the input directory
   - Generates headers for font sizes: 7, 8, 9, 10, 12, 14, 16 points
   - Output files named as: `<FontName><Size>pt7b.h`
   - Cleans up temporary converted files
4. **Master Files**: Creates `all_fonts.h` (font includes, `enum class Font`,
   and the `GetFont` declaration) and `all_fonts.cpp` (the `GetFont`
   implementation), both in the `Fonts::` namespace

## Output Structure

For a font file `MyFont.ttf` or `MyFont.otf`, the script generates:
```
output/
├── MyFont7pt7b.h
├── MyFont8pt7b.h
├── MyFont9pt7b.h
├── MyFont10pt7b.h
├── MyFont12pt7b.h
├── MyFont14pt7b.h
├── MyFont16pt7b.h
├── all_fonts.h
└── all_fonts.cpp
```

## Performance

The script caches dependencies in the `_deps/` folder to make subsequent runs faster. This includes:
- Built `fontconvert` tool
- Python virtual environment with `otf2ttf` installed
- Cloned Adafruit GFX Library

All cached files are preserved between runs for optimal performance.

## Troubleshooting

- **Permission errors**: Make sure you have sudo access for installing packages
- **Font conversion failures**: Check that your TTF/OTF files are valid and not corrupted
- **Build failures**: Ensure you have a working C compiler (usually installed with `build-essential`)
- **OTF conversion issues**: If OTF conversion fails, the script will warn but continue processing other fonts
- **Python errors**: Ensure Python 3 and python3-venv are installed (`sudo apt install python3 python3-venv`)
- **Virtual environment issues**: Delete `_deps/python_venv/` folder to force recreation on next run

## Integration

Include the generated header (and compile `all_fonts.cpp`) in your Arduino/C++
project:

```cpp
#include "all_fonts.h"

// Look up a font by enum + size:
display.setFont(Fonts::GetFont(Fonts::Font::JetBrainsMono, 12));

// Or reference a specific generated font directly:
display.setFont(&MyFont12pt7b);
```
