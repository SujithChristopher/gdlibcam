#!/bin/bash

# GDLibCam Installation Script for Raspberry Pi 5
# This script sets up a clean development environment for AprilTag detection with Godot

set -e  # Exit on any error

# Configuration
INSTALL_DIR="/home/$(whoami)/Documents/games/camcpp"
GODOT_VERSION="4.4-stable"
OPENCV_VERSION="4.13.0"
TEMP_DIR="/tmp/gdlibcam_install"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running on Raspberry Pi 5
check_platform() {
    print_status "Checking platform compatibility..."
    
    # Check if it's ARM64
    if [ "$(uname -m)" != "aarch64" ]; then
        print_error "This script is designed for ARM64 (aarch64) architecture only"
        exit 1
    fi
    
    # Check if it's Raspberry Pi
    if ! grep -q "Raspberry Pi" /proc/cpuinfo 2>/dev/null; then
        print_warning "Not detected as Raspberry Pi, but continuing anyway"
    fi
    
    print_success "Platform check passed"
}

# Install system dependencies
install_system_deps() {
    print_status "Installing system dependencies..."
    
    sudo apt update
    sudo apt install -y \
        build-essential \
        cmake \
        pkg-config \
        git \
        wget \
        unzip \
        libcamera-dev \
        scons \
        python3-dev \
        python3-pip
    
    print_success "System dependencies installed"
}

# Install OpenCV
install_opencv() {
    print_status "Installing OpenCV ${OPENCV_VERSION}..."
    
    # Check if OpenCV is already installed
    if pkg-config --exists opencv4; then
        INSTALLED_VERSION=$(pkg-config --modversion opencv4)
        print_success "OpenCV ${INSTALLED_VERSION} is already installed"
        return 0
    fi
    
    mkdir -p "$TEMP_DIR"
    cd "$TEMP_DIR"
    
    # Download OpenCV
    print_status "Downloading OpenCV source..."
    wget -q -O opencv.zip "https://github.com/opencv/opencv/archive/${OPENCV_VERSION}.zip"
    wget -q -O opencv_contrib.zip "https://github.com/opencv/opencv_contrib/archive/${OPENCV_VERSION}.zip"
    
    unzip -q opencv.zip
    unzip -q opencv_contrib.zip
    
    cd "opencv-${OPENCV_VERSION}"
    mkdir -p build && cd build
    
    print_status "Configuring OpenCV build (this may take a while)..."
    cmake \
        -D CMAKE_BUILD_TYPE=Release \
        -D CMAKE_INSTALL_PREFIX=/usr/local \
        -D OPENCV_EXTRA_MODULES_PATH="../opencv_contrib-${OPENCV_VERSION}/modules" \
        -D ENABLE_NEON=ON \
        -D ENABLE_VFPV3=ON \
        -D BUILD_TESTS=OFF \
        -D BUILD_PERF_TESTS=OFF \
        -D BUILD_EXAMPLES=OFF \
        -D INSTALL_PYTHON_EXAMPLES=OFF \
        -D OPENCV_ENABLE_NONFREE=ON \
        -D CMAKE_SHARED_LINKER_FLAGS=-latomic \
        ..
    
    print_status "Building OpenCV (this will take 30-60 minutes)..."
    make -j$(nproc)
    
    print_status "Installing OpenCV..."
    sudo make install
    sudo ldconfig
    
    print_success "OpenCV installed successfully"
}

# Download and install Godot
install_godot() {
    print_status "Installing Godot ${GODOT_VERSION}..."
    
    mkdir -p "$INSTALL_DIR"
    cd "$INSTALL_DIR"
    
    # Download Godot for ARM64
    GODOT_URL="https://github.com/godotengine/godot/releases/download/${GODOT_VERSION}/Godot_v${GODOT_VERSION}_linux.arm64.zip"
    
    print_status "Downloading Godot ${GODOT_VERSION} for ARM64..."
    wget -q -O godot.zip "$GODOT_URL"
    unzip -q godot.zip
    rm godot.zip
    
    # Make Godot executable
    chmod +x "Godot_v${GODOT_VERSION}_linux.arm64"
    
    # Create symlink for easier access
    ln -sf "Godot_v${GODOT_VERSION}_linux.arm64" godot
    
    print_success "Godot installed to $INSTALL_DIR"
}

# Clone and setup GDLibCam project
setup_project() {
    print_status "Setting up GDLibCam project..."
    
    cd "$INSTALL_DIR"
    
    # Clone the project
    git clone https://github.com/SujithChristopher/gdlibcam.git project
    cd project
    
    # Initialize submodules (godot-cpp)
    print_status "Initializing godot-cpp submodule..."
    git submodule update --init --recursive
    
    # Build the GDExtension
    print_status "Building GDExtension..."
    make gdext
    
    print_success "Project setup complete"
}

# Create camera calibration files
setup_calibration() {
    print_status "Setting up camera calibration files..."
    
    cd "$INSTALL_DIR/project"
    
    # Create default camera parameters if they don't exist
    if [ ! -f "camera_parameters.json" ]; then
        cat > camera_parameters.json << 'EOF'
{
    "calibration": {
        "camera_matrix": [
            [800.0, 0.0, 600.0],
            [0.0, 800.0, 400.0],
            [0.0, 0.0, 1.0]
        ],
        "dist_coeffs": [
            [0.1],
            [-0.2],
            [0.0],
            [0.0]
        ]
    }
}
EOF
        print_warning "Created default camera_parameters.json - you should calibrate your camera"
    fi
    
    print_success "Calibration files ready"
}

# Clean up temporary files
cleanup() {
    print_status "Cleaning up temporary files..."
    rm -rf "$TEMP_DIR"
    print_success "Cleanup complete"
}

# Create run script
create_run_script() {
    print_status "Creating run script..."
    
    cat > "$INSTALL_DIR/run.sh" << 'EOF'
#!/bin/bash
# GDLibCam Runner Script

cd "$(dirname "$0")/project"

# Check if GDExtension is built
if [ ! -f "bin/libapriltag.linux.template_debug.arm64.so" ]; then
    echo "GDExtension not found. Building..."
    make gdext
fi

# Run Godot with the project
../godot --path .
EOF

    chmod +x "$INSTALL_DIR/run.sh"
    
    print_success "Run script created at $INSTALL_DIR/run.sh"
}

# Main installation function
main() {
    print_status "Starting GDLibCam installation for Raspberry Pi 5"
    print_status "Installation directory: $INSTALL_DIR"
    
    # Create installation directory
    mkdir -p "$INSTALL_DIR"
    
    # Run installation steps
    check_platform
    install_system_deps
    install_opencv
    install_godot
    setup_project
    setup_calibration
    create_run_script
    cleanup
    
    print_success "Installation completed successfully!"
    echo
    print_status "Next steps:"
    echo "  1. cd $INSTALL_DIR"
    echo "  2. ./run.sh  # To start the AprilTag detection system"
    echo
    print_status "For camera calibration, see: $INSTALL_DIR/project/CLAUDE.md"
    print_status "Project repository: https://github.com/SujithChristopher/gdlibcam"
}

# Run main function
main "$@"