#!/bin/bash

# Bash script to generate Adafruit GFX Library font headers from TTF files
# Usage: ./convert.sh --input <input_dir> --output <output_dir> [--reset]

set -e  # Exit on any error

# Default values and constants
FONT_SIZES=(7 8 9 10 12 14 16)
DEPS_DIR="_deps"
ADAFRUIT_REPO="https://github.com/adafruit/Adafruit-GFX-Library.git"
FONTCONVERT_PATH="$DEPS_DIR/Adafruit-GFX-Library/fontconvert"

# Function to show usage
usage() {
    echo "Usage: $0 --input <input_dir> --output <output_dir> [--reset]"
    echo "  --input   Directory containing TTF font files"
    echo "  --output  Directory to write generated header files"
    echo "  --reset   Remove all *.h files from output directory before processing"
    exit 1
}

# Function to check if we're running on Ubuntu/WSL
check_environment() {
    if ! grep -q "Ubuntu\|ubuntu" /etc/os-release 2>/dev/null; then
        echo "Error: This script only supports Ubuntu or WSL Ubuntu" >&2
        exit 1
    fi
}

# Function to install freetype2 development packages
install_freetype() {
    echo "Checking for freetype2 development packages..."
    if ! pkg-config --exists freetype2; then
        echo "Installing freetype2-dev..."
        sudo apt update
        sudo apt install -y libfreetype6-dev pkg-config
    else
        echo "freetype2-dev is already installed"
    fi
}

# Function to setup Python virtual environment for OTF support
setup_python_venv() {
    echo "Setting up Python virtual environment for OTF support..."
    
    # Check if Python3 is available
    if ! command -v python3 &> /dev/null; then
        echo "Error: Python 3 is required but not installed" >&2
        echo "Please install Python 3 first: sudo apt install python3 python3-venv" >&2
        exit 1
    fi
    
    # Check if venv module is available
    if ! python3 -m venv --help &> /dev/null; then
        echo "Error: python3-venv is required but not installed" >&2
        echo "Please install: sudo apt install python3-venv" >&2
        exit 1
    fi
    
    local venv_dir="$DEPS_DIR/python_venv"
    
    # Create virtual environment if it doesn't exist
    if [ ! -d "$venv_dir" ]; then
        echo "Creating Python virtual environment..."
        python3 -m venv "$venv_dir"
    fi
    
    # Ensure pip is available in the venv
    if [ ! -f "$venv_dir/bin/pip" ]; then
        echo "Installing pip in virtual environment..."
        "$venv_dir/bin/python" -m ensurepip --default-pip
    fi
    
    # Check if otf2ttf is installed in the venv
    if ! "$venv_dir/bin/python" -c "import otf2ttf" 2>/dev/null; then
        echo "Installing otf2ttf in virtual environment..."
        "$venv_dir/bin/pip" install otf2ttf
    else
        echo "otf2ttf is already installed in virtual environment"
    fi
}

# Function to clone and build fontconvert tool
setup_fontconvert() {
    if [ ! -d "$DEPS_DIR" ]; then
        mkdir -p "$DEPS_DIR"
    fi
    
    if [ ! -d "$FONTCONVERT_PATH" ]; then
        echo "Cloning Adafruit GFX Library..."
        cd "$DEPS_DIR"
        git clone "$ADAFRUIT_REPO"
        cd ..
    fi
    
    if [ ! -f "$FONTCONVERT_PATH/fontconvert" ]; then
        echo "Building fontconvert tool..."
        cd "$FONTCONVERT_PATH"
        make
        cd - > /dev/null
    else
        echo "fontconvert tool already built"
    fi
}

