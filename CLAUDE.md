# AprilTag Detection System for Raspberry Pi 5 + OV9281

## Overview
Complete AprilTag 36h11 detection system using direct libcamera API with OpenCV for Raspberry Pi 5 and OV9281 160° FOV monochrome camera.

## Hardware Configuration
- **Camera**: OV9281 160° FOV monochrome camera
- **Platform**: Raspberry Pi 5
- **Resolution**: 1200x800 (matches camera calibration parameters)
- **Format**: R16 → CV_8UC1 conversion
- **Orientation**: Normal (no horizontal flip needed)
- **Exposure**: 5000 microseconds (5ms)

## Working Implementation

### Final Production Program
- **File**: `apriltag_detector_final.cpp`
- **Build**: `make apriltag_detector`
- **Run**: `./apriltag_detector [marker_size_in_meters]`

### Key Technical Details

#### Camera Configuration
```cpp
streamConfig.size.width = 1200;   // Match camera calibration parameters
streamConfig.size.height = 800;
streamConfig.pixelFormat = formats::R8; // 8-bit monochrome

// Apply exposure control to each request
request->controls().set(controls::ExposureTime, 5000); // 5ms exposure
```

#### Direct libcamera API Flow
1. **CameraManager** → Initialize libcamera
2. **Camera Configuration** → Set optimal resolution/format
3. **FrameBufferAllocator** → Allocate memory buffers
4. **Request/Response Loop** → Capture frames
5. **Memory Mapping** → Direct buffer access (zero-copy)
6. **OpenCV Integration** → Mat creation from buffer data

#### AprilTag Detection
- **Dictionary**: `cv::aruco::DICT_APRILTAG_36h11`
- **No preprocessing** needed (monochrome camera)
- **No horizontal flip** required
- **Pose estimation** with camera calibration parameters

### Performance Characteristics
- **Zero GStreamer overhead** (direct libcamera API)
- **Zero intermediate copying** (memory mapping)
- **Stable detection** at 1200x800 resolution
- **Multiple marker support** (tested with 3 simultaneous markers)
- **Fixed exposure** for consistent lighting conditions

### Camera Calibration
- **File**: `camera_parameters.toml`
- **Format**: TOML with camera_matrix and dist_coeffs arrays
- **Parser**: Built-in regex-based TOML reader

### Build Commands
```bash
# Production version
make apriltag_detector

# Debug version (saves images)
make test_debug

# Clean
make clean
```

### Successful Test Results
- **Detected markers**: IDs 12, 14, 20
- **Distance estimation**: ~58-61cm accuracy
- **Stable pose vectors**: rvec and tvec values
- **Frame rate**: Smooth real-time operation

## GDExtension Structure
Complete GDExtension files ready for Godot integration:
- `src/apriltag_detector.h` - Main detector class
- `src/apriltag_detector.cpp` - Implementation
- `src/register_types.h/cpp` - GDExtension registration
- `SConstruct` - Build configuration
- `project/apriltag.gdextension` - Godot extension definition

## Notes
- **libcamera-dev package required**
- **OpenCV 4.13.0 with aruco module**
- **Direct API is more efficient than GStreamer pipeline**
- **Normal orientation works (no flip needed for this camera setup)**
- **System handles cleanup properly (no segfaults)**

## Status: Production Ready ✅
The AprilTag detection system is fully functional and ready for integration into Godot projects.

## Memories
- Added memory to track project development and key insights
- add to memory