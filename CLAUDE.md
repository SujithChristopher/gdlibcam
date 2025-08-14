# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

GDLibCam is an AprilTag detection system for Raspberry Pi 5, implemented as a Godot GDExtension. It combines libcamera (for camera access), OpenCV (for image processing), and AprilTag detection (via OpenCV's ArUco module) to provide real-time marker detection and pose estimation within the Godot game engine.

## Development Commands

### Build Commands
- `make gdext` - Build the GDExtension library using SCons
- `scons platform=linux target=template_debug` - Direct SCons build for Linux debug target
- `make apriltag_detector` - Build standalone AprilTag detector executable
- `make test_debug` - Build debug version for troubleshooting
- `make clean` - Clean all build artifacts

### Testing Commands
- `./apriltag_detector` - Run standalone detector (after building)
- `./test_debug` - Run debug version for camera troubleshooting

## Architecture

### Core Components

1. **AprilTagDetector Class** (`src/apriltag_detector.h/.cpp`)
   - Main GDExtension class inheriting from Godot's Resource
   - Integrates libcamera for camera access and OpenCV for AprilTag detection
   - Handles camera initialization, frame capture, and marker detection
   - Provides pose estimation (rvec/tvec) for detected markers

2. **Build System**
   - **SCons** (`SConstruct`): Primary build system for GDExtension
   - **Makefile**: Alternative build targets and utilities
   - Both automatically detect and link OpenCV4 and libcamera via pkg-config

3. **Godot Integration** (`project/`)
   - GDExtension configuration in `apriltag.gdextension`
   - Main scene and script demonstrate real-time detection
   - Camera parameters loaded from JSON/TOML files

### Key Dependencies
- **libcamera**: Raspberry Pi camera access (requires libcamera-dev)
- **OpenCV4**: Image processing and ArUco detection (requires libopencv-dev)
- **godot-cpp**: GDExtension bindings (included as submodule)

### Camera Calibration
- Camera parameters stored in `camera_parameters.toml` and `project/camera_parameters.json`
- Contains camera matrix and distortion coefficients for pose estimation
- Required for accurate 3D position/orientation calculations

## Important Development Notes

### Platform Specifics
- Designed specifically for Raspberry Pi 5 with ARM64 architecture
- Uses libcamera API (modern replacement for legacy camera interface)
- Binary outputs target `linux.template_debug.arm64` configuration

### Threading and Safety
- Camera callbacks run on separate threads
- Detection results stored in global variables with mutex protection
- Current implementation uses static instance pointer for callback access

### Calibration Requirements
- Camera must be calibrated before accurate pose estimation
- Marker size must be set correctly (default: 5cm/0.05m)
- JSON path in main.gd uses absolute path - adjust for deployment

### Build Dependencies
The build system expects these packages to be installed:
- `libcamera-dev`
- `libopencv-dev` 
- `pkg-config`

If pkg-config fails to find these libraries, the build will fail with appropriate error messages.
- add to memory
- Add to memory