#!/bin/bash

# GDLibCam Quick Installation Script
# For users who already have OpenCV installed

set -e

# Configuration
INSTALL_DIR="/home/$(whoami)/Documents/games/camcpp"
GODOT_VERSION="4.4-stable"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

main() {
    print_status "Quick installing GDLibCam to $INSTALL_DIR"
    
    # Check dependencies
    if ! pkg-config --exists opencv4; then
        echo "Error: OpenCV4 not found. Please install OpenCV first or use the full install.sh script"
        exit 1
    fi
    
    if ! command -v scons &> /dev/null; then
        print_status "Installing SCons..."
        sudo apt update && sudo apt install -y scons libcamera-dev
    fi
    
    # Create directory and clone
    mkdir -p "$INSTALL_DIR"
    cd "$INSTALL_DIR"
    
    print_status "Cloning project..."
    git clone https://github.com/SujithChristopher/gdlibcam.git project
    cd project
    
    # Setup submodules and build
    print_status "Setting up godot-cpp..."
    git submodule update --init --recursive
    
    print_status "Building GDExtension..."
    make gdext
    
    # Download Godot
    print_status "Downloading Godot..."
    cd ..
    wget -q "https://github.com/godotengine/godot/releases/download/${GODOT_VERSION}/Godot_v${GODOT_VERSION}_linux.arm64.zip"
    unzip -q "Godot_v${GODOT_VERSION}_linux.arm64.zip"
    rm "Godot_v${GODOT_VERSION}_linux.arm64.zip"
    chmod +x "Godot_v${GODOT_VERSION}_linux.arm64"
    ln -sf "Godot_v${GODOT_VERSION}_linux.arm64" godot
    
    # Create run script
    cat > run.sh << 'EOF'
#!/bin/bash
cd "$(dirname "$0")/project"
../godot --path .
EOF
    chmod +x run.sh
    
    print_success "Installation complete!"
    echo "Run with: cd $INSTALL_DIR && ./run.sh"
}

main "$@"