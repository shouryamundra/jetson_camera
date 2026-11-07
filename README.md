# jetson_camera

Hikrobot USB camera → pinned memory shared by CPU and GPU → CUDA demosaic to BGRA → optional
hardware H.265 recording to `.mkv`. Built for Jetson Orin (JetPack 6.1).

```cpp
io::Camera camera("config/camera.ini");
io::Recorder recorder("out.mkv", camera.width(), camera.height(), {.fps = 60});
while (auto frame = camera.read()) {
  my_kernel<<<grid, block, 0, frame->stream>>>(frame->bgra, frame->width, frame->height);
  recorder.record(*frame);  // optional; copies the finished frame into the encoder
}
```

- `read()` waits for the previous frame's GPU work, takes the frame the SDK holds (at most one
  frame old) and queues the demosaic on `frame.stream`. Queue your work on that stream; anything
  on another stream must finish before the next `read()`, which overwrites the frame.
- `record()` skips frames faster than `fps` and drops frames if the encoder is 3 behind.
- GStreamer: `appsrc (BGRx) ! nvvidconv ! NVMM NV12 ! nvv4l2h265enc ! h265parse ! matroskamux`.

## Build and run on the Jetson

```sh
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev   # MVS runtime in /opt/MVS
# Check the hardware path first:
gst-inspect-1.0 nvvidconv | grep -A3 "SINK template" -m1    # must list BGRx for video/x-raw
gst-launch-1.0 -e videotestsrc num-buffers=300 ! video/x-raw,format=BGRx,width=1440,height=1080 \
  ! nvvidconv ! 'video/x-raw(memory:NVMM),format=NV12' ! nvv4l2h265enc ! h265parse \
  ! matroskamux ! filesink location=test.mkv
cmake -B build && cmake --build build -j && (cd build && ctest)
./build/capture_demo config/camera.ini --seconds 20 --record out.mkv --fps 60
ffprobe out.mkv
```

To check frames are fresh: with slow processing, the printed frame counter should jump by the
number of frames that arrived in between. A killed recording may lack an index; repair it with
`ffmpeg -i out.mkv -c copy fixed.mkv`.
