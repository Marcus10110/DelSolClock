# Font Conversion Tool

The Adafruit GFX Library uses its own font format, and provides a tool for converting fonts to this format. This directory contains a bash script that automates the entire process of converting TTF and OTF fonts to Adafruit GFX headers.

## Prerequisites

- Ubuntu or WSL Ubuntu
- Internet connection (for cloning dependencies)
- sudo access (for installing packages)
- Python 3 with venv module (`sudo apt install python3 python3-venv`)

## Usage

```bash
./convert.sh --input <input_directory> --output <output_directory> [--reset]
```

### Arguments

- `--input`: Directory containing your TTF and/or OTF font files
- `--output`: Directory where generated header files will be written (created if it doesn't exist)
- `--reset`: (Optional) Remove all existing `*.h` files from output directory before processing

### Examples

```bash
# Convert fonts from input/ to output/
./convert.sh --input ./input --output ./output

# Clean output directory first, then convert
./convert.sh --input ./fonts --output ./headers --reset
```

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
4. **Master Header**: Creates `all_fonts.h` with includes for all generated headers

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
└── all_fonts.h
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

Include the generated headers in your Arduino/C++ project:

```cpp
#include "all_fonts.h"

// Use fonts like:
display.setFont(&MyFont12pt7b);
```
