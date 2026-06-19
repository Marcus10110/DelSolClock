#!/usr/bin/env python3
"""
Font conversion script for Adafruit GFX Library
Converts TTF and OTF fonts to C++ headers with enhanced structure
"""

import argparse
import os
import subprocess
import sys
import shutil
from pathlib import Path
from typing import Dict, List, Set
import tempfile
import re

# Font sizes to generate
FONT_SIZES = [7, 8, 9, 10, 12, 14, 16]

# Dependencies paths
DEPS_DIR = "_deps"
FONTCONVERT_PATH = f"{DEPS_DIR}/Adafruit-GFX-Library/fontconvert/fontconvert"
VENV_PYTHON = f"{DEPS_DIR}/python_venv/bin/python"


class FontInfo:
    """Information about a font family"""
    def __init__(self, original_name: str, enum_name: str, variable_base: str):
        self.original_name = original_name  # e.g., "JetBrainsMono-Regular"
        self.enum_name = enum_name          # e.g., "JetBrainsMono" (Regular removed)
        self.variable_base = variable_base   # e.g., "JetBrainsMono_Regular"
    
    def get_header_filename(self, size: int) -> str:
        """Get the header filename for this font at the given size"""
        return f"{self.original_name}{size}pt7b.h"
        

def clean_filename_for_c_identifier(name: str) -> str:
    """Convert filename to valid C identifier (replace non-alphanumeric with underscores)"""
    return re.sub(r'[^a-zA-Z0-9]', '_', name)


def extract_font_enum_name(filename: str) -> str:
    """Extract font enum name by removing dashes and processing suffixes"""
    base_name = Path(filename).stem  # Remove extension
    
    # Remove dashes and convert dots to underscores
    enum_name = base_name.replace('-', '').replace('.', '_')
    
    # Remove "Regular" suffix but keep other modifiers like "Bold", "Thin", etc.
    if enum_name.endswith('Regular'):
        enum_name = enum_name[:-7]  # Remove "Regular"
    
    return enum_name


def convert_otf_to_ttf(otf_file: Path, output_dir: Path) -> Path:
    """Convert OTF file to TTF using otf2ttf"""
    ttf_file = output_dir / f"{otf_file.stem}.ttf"
    
    try:
        subprocess.run([
            VENV_PYTHON, "-m", "otf2ttf", 
            str(otf_file), "-o", str(ttf_file)
        ], check=True, capture_output=True)
        return ttf_file
    except subprocess.CalledProcessError as e:
        print(f"Warning: Failed to convert {otf_file}: {e}")
        return None


def generate_font_header(ttf_file: Path, font_info: FontInfo, size: int, output_dir: Path) -> bool:
    """Generate a single font header file"""
    output_file = output_dir / font_info.get_header_filename(size)
    
    try:
        with open(output_file, 'w') as f:
            result = subprocess.run([
                FONTCONVERT_PATH, str(ttf_file), str(size)
            ], check=True, capture_output=True, text=True)
            f.write(result.stdout)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to convert {font_info.original_name} at size {size}: {e}")
        return False


def collect_fonts(input_dir: Path, temp_dir: Path) -> Dict[str, FontInfo]:
    """Collect all font files and convert OTF to TTF if needed"""
    fonts = {}
    
    # Process TTF files
    for ttf_file in input_dir.glob("*.ttf"):
        original_name = ttf_file.stem
        enum_name = extract_font_enum_name(ttf_file.name)
        variable_base = clean_filename_for_c_identifier(original_name)
        fonts[original_name] = FontInfo(original_name, enum_name, variable_base)
    
    # Process OTF files
    otf_count = 0
    for otf_file in input_dir.glob("*.otf"):
        print(f"Converting OTF: {otf_file.stem}")
        ttf_file = convert_otf_to_ttf(otf_file, temp_dir)
        if ttf_file and ttf_file.exists():
            original_name = otf_file.stem  # Use original OTF name
            enum_name = extract_font_enum_name(otf_file.name)
            variable_base = clean_filename_for_c_identifier(original_name)
            fonts[original_name] = FontInfo(original_name, enum_name, variable_base)
            otf_count += 1
    
    if otf_count > 0:
        print(f"Converted {otf_count} OTF files to temporary TTF files")
    
    return fonts


def generate_all_fonts_header(fonts: Dict[str, FontInfo], output_dir: Path):
    """Generate the all_fonts.h header file"""
    header_file = output_dir / "all_fonts.h"
    
    with open(header_file, 'w') as f:
        f.write("#pragma once\n\n")
        f.write("#include <Adafruit_GFX.h>\n")
        f.write("#include <map>\n")
        f.write("#include <vector>\n")
        f.write("#include <string>\n\n")
        
        # Include all font headers
        f.write("// Auto-generated font includes\n")
        for font_info in fonts.values():
            for size in FONT_SIZES:
                f.write(f'#include "{font_info.get_header_filename(size)}"\n')
        f.write("\n")
        
        # Start namespace
        f.write("namespace Fonts {\n\n")
        
        # Generate Font enum class
        f.write("// Font enum class\n")
        f.write("enum class Font {\n")
        for i, font_info in enumerate(fonts.values()):
            comma = "," if i < len(fonts) - 1 else ""
            f.write(f"    {font_info.enum_name}{comma}\n")
        f.write("};\n\n")
        
        # Function declarations
        f.write("// Function to get font by enum and size\n")
        f.write("const GFXfont* GetFont(Font font, int size);\n\n")
        
        # Font collections declarations
        f.write("// Font name mapping\n")
        f.write("extern const std::map<Font, std::string> FontNames;\n\n")
        f.write("// Available font sizes\n")
        f.write("extern const std::vector<int> FontSizes;\n\n")
        
        # Close namespace
        f.write("} // namespace Fonts\n")


