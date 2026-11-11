// INI loader for PipelineConfig: parse into a map, then pull each known key
// with a typed, range-checked getter.

#include "src/config.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace jetson_camera {
namespace {

std::string Trim(const std::string& s) {
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

class Values {
 public:
  void Set(const std::string& key, const std::string& value,
           const std::string& where) {
    values_[key] = value;
    where_[key] = where;
  }

  template <typename T>
  void Get(const std::string& key, T* out, T min, T max) {
    const auto it = values_.find(key);
    if (it == values_.end()) return;  // Keep the built-in default.
    used_.insert(key);
    T v;
    try {
      std::size_t n = 0;
      if constexpr (std::is_same_v<T, int>) {
        v = std::stoi(it->second, &n);
      } else {
        v = static_cast<T>(std::stod(it->second, &n));
      }
      if (n != it->second.size()) throw std::invalid_argument("trailing text");
    } catch (const std::exception&) {
      throw std::runtime_error(where_[key] + ": " + key + " = '" + it->second +
                               "' is not a number");
    }
    if (v < min || v > max) {
      throw std::runtime_error(where_[key] + ": " + key + " = " + it->second +
                               " out of range [" + std::to_string(min) + ", " +
                               std::to_string(max) + "]");
    }
    *out = v;
  }

  void Get(const std::string& key, bool* out) {
    const auto it = values_.find(key);
    if (it == values_.end()) return;
    used_.insert(key);
    std::string v = it->second;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    if (v == "true" || v == "1" || v == "yes" || v == "on") {
      *out = true;
    } else if (v == "false" || v == "0" || v == "no" || v == "off") {
      *out = false;
    } else {
      throw std::runtime_error(where_[key] + ": " + key + " = '" + it->second +
                               "' is not a boolean");
    }
  }

  void GetPath(const std::string& key, std::string* out,
               const std::filesystem::path& base) {
    const auto it = values_.find(key);
    if (it == values_.end()) return;
    used_.insert(key);
    const std::filesystem::path p = it->second;
    const bool relative_to_file = p.is_relative() && where_[key] != "override";
    *out = (relative_to_file ? base / p : p).string();
  }

  void RejectUnused() const {
    for (const auto& [key, value] : values_) {
      if (used_.count(key) == 0) {
        throw std::runtime_error(where_.at(key) + ": unknown setting '" + key +
                                 "'");
      }
    }
  }

 private:
  std::map<std::string, std::string> values_;
  std::map<std::string, std::string> where_;
  std::set<std::string> used_;
};

}  // namespace

PipelineConfig LoadPipelineConfig(const std::string& path,
                                  const std::vector<std::string>& overrides) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open pipeline config " + path);
  Values values;
  std::string line;
  std::string section;
  for (int number = 1; std::getline(in, line); ++number) {
    line = Trim(line.substr(0, line.find_first_of("#;")));
    if (line.empty()) continue;
    const std::string where = path + ":" + std::to_string(number);
    if (line.front() == '[' && line.back() == ']') {
      section = Trim(line.substr(1, line.size() - 2));
      continue;
    }
    const auto eq = line.find('=');
    if (eq == std::string::npos || section.empty()) {
      throw std::runtime_error(where +
                               ": expected 'key = value' in a [section]");
    }
    values.Set(section + "." + Trim(line.substr(0, eq)),
               Trim(line.substr(eq + 1)), where);
  }
  for (const std::string& o : overrides) {
    const auto eq = o.find('=');
    if (eq == std::string::npos) {
      throw std::runtime_error("override '" + o +
                               "' must be section.key=value");
    }
    values.Set(Trim(o.substr(0, eq)), Trim(o.substr(eq + 1)), "override");
  }

  const std::filesystem::path base = std::filesystem::path(path).parent_path();
  PipelineConfig c;
  CameraOptions& cam = c.camera;
  values.GetPath("camera.feature_file", &cam.feature_file, base);
  values.Get("camera.exposure_us", &cam.exposure_us, 0.0, 1e6);
  values.Get("camera.gain_db", &cam.gain_db, -1.0, 48.0);
  values.Get("camera.sensor_height", &cam.sensor_height, 0, 1080);
  values.Get("camera.unlimited_frame_rate", &cam.unlimited_frame_rate);
  values.Get("camera.sdk_buffers", &cam.sdk_buffers, 3, 64);
  values.Get("camera.grab_timeout_ms", &cam.grab_timeout_ms, 1, 10'000);

  values.Get("output.width", &cam.out_width, 2, 4096);
  values.Get("output.height", &cam.out_height, 2, 4096);
  values.Get("output.tensor", &cam.tensor);
  values.Get("output.tensor_fp16", &cam.tensor_fp16);
  values.Get("output.tensor_rgb", &cam.tensor_rgb);
  values.Get("output.shared_tile", &cam.shared_tile);
  values.Get("output.bgr", &cam.bgr);
  values.Get("output.nv12", &cam.nv12);
  values.Get("output.pool_size", &cam.pool_size, 2, 256);

  values.Get("threads.realtime_priority", &cam.realtime_priority);
  values.Get("threads.realtime_level", &cam.realtime_level, 1, 99);
  values.Get("threads.cpu", &cam.cpu, -1, 1023);
  values.Get("threads.watchdog_cpu", &cam.watchdog_cpu, -1, 1023);
  values.Get("threads.lock_memory", &cam.lock_memory);

  values.Get("timing.latency_us", &cam.latency_us, -1, 1'000'000);
  values.Get("timing.line_time_us", &cam.line_time_us, 0.0, 1000.0);
  values.Get("timing.latency_base_us", &cam.latency_base_us, 0, 100'000);
  values.Get("timing.clock_window_ms", &cam.clock_window_ms, 10, 600'000);
  values.Get("timing.clock_reset_ms", &cam.clock_reset_ms, 1, 600'000);
  values.Get("timing.clock_reset_frames", &cam.clock_reset_frames, 1, 1000);

  values.Get("watchdog.enabled", &cam.watchdog);
  values.Get("watchdog.stall_ms", &cam.stall_ms, 10, 60'000);
  values.Get("watchdog.check_ms", &cam.check_ms, 1, 10'000);
  values.Get("watchdog.retry_ms", &cam.retry_ms, 1, 60'000);
  values.Get("watchdog.retry_max_ms", &cam.retry_max_ms, 1, 600'000);
  values.Get("watchdog.usb_reset_after", &cam.usb_reset_after, 0, 1000);

  RecorderOptions& rec = c.recorder;
  values.Get("recorder.fps", &rec.fps, 0, 1000);
  values.Get("recorder.bitrate", &rec.bitrate, 100'000, 200'000'000);
  values.Get("recorder.queue", &rec.queue, 1, 64);
  values.Get("recorder.finish_timeout_ms", &rec.finish_timeout_ms, 0, 60'000);
  values.Get("recorder.segment_s", &rec.segment_s, 0, 3600);
  values.Get("recorder.max_files", &rec.max_files, 0, 100'000);
  values.Get("recorder.min_free_mb", &rec.min_free_mb, 0, 1'000'000);

  values.RejectUnused();
  if (cam.out_width % 2 != 0 || cam.out_height % 2 != 0) {
    throw std::runtime_error("output.width and output.height must be even");
  }
  return c;
}

}  // namespace jetson_camera
