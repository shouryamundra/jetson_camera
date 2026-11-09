// Loads the pipeline's tunable settings from an INI file.
//
// Every key in the file must be consumed, so a typo is an error instead of a
// silently ignored setting.

#ifndef JETSON_CAMERA_SRC_CONFIG_H_
#define JETSON_CAMERA_SRC_CONFIG_H_

#include <string>
#include <vector>

#include "src/camera.h"
#include "src/recorder.h"

namespace jetson_camera {

struct PipelineConfig {
  CameraOptions camera;
  RecorderOptions recorder;
};

// Reads `path` (INI: [section], key = value, '#' or ';' comments), then
// applies `overrides` ("section.key=value", e.g. from the command line).
// Relative paths in the file resolve against the file's directory. Throws
// std::runtime_error on unknown keys, bad values or out-of-range values.
PipelineConfig LoadPipelineConfig(
    const std::string& path, const std::vector<std::string>& overrides = {});

}  // namespace jetson_camera

#endif  // JETSON_CAMERA_SRC_CONFIG_H_
