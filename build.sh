#!/bin/bash

# Blood Strike Linux Build Script
# Run this on Fedora Linux

set -e

echo "========================================"
echo "Blood Strike Linux Build Script"
echo "========================================"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Check for dependencies
echo -e "${YELLOW}[*] Checking dependencies...${NC}"

# Install dependencies on Fedora
install_deps() {
    echo -e "${YELLOW}[*] Installing dependencies...${NC}"
    sudo dnf install -y \
        gcc-c++ \
        cmake \
        make \
        vulkan-devel \
        vulkan-headers \
        vulkan-loader \
        vulkan-validation-layers \
        glfw-devel \
        libX11-devel \
        libXi-devel \
        libXrandr-devel \
        libXinerama-devel \
        libXcursor-devel \
        git
}

# Check if running on Fedora
if [ -f /etc/fedora-release ]; then
    echo -e "${GREEN}[+] Detected Fedora Linux${NC}"
    if ! command -v cmake &> /dev/null; then
        install_deps
    fi
else
    echo -e "${YELLOW}[!] Not running on Fedora, assuming dependencies are installed${NC}"
fi

# Download ImGui if not present
if [ ! -d "libs/imgui" ]; then
    echo -e "${YELLOW}[*] Downloading Dear ImGui...${NC}"
    mkdir -p libs
    cd libs
    git clone --depth 1 --branch docking https://github.com/ocornut/imgui.git
    cd ..
    echo -e "${GREEN}[+] ImGui downloaded${NC}"
else
    echo -e "${GREEN}[+] ImGui already present${NC}"
fi

# Create build directory
echo -e "${YELLOW}[*] Creating build directory...${NC}"
mkdir -p build
cd build

# Configure
echo -e "${YELLOW}[*] Configuring CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo -e "${YELLOW}[*] Building...${NC}"
make -j$(nproc)

# Check result
if [ -f "libbloodstrike.so" ]; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}[+] Build successful!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo "Output: build/libbloodstrike.so"
    echo ""
    echo "Usage:"
    echo "  LD_PRELOAD=./build/libbloodstrike.so ./game"
    echo ""
    echo "Or use injector:"
    echo "  ./build/bloodstrike --inject <pid> ./build/libbloodstrike.so"
    echo ""
else
    echo -e "${RED}[-] Build failed!${NC}"
    exit 1
fi