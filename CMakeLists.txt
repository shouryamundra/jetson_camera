cmake_minimum_required(VERSION 3.22)
project(jetson_camera LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(NOT CMAKE_CUDA_ARCHITECTURES)
  set(CMAKE_CUDA_ARCHITECTURES 87)  # Jetson Orin
endif()
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -lineinfo")
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-Wconversion>")

find_package(CUDAToolkit REQUIRED)
find_package(PkgConfig REQUIRED)
# apt: libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)

# Hikrobot MVS SDK. libMvCameraControl loads its USB transport from the MVS runtime install.
set(MVS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mvs)
add_library(MvCameraControl SHARED IMPORTED)
set_target_properties(MvCameraControl PROPERTIES
  IMPORTED_LOCATION ${MVS_DIR}/lib/arm64/libMvCameraControl.so
  INTERFACE_INCLUDE_DIRECTORIES ${MVS_DIR}/include)
set(CMAKE_BUILD_RPATH ${MVS_DIR}/lib/arm64 /opt/MVS/lib/aarch64)

add_library(camera src/camera.cpp src/recorder.cpp src/demosaic.cu)
target_include_directories(camera PUBLIC src)
target_link_libraries(camera PUBLIC CUDA::cudart MvCameraControl PkgConfig::GST)

add_executable(capture_demo demo/capture_demo.cpp)
target_link_libraries(capture_demo camera)

# Host-only test of the demosaic math; needs neither a GPU nor a camera.
enable_testing()
add_executable(bayer_test tests/bayer_test.cpp)
target_include_directories(bayer_test PRIVATE src)
add_test(NAME bayer_test COMMAND bayer_test)
