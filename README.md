# jetson_camera

Low-latency capture for a Hikrobot USB3 camera on Jetson Orin. Each frame goes
from USB to a model-ready GPU tensor in one CUDA kernel, with no CPU copy, and
carries the time it was exposed.

```cpp
jc::Camera camera(config.camera, [&](const jc::FrameRef& f) { recorder.Push(f); });

std::uint64_t seen = 0;
while (running) {
  auto frame = camera.Latest(seen, 100ms);  // newest frame, never a backlog
  if (!frame) continue;
  seen = frame->info().seq;
  Infer(frame->buffers().tensor);           // 1x3x540x960 fp16, on the GPU
  Track(frame->info().ExposureMidUs());     // CLOCK_REALTIME microseconds
}
```

## Performance

AGX Orin, MV-CS016-10UC, 4 ms exposure, clocks locked, recording at 30 fps,
15 ms of simulated inference running alongside:

| | |
|---|---|
| Capture | 216 fps, no frames lost |
| USB frame to tensor ready | 0.23 ms (p99 0.35 ms) |
| Frame age when the detector takes it | 9.4 ms from mid-exposure (p99 11.6 ms) |
| Recording | 30 fps, 6.7 ms to encode, never delays capture |

## Features

- **Newest frame only.** A slow consumer gets the latest frame; capture never
  waits for anyone.
- **Zero copy.** The GPU reads the SDK's receive buffers directly, and the
  recorder encodes straight from the frame pool.
- **One kernel per frame.** Demosaic, 1.5x area downscale, fp16 tensor and the
  NV12 frame for the encoder in a single pass.
- **Exposure timestamps** derived from the camera clock, about 30 us jitter.
- **Self-healing.** A watchdog reopens the camera after a disconnect or stall
  and resets its USB port if needed.
- **One config file.** Every tunable lives in `config/pipeline.ini`; unknown
  keys are errors.

## Setup

Needs JetPack 6, the MVS SDK 4.9 runtime in `/opt/MVS`, and GStreamer headers.

```sh
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
sudo deploy/setup_jetson.sh            # journal, USB power, RT limits, USB IRQs
cmake -B build && cmake --build build -j && (cd build && ctest)
export MVCAM_COMMON_RUNENV=/opt/MVS/lib
deploy/run_rt.sh ./build/capture_bench --seconds 30 --record out.mkv
```

Camera image settings are in `config/camera.ini` (an MVS feature file).
`deploy/README.md` covers core pinning and isolation.
