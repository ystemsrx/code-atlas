#!/bin/bash

# Build script for macOS
# This script sets up the build environment and compiles Code Atlas on macOS

set -e  # Exit on any error

echo "Building Code Atlas for macOS..."

# Check if we're on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Error: This script is designed for macOS only."
    echo "For Linux, use build.sh instead."
    exit 1
fi

# Check for required tools
command -v cmake >/dev/null 2>&1 || { echo "Error: cmake is required but not installed. Install with: brew install cmake"; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "Error: python3 is required but not installed. Install with: brew install python"; exit 1; }

# Check if Homebrew is available for dependency management
if command -v brew >/dev/null 2>&1; then
    echo "Homebrew detected. Installing dependencies..."
    
    # Install dependencies via Homebrew
    brew install cmake nlohmann-json cpr python@3.11 || {
        echo "Warning: Some dependencies might already be installed or unavailable via Homebrew."
        echo "Continuing with build..."
    }
else
    echo "Homebrew not found. You may need to install dependencies manually:"
    echo "- CMake >= 3.16"
    echo "- nlohmann-json"
    echo "- cpr (C++ Requests library)"
    echo "- Python 3.x with development headers"
fi

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring build with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_STANDARD=17 \
         -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build the project
echo "Building Code Atlas..."
cmake --build . --config Release

# Check if build was successful
if [ -f "./code-atlas" ]; then
    echo ""
    echo "✅ Build successful!"
    echo "Executable: $(pwd)/code-atlas"
    echo ""
    echo "To run Code Atlas:"
    echo "  cd $(pwd)"
    echo "  ./code-atlas"
    echo ""
    echo "Make sure to configure your config.json file before running."
else
    echo "❌ Build failed. Please check the error messages above."
    exit 1
fi