# Function to convert OTF files to temporary TTF files
preprocess_otf_fonts() {
    local input_dir="$1"
    local temp_dir="$2"
    local venv_python="$DEPS_DIR/python_venv/bin/python"
    
    mkdir -p "$temp_dir"
    
    local otf_count=0
    for otf_file in "$input_dir"/*.otf; do
        if [ -f "$otf_file" ]; then
            local font_name=$(basename "$otf_file" .otf)
            local temp_ttf="$temp_dir/${font_name}.ttf"
            echo "Converting OTF: $font_name"
            
            if "$venv_python" -m otf2ttf "$otf_file" -o "$temp_ttf"; then
                otf_count=$((otf_count + 1))
            else
                echo "Warning: Failed to convert $otf_file" >&2
            fi
        fi
    done
    
    if [ $otf_count -gt 0 ]; then
        echo "Converted $otf_count OTF files to temporary TTF files"
    fi
}

# Function to process fonts
process_fonts() {
    local input_dir="$1"
    local output_dir="$2"
    local temp_dir="$DEPS_DIR/temp_ttf"
    local all_fonts_file="$output_dir/all_fonts.h"
    
    # Create output directory if it doesn't exist
    mkdir -p "$output_dir"
    
    # Convert OTF files to temporary TTF files
    preprocess_otf_fonts "$input_dir" "$temp_dir"
    
    # Start writing all_fonts.h
    cat > "$all_fonts_file" << EOF
#pragma once

// Auto-generated font includes
EOF
    
    local font_count=0
    local total_fonts_processed=0
    
    # Process TTF files from both original input and temp directories
    for search_dir in "$input_dir" "$temp_dir"; do
        if [ ! -d "$search_dir" ]; then
            continue
        fi
        
        for ttf_file in "$search_dir"/*.ttf; do
            if [ ! -f "$ttf_file" ]; then
                continue
            fi
            
            local font_name=$(basename "$ttf_file" .ttf)
            echo "Processing font: $font_name"
            total_fonts_processed=$((total_fonts_processed + 1))
            
            # Generate header for each font size
            for size in "${FONT_SIZES[@]}"; do
                local output_file="$output_dir/${font_name}${size}pt7b.h"
                echo "  Generating ${font_name}${size}pt7b.h..."
                
                if ! "$FONTCONVERT_PATH/fontconvert" "$ttf_file" "$size" > "$output_file"; then
                    echo "Error: Failed to convert $font_name at size $size" >&2
                    exit 1
                fi
                
                # Add include to all_fonts.h
                echo "#include \"${font_name}${size}pt7b.h\"" >> "$all_fonts_file"
                font_count=$((font_count + 1))
            done
        done
    done
    
    # Check if any fonts were found
    if [ $total_fonts_processed -eq 0 ]; then
        echo "No TTF or OTF files found in $input_dir"
        exit 1
    fi
    
    # Close the header file
    echo "" >> "$all_fonts_file"
    
    # Cleanup temporary files
    if [ -d "$temp_dir" ]; then
        rm -rf "$temp_dir"
    fi
    
    echo "Generated $font_count font headers from $total_fonts_processed fonts and created all_fonts.h"
}

# Parse command line arguments
INPUT_DIR=""
OUTPUT_DIR=""
RESET_FLAG=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --input)
            INPUT_DIR="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --reset)
            RESET_FLAG=true
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            ;;
    esac
done

# Validate required arguments
if [ -z "$INPUT_DIR" ] || [ -z "$OUTPUT_DIR" ]; then
    echo "Error: --input and --output are required" >&2
    usage
fi

if [ ! -d "$INPUT_DIR" ]; then
    echo "Error: Input directory '$INPUT_DIR' does not exist" >&2
    exit 1
fi

# Main execution
echo "Font converter starting..."
echo "Input directory: $INPUT_DIR"
echo "Output directory: $OUTPUT_DIR"

check_environment
install_freetype
setup_python_venv
setup_fontconvert

# Reset output directory if requested
if [ "$RESET_FLAG" = true ]; then
    echo "Resetting output directory..."
    rm -f "$OUTPUT_DIR"/*.h
fi

process_fonts "$INPUT_DIR" "$OUTPUT_DIR"

echo "Font conversion completed successfully!"
