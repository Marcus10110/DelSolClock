#!/bin/bash

# Font conversion setup and execution script
# This script handles all dependencies and then runs the Python font converter

set -e  # Exit on any error

# Constants
DEPS_DIR="_deps"
ADAFRUIT_REPO="https://github.com/adafruit/Adafruit-GFX-Library.git"
FONTCONVERT_PATH="$DEPS_DIR/Adafruit-GFX-Library/fontconvert"

# Function to check if we're running on a supported Linux distribution
check_environment() {
    # Check if apt is available (works on Ubuntu, Debian, and derivatives)
    if ! command -v apt &> /dev/null; then
        echo "Error: This script requires apt package manager" >&2
        echo "Supported distributions: Ubuntu, Debian, Linux Mint, Pop!_OS, Elementary OS, etc." >&2
        exit 1
    fi
    
    # Check if we're on Linux with /etc/os-release
    if [ ! -f /etc/os-release ]; then
        echo "Error: Unable to detect Linux distribution" >&2
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

# Main execution
echo "Setting up font conversion environment..."

check_environment
install_freetype
setup_python_venv
setup_fontconvert

echo "Dependencies ready. Running Python font converter..."

# Run the Python script with default arguments
"$DEPS_DIR/python_venv/bin/python" convert_fonts.py --input input --output output --reset