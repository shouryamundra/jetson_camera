// Checks the pipeline config loader: values, defaults, overrides, path
// resolution, and that typos and bad values are rejected. Host-only.

#include "src/config.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace jc = jetson_camera;

int failures = 0;

#define EXPECT(c)                                      \
  do {                                                 \
    if (!(c)) {                                        \
      std::printf("FAIL line %d: %s\n", __LINE__, #c); \
      ++failures;                                      \
    }                                                  \
  } while (0)

std::string Write(const std::string& text) {
  const std::string path = "/tmp/jc_config_test.ini";
  std::ofstream(path) << text;
  return path;
}

bool Throws(const std::string& text,
            const std::vector<std::string>& overrides = {}) {
  try {
    (void)jc::LoadPipelineConfig(Write(text), overrides);
  } catch (const std::runtime_error&) {
    return true;
  }
  return false;
}

}  // namespace

int main() {
  const std::string good =
      "# comment\n[camera]\nfeature_file = camera.ini ; trailing comment\n"
      "exposure_us = 4000\n[output]\ntensor_fp16 = false\n"
      "[recorder]\nfps = 45\n[watchdog]\nstall_ms = 250\n";
  const jc::PipelineConfig c = jc::LoadPipelineConfig(
      Write(good), {"recorder.bitrate=4000000", "camera.gain_db=9.5"});
  EXPECT(c.camera.feature_file == "/tmp/camera.ini");  // Relative to the file.
  EXPECT(c.camera.exposure_us == 4000);
  EXPECT(!c.camera.tensor_fp16);
  EXPECT(c.camera.gain_db == 9.5);
  EXPECT(c.camera.sensor_height == 812);  // Untouched default.
  EXPECT(c.camera.stall_ms == 250);
  EXPECT(c.recorder.fps == 45);
  EXPECT(c.recorder.bitrate == 4'000'000);

  const jc::PipelineConfig d = jc::LoadPipelineConfig(Write("[recorder]\n"));
  EXPECT(d.recorder.fps == 30);  // Default recording rate.
  EXPECT(!d.camera.bgr);         // No CPU image by default.

  EXPECT(Throws("[camera]\nexposre_us = 10\n"));         // Typo in key.
  EXPECT(Throws("[recorder]\nfps = fast\n"));            // Not a number.
  EXPECT(Throws("[recorder]\nfps = 30fps\n"));           // Trailing junk.
  EXPECT(Throws("[camera]\nsdk_buffers = 1\n"));         // Out of range.
  EXPECT(Throws("[output]\nbgr = maybe\n"));             // Not a boolean.
  EXPECT(Throws("[output]\nwidth = 961\n"));             // Must be even.
  EXPECT(Throws("exposure_us = 10\n"));                  // Outside a section.
  EXPECT(Throws("[camera]\n", {"camera.nope=1"}));       // Unknown override.
  EXPECT(Throws("[camera]\n", {"camera.exposure_us"}));  // Malformed override.

  std::printf(failures ? "config_test FAILED\n" : "config_test passed\n");
  return failures ? 1 : 0;
}
