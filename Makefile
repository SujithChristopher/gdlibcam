CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
OPENCV_FLAGS = $(shell pkg-config --cflags --libs opencv4)
LIBCAMERA_FLAGS = $(shell pkg-config --cflags --libs libcamera)

# Final production version
apriltag_detector: apriltag_detector.cpp
	$(CXX) $(CXXFLAGS) -o apriltag_detector apriltag_detector.cpp $(OPENCV_FLAGS) $(LIBCAMERA_FLAGS)

# Debug version (keep for troubleshooting)
test_debug: test_libcamera_debug.cpp
	$(CXX) $(CXXFLAGS) -o test_debug test_libcamera_debug.cpp $(OPENCV_FLAGS) $(LIBCAMERA_FLAGS)

# GDExtension build
gdext: 
	scons platform=linux target=template_debug

clean:
	rm -f apriltag_detector test_debug debug_frame_*.jpg detected_frame_*.jpg
	rm -f project/bin/*.so

.PHONY: clean gdext