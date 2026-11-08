cmake_minimum_required(VERSION 3.22)
# JetPack puts nvcc in /usr/local/cuda/bin, which non-login shells (ssh,
# services) lack in PATH.
if(NOT DEFINED CMAKE_CUDA_COMPILER AND NOT DEFINED ENV{CUDACXX} AND EXISTS /usr/local/cuda/bin/nvcc)
  set(CMAKE_CUDA_COMPILER /usr/local/cuda/bin/nvcc)
endif()
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
# bayer headers' constexpr host/device functions are called with device lambdas.
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -lineinfo --expt-relaxed-constexpr")
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-Wconversion>")

find_package(CUDAToolkit REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
# apt: libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)

# Hikrobot MVS SDK 4.9+ (installed runtime).
set(MVS_ROOT /opt/MVS CACHE PATH "MVS SDK install directory")
find_library(MVS_LIBRARY MvCameraControl PATHS ${MVS_ROOT}/lib/aarch64 ${MVS_ROOT}/lib/64 NO_DEFAULT_PATH REQUIRED)
add_library(MvCameraControl SHARED IMPORTED)
set_target_properties(MvCameraControl PROPERTIES
  IMPORTED_LOCATION ${MVS_LIBRARY}
  INTERFACE_INCLUDE_DIRECTORIES ${MVS_ROOT}/include
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${MVS_ROOT}/include)
get_filename_component(MVS_LIBDIR ${MVS_LIBRARY} DIRECTORY)
set(CMAKE_BUILD_RPATH ${MVS_LIBDIR})

enable_testing()
# New code is held to -Wshadow -Werror.
function(jc_strict target)
  target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Wshadow;-Werror>")
endfunction()

# Previous capture pipeline (replaced step by step).
add_library(camera src/camera.cpp src/recorder.cpp src/demosaic.cu)
target_include_directories(camera PUBLIC src)
target_link_libraries(camera PUBLIC CUDA::cudart MvCameraControl PkgConfig::GST)
add_executable(capture_demo demo/capture_demo.cpp)
target_link_libraries(capture_demo camera)

add_library(jetson_camera src/frame.cc src/preprocess.cu)
target_include_directories(jetson_camera PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(jetson_camera PUBLIC CUDA::cudart Threads::Threads )
jc_strict(jetson_camera)
add_executable(bayer_test tests/bayer_test.cc)
target_include_directories(bayer_test PRIVATE ${CMAKE_SOURCE_DIR})
jc_strict(bayer_test)
add_test(NAME bayer_test COMMAND bayer_test)
add_executable(clock_sync_test tests/clock_sync_test.cc)
target_include_directories(clock_sync_test PRIVATE ${CMAKE_SOURCE_DIR})
jc_strict(clock_sync_test)
add_test(NAME clock_sync_test COMMAND clock_sync_test)
add_executable(frame_test tests/frame_test.cc)
target_link_libraries(frame_test jetson_camera)
jc_strict(frame_test)
add_test(NAME frame_test COMMAND frame_test)
add_executable(preprocess_test tests/preprocess_test.cu)
target_link_libraries(preprocess_test jetson_camera)
jc_strict(preprocess_test)
add_test(NAME preprocess_test COMMAND preprocess_test)
