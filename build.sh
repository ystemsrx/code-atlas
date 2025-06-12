#!/bin/bash
sudo apt update && sudo apt install -y ninja-build
pip3 install --upgrade "conan>=1.60,<2"

mkdir -p build && cd build
conan install .. --build=missing

cmake .. -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release

cmake --build .
