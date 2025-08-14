#!/usr/bin/env python
import os
import subprocess

env = SConscript("godot-cpp/SConstruct")

# For the reference:
# - CCFLAGS are compilation flags shared between C and C++
# - CFLAGS are for C-specific compilation flags
# - CXXFLAGS are for C++-specific compilation flags
# - CPPFLAGS are for pre-processor flags
# - CPPDEFINES are for pre-processor defines
# - LINKFLAGS are for linking flags

# tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

# Get OpenCV flags using pkg-config
def get_opencv_flags():
    try:
        # Get OpenCV compile flags
        cflags_output = subprocess.check_output(['pkg-config', '--cflags', 'opencv4']).decode('utf-8').strip()
        cflags = cflags_output.split()
        
        # Get OpenCV link flags  
        libs_output = subprocess.check_output(['pkg-config', '--libs', 'opencv4']).decode('utf-8').strip()
        libs = libs_output.split()
        
        return cflags, libs
    except subprocess.CalledProcessError:
        print("Error: pkg-config failed to find opencv4. Make sure OpenCV is properly installed.")
        return [], []

opencv_cflags, opencv_libs = get_opencv_flags()

# Add OpenCV compile flags
for flag in opencv_cflags:
    if flag.startswith('-I'):
        env.Append(CPPPATH=[flag[2:]])
    else:
        env.Append(CCFLAGS=[flag])

# Add OpenCV link flags
for flag in opencv_libs:
    if flag.startswith('-l'):
        env.Append(LIBS=[flag[2:]])
    elif flag.startswith('-L'):
        env.Append(LIBPATH=[flag[2:]])
    else:
        env.Append(LINKFLAGS=[flag])

# Get libcamera flags using pkg-config
def get_libcamera_flags():
    try:
        # Get libcamera compile flags
        cflags_output = subprocess.check_output(['pkg-config', '--cflags', 'libcamera']).decode('utf-8').strip()
        cflags = cflags_output.split()
        
        # Get libcamera link flags  
        libs_output = subprocess.check_output(['pkg-config', '--libs', 'libcamera']).decode('utf-8').strip()
        libs = libs_output.split()
        
        return cflags, libs
    except subprocess.CalledProcessError:
        print("Error: pkg-config failed to find libcamera. Make sure libcamera-dev is installed.")
        return [], []

libcamera_cflags, libcamera_libs = get_libcamera_flags()

# Add libcamera compile flags
for flag in libcamera_cflags:
    if flag.startswith('-I'):
        env.Append(CPPPATH=[flag[2:]])
    else:
        env.Append(CCFLAGS=[flag])

# Add libcamera link flags
for flag in libcamera_libs:
    if flag.startswith('-l'):
        env.Append(LIBS=[flag[2:]])
    elif flag.startswith('-L'):
        env.Append(LIBPATH=[flag[2:]])
    else:
        env.Append(LINKFLAGS=[flag])

# Add C++17 standard (required for OpenCV) and enable exceptions
env.Append(CXXFLAGS=['-std=c++17', '-fexceptions'])

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "project/bin/libapriltag.{}.{}.framework/libapriltag.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "project/bin/libapriltag{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)