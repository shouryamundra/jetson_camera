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
# bayer.h's constexpr host/device functions are called with device lambdas.
# -lineinfo keeps source lines for compute-sanitizer and Nsight at no runtime cost.
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -lineinfo --expt-relaxed-constexpr -Xptxas=-warn-spills")
option(JC_WERROR "Treat C++ warnings as errors" ON)
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow>")
if(JC_WERROR)
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Werror>")
endif()

find_package(CUDAToolkit REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
# apt: libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)

# Hikrobot MVS SDK 4.9+ (installed runtime): needed for MV_CC_Initialize and
# for MV_CC_SetGrabStrategy on Linux USB cameras.
set(MVS_ROOT /opt/MVS CACHE PATH "MVS SDK install directory")
find_library(MVS_LIBRARY MvCameraControl PATHS ${MVS_ROOT}/lib/aarch64 ${MVS_ROOT}/lib/64 NO_DEFAULT_PATH REQUIRED)
add_library(MvCameraControl SHARED IMPORTED)
# SYSTEM: vendor headers are not held to our warning flags.
set_target_properties(MvCameraControl PROPERTIES
  IMPORTED_LOCATION ${MVS_LIBRARY}
  INTERFACE_INCLUDE_DIRECTORIES ${MVS_ROOT}/include
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${MVS_ROOT}/include)
get_filename_component(MVS_LIBDIR ${MVS_LIBRARY} DIRECTORY)
set(CMAKE_BUILD_RPATH ${MVS_LIBDIR})
set(CMAKE_INSTALL_RPATH ${MVS_LIBDIR})

add_library(jetson_camera
  src/camera.cc src/config.cc src/frame.cc src/recorder.cc src/preprocess.cu)
target_include_directories(jetson_camera PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(jetson_camera PUBLIC CUDA::cudart MvCameraControl PkgConfig::GST Threads::Threads)

add_executable(capture_bench demo/capture_bench.cu)
target_link_libraries(capture_bench jetson_camera)
add_executable(usb_reset tools/usb_reset.cc)

enable_testing()
# Host-only tests.
foreach(t bayer_test clock_sync_test)
  add_executable(${t} tests/${t}.cc)
  target_include_directories(${t} PRIVATE ${CMAKE_SOURCE_DIR})
  add_test(NAME ${t} COMMAND ${t})
endforeach()
add_executable(config_test tests/config_test.cc)
target_link_libraries(config_test jetson_camera)
add_test(NAME config_test COMMAND config_test)
# GPU tests (no camera).
add_executable(frame_test tests/frame_test.cc)
target_link_libraries(frame_test jetson_camera)
add_test(NAME frame_test COMMAND frame_test)
add_executable(preprocess_test tests/preprocess_test.cu)
target_link_libraries(preprocess_test jetson_camera)
add_test(NAME preprocess_test COMMAND preprocess_test)

# compute-sanitizer: out-of-bounds/misaligned access, races in shared memory,
# uninitialised reads. Opt-in, because Jetson only allows GPU debugging as
# root: cmake -B build -DJC_SANITIZER_TESTS=ON && sudo ctest --test-dir build -L sanitizer
option(JC_SANITIZER_TESTS "Add compute-sanitizer tests (need root on Jetson)" OFF)
find_program(COMPUTE_SANITIZER compute-sanitizer HINTS /usr/local/cuda/bin)
if(JC_SANITIZER_TESTS AND COMPUTE_SANITIZER)
  foreach(tool memcheck racecheck initcheck synccheck)
    add_test(NAME preprocess_${tool} COMMAND ${COMPUTE_SANITIZER} --tool ${tool} --error-exitcode 1 $<TARGET_FILE:preprocess_test>)
    set_tests_properties(preprocess_${tool} PROPERTIES LABELS sanitizer)
  endforeach()
  add_test(NAME frame_memcheck COMMAND ${COMPUTE_SANITIZER} --tool memcheck --error-exitcode 1 $<TARGET_FILE:frame_test>)
  set_tests_properties(frame_memcheck PROPERTIES LABELS sanitizer)
endif()

# Style: cpplint (Google C++ style) and clang-format, e.g. `cmake --build build -t lint`.
add_custom_target(lint
  COMMAND cpplint --quiet --recursive src tests demo tools
  COMMAND clang-format --dry-run --Werror src/*.h src/*.cc src/*.cu tests/*.cc tests/*.cu demo/*.cu tools/*.cc
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "cpplint + clang-format check")
