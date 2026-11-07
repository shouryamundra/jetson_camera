# jetson_camera

Hikrobot USB camera to GPU memory on Jetson Orin (JetPack 6, CUDA 12). Raw Bayer frames land in
pinned, GPU-mapped memory with no CUDA copy; one kernel demosaics them (optionally cropped to a
region of interest) to BGRA8 for your own GPU work.

```cpp
io::GpuCamera camera("config/camera.ini", io::Roi{x, y, w, h});  // Roi optional; {} = full frame
camera.start();
io::GpuFrame frame;
while (camera.read(frame)) {             // newest frame; earlier unread frames are dropped
  my_kernel<<<grid, block, 0, frame.stream>>>(frame.bgra, frame.width, frame.height, frame.pitch);
}                                        // work on frame.stream is waited for by the next read()
camera.stop();
```

`frame.bgra` is valid until the next `read()`. `frame.index` is the camera's frame counter; a gap
means dropped frames. `config/camera.ini` is an MVS feature file: exposure, gain, white balance,
gamma and pixel format come from it, not from code.

## Build and run on the Jetson

1. Install the Hikrobot MVS Linux (arm64) runtime, which provides the USB transport layer
   `libMvCameraControl.so` loads at runtime (default `/opt/MVS`), and its udev rule so the camera
   is accessible without root.
2. `cmake -B build && cmake --build build -j`
3. `./build/capture_demo config/camera.ini --seconds 10 --snapshot snap.ppm [--roi x,y,w,h]`
4. `ctest --test-dir build` runs the host-only tests (demosaic math, frame handoff).

The 8.33 ms exposure in `config/camera.ini` caps the camera at about 120 fps.