def generate_all_fonts_cpp(fonts: Dict[str, FontInfo], output_dir: Path):
    """Generate the all_fonts.cpp implementation file"""
    cpp_file = output_dir / "all_fonts.cpp"
    
    with open(cpp_file, 'w') as f:
        f.write('#include "all_fonts.h"\n\n')
        
        # Start namespace
        f.write("namespace Fonts {\n\n")
        
        # Generate GetFont function
        f.write("const GFXfont* GetFont(Font font, int size)\n")
        f.write("{\n")
        f.write("    switch (font)\n")
        f.write("    {\n")
        
        for font_info in fonts.values():
            f.write(f"    case Font::{font_info.enum_name}:\n")
            f.write("    {\n")
            f.write("        switch (size)\n")
            f.write("        {\n")
            
            for size in FONT_SIZES:
                f.write(f"        case {size}:\n")
                f.write(f"            return &{font_info.variable_base}{size}pt7b;\n")
            
            f.write("        default:\n")
            f.write("            return nullptr;\n")
            f.write("        }\n")
            f.write("    }\n")
        
        f.write("    default:\n")
        f.write("        return nullptr;\n")
        f.write("    }\n")
        f.write("}\n\n")
        
        # Generate FontNames map
        f.write("const std::map<Font, std::string> FontNames = {\n")
        for i, font_info in enumerate(fonts.values()):
            comma = "," if i < len(fonts) - 1 else ""
            f.write(f'    {{Font::{font_info.enum_name}, "{font_info.original_name}"}}{comma}\n')
        f.write("};\n\n")
        
        # Generate FontSizes vector
        f.write("const std::vector<int> FontSizes = {")
        f.write(", ".join(str(size) for size in FONT_SIZES))
        f.write("};\n\n")
        
        # Close namespace
        f.write("} // namespace Fonts\n")


def main():
    parser = argparse.ArgumentParser(description="Convert fonts to Adafruit GFX headers")
    parser.add_argument("--input", required=True, help="Input directory containing font files")
    parser.add_argument("--output", required=True, help="Output directory for generated headers")
    parser.add_argument("--reset", action="store_true", help="Remove existing *.h files first")
    
    args = parser.parse_args()
    
    input_dir = Path(args.input)
    output_dir = Path(args.output)
    
    if not input_dir.exists():
        print(f"Error: Input directory '{input_dir}' does not exist")
        sys.exit(1)
    
    # Create output directory
    output_dir.mkdir(exist_ok=True)
    
    # Reset output directory if requested
    if args.reset:
        print("Resetting output directory...")
        for h_file in output_dir.glob("*.h"):
            h_file.unlink()
        for cpp_file in output_dir.glob("*.cpp"):
            cpp_file.unlink()
    
    # Create temporary directory for OTF conversion
    with tempfile.TemporaryDirectory(dir=Path.cwd()) as temp_dir:
        temp_path = Path(temp_dir)
        
        print("Collecting and processing fonts...")
        fonts = collect_fonts(input_dir, temp_path)
        
        if not fonts:
            print("No TTF or OTF files found in input directory")
            sys.exit(1)
        
        print(f"Found {len(fonts)} font families")
        
        # Generate individual font headers
        font_count = 0
        failed_fonts = []
        
        for font_name, font_info in fonts.items():
            # Find the TTF file (either original or converted)
            ttf_file = input_dir / f"{font_name}.ttf"
            if not ttf_file.exists():
                ttf_file = temp_path / f"{font_name}.ttf"
            
            if ttf_file.exists():
                print(f"Processing font: {font_name}")
                for size in FONT_SIZES:
                    if generate_font_header(ttf_file, font_info, size, output_dir):
                        font_count += 1
                    else:
                        failed_fonts.append(f"{font_name} {size}pt")
            else:
                print(f"Warning: TTF file not found for {font_name}")
                failed_fonts.append(font_name)
        
        if failed_fonts:
            print(f"Warning: Failed to process {len(failed_fonts)} fonts: {', '.join(failed_fonts)}")
        
        # Generate master files
        print("Generating all_fonts.h and all_fonts.cpp...")
        generate_all_fonts_header(fonts, output_dir)
        generate_all_fonts_cpp(fonts, output_dir)
        
        total_headers = len(fonts) * len(FONT_SIZES)
        print(f"Generated {font_count}/{total_headers} font headers from {len(fonts)} font families")
        print("Created all_fonts.h and all_fonts.cpp with enhanced C++ structure")


if __name__ == "__main__":
    main()