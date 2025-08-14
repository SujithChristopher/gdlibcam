# GDLibCam

**AprilTag detection system for Raspberry Pi 5 using libcamera and Godot GDExtension**

GDLibCam provides real-time AprilTag detection and pose estimation within the Godot game engine, specifically designed for Raspberry Pi 5. It combines libcamera for camera access, OpenCV for image processing, and AprilTag detection to create an easy-to-use game development platform.

![Raspberry Pi 5](https://img.shields.io/badge/Raspberry%20Pi-5-red?style=flat&logo=raspberrypi)
![Godot](https://img.shields.io/badge/Godot-4.4-blue?style=flat&logo=godotengine)
![OpenCV](https://img.shields.io/badge/OpenCV-4.13-green?style=flat)
![Architecture](https://img.shields.io/badge/Architecture-ARM64-orange?style=flat)

## 🚀 One-Command Setup

Get started on any fresh Raspberry Pi 5 with a single command:

```bash
curl -sSL https://raw.githubusercontent.com/SujithChristopher/gdlibcam/main/install.sh | bash
```

This will:
- Install all system dependencies (libcamera-dev, OpenCV 4.13, SCons)
- Download and setup Godot 4.4 for ARM64
- Build the GDExtension
- Create a ready-to-use development environment in `~/Documents/games/camcpp`

## 📋 Manual Installation

If you prefer manual setup or already have some dependencies:

```bash
# Clone the repository
git clone https://github.com/SujithChristopher/gdlibcam.git
cd gdlibcam

# Run the installation script
chmod +x install.sh
./install.sh
```

### Quick Install (if you already have OpenCV)

```bash
# For users with existing OpenCV installation
chmod +x quick_install.sh
./quick_install.sh
```

## 🎮 Running the System

After installation:

```bash
cd ~/Documents/games/camcpp
./run.sh
```

This launches Godot with the AprilTag detection system ready to use.

## ✨ Features

- **Real-time AprilTag Detection**: Fast marker detection with pose estimation
- **Video Feedback**: Optional live camera feed display (toggleable for performance)
- **Easy Integration**: Simple Godot API for game development
- **Optimized for Pi 5**: Uses libcamera API and ARM64 optimizations
- **Ready-to-Use**: Complete development environment setup

## 🎯 Usage in Your Games

The system provides a simple Godot interface:

```gdscript
# Initialize the detector
var detector = AprilTagDetector.new()
detector.load_camera_parameters("camera_parameters.json")
detector.initialize_camera()
detector.start_camera()

# Get detections in your game loop
var detections = detector.get_latest_detections()
for detection in detections:
    var marker_id = detection["id"]
    var position = detection["tvec"]  # 3D position
    var rotation = detection["rvec"]  # 3D rotation
    var distance = position.length() * 100  # Distance in cm
```

Enable video feedback for debugging:
```gdscript
detector.set_video_feedback_enabled(true)  # Toggle camera view
```

## 📹 Video Feedback System

- **Toggle Control**: Enable/disable video feed on demand
- **Performance Optimized**: Separate processing for detection vs display
- **Resized Feed**: 400x300 video display for efficiency
- **Real-time**: Live camera preview alongside detection data

## 📐 Camera Calibration

For accurate 3D pose estimation, calibrate your camera:

1. The system includes default parameters that work reasonably well
2. For precise measurements, use camera calibration tools
3. Update `camera_parameters.json` with your calibration data

Example calibration file structure:
```json
{
    "calibration": {
        "camera_matrix": [
            [fx, 0, cx],
            [0, fy, cy],
            [0, 0, 1]
        ],
        "dist_coeffs": [[k1], [k2], [p1], [p2]]
    }
}
```

## 🏗️ Development

### Building from Source

```bash
# After cloning the repository
git submodule update --init --recursive
make gdext  # Build the GDExtension
```

### Project Structure

```
gdlibcam/
├── src/                    # C++ GDExtension source
│   ├── apriltag_detector.h
│   └── apriltag_detector.cpp
├── project/               # Godot project
│   ├── main.gd           # Demo application
│   ├── main.tscn         # Main scene
│   └── bin/              # Built libraries
├── godot-cpp/            # Godot C++ bindings (submodule)
├── install.sh            # Full installation script
├── quick_install.sh      # Quick setup script
└── CLAUDE.md            # Detailed development guide
```

## 🔧 System Requirements

- **Hardware**: Raspberry Pi 5 (ARM64)
- **OS**: Raspberry Pi OS (64-bit) or Ubuntu 22.04+ ARM64
- **Dependencies**: Automatically installed by setup script
  - libcamera-dev
  - OpenCV 4.13+
  - SCons
  - Godot 4.4

## 🚨 Troubleshooting

### Camera Issues
- Ensure camera is enabled: `sudo raspi-config` → Interface Options → Camera
- Check camera connection and ribbon cable
- Verify camera detection: `libcamera-hello --list-cameras`

### Build Issues
- Ensure all dependencies are installed: `./install.sh` handles this
- For manual builds: `make clean && make gdext`

### Performance Issues
- Disable video feedback for maximum detection speed
- Adjust marker size in code: `detector.set_marker_size(0.05)` (5cm markers)

## 📚 Documentation

- [CLAUDE.md](CLAUDE.md) - Comprehensive development guide
- [Godot GDExtension Docs](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/index.html)
- [OpenCV ArUco Documentation](https://docs.opencv.org/4.13.0/d5/dae/tutorial_aruco_detection.html)

## 🤝 Contributing

1. Fork the repository
2. Create your feature branch: `git checkout -b feature/amazing-feature`
3. Commit changes: `git commit -m 'Add amazing feature'`
4. Push to branch: `git push origin feature/amazing-feature`
5. Open a Pull Request

## 📄 License

This project is open source and available under the [MIT License](LICENSE).

## 🙏 Acknowledgments

- Built with [Godot Engine](https://godotengine.org/)
- Uses [OpenCV](https://opencv.org/) for computer vision
- AprilTag detection via OpenCV's ArUco module
- Raspberry Pi camera integration with [libcamera](https://libcamera.org/)

---

**Ready to build AR games on Raspberry Pi 5? Get started with one command! 🎮**