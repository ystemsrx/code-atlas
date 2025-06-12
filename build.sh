#!/bin/bash

# Build script for Linux
# This script sets up the build environment and compiles Code Atlas on Linux

set -e  # Exit on any error

echo "Building Code Atlas for Linux..."

# Check if we're on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo "Error: This script is designed for Linux only."
    echo "For macOS, use build_macos.sh instead."
    exit 1
fi

# Update package list and install dependencies
echo "Installing system dependencies..."
sudo apt update && sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    python3-dev \
    python3-pip \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    pkg-config

# Install Conan for dependency management
echo "Installing Conan package manager..."
pip3 install --upgrade "conan>=1.60,<2"

# Configure Conan profile for C++11 ABI
echo "Configuring Conan profile..."
conan profile update settings.compiler.libcxx=libstdc++11 default

# Create build directory
mkdir -p build && cd build

# Install dependencies with Conan
echo "Installing C++ dependencies with Conan..."
conan install .. --build=missing

# Configure with CMake
echo "Configuring build with CMake..."
cmake .. -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=17

# Build the project
echo "Building Code Atlas..."
cmake --build .

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
