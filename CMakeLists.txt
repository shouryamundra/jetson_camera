cmake_minimum_required(VERSION 3.22)
project(jetson_camera LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)  # std::jthread, std::atomic::wait, concepts
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)  # the .cu files only include C++17-safe headers
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
find_package(Threads REQUIRED)

# Hikrobot MVS SDK. libMvCameraControl loads its USB transport from the MVS runtime install.
set(MVS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mvs)
add_library(MvCameraControl SHARED IMPORTED)
set_target_properties(MvCameraControl PROPERTIES
  IMPORTED_LOCATION ${MVS_DIR}/lib/arm64/libMvCameraControl.so
  INTERFACE_INCLUDE_DIRECTORIES ${MVS_DIR}/include)
set(CMAKE_BUILD_RPATH ${MVS_DIR}/lib/arm64 /opt/MVS/lib/aarch64)

add_library(gpu_camera src/gpu_camera.cpp src/mvs_device.cpp src/demosaic.cu)
target_include_directories(gpu_camera PUBLIC src)
target_link_libraries(gpu_camera PUBLIC CUDA::cudart MvCameraControl Threads::Threads)

add_executable(capture_demo demo/capture_demo.cpp)
target_link_libraries(capture_demo gpu_camera)

# Host-only tests; they need neither a GPU nor a camera.
enable_testing()
foreach(test bayer_test mailbox_test)
  add_executable(${test} tests/${test}.cpp)
  target_include_directories(${test} PRIVATE src)
  target_link_libraries(${test} Threads::Threads)
  add_test(NAME ${test} COMMAND ${test})
endforeach()
