// Purpose: Implement hardware-encoded recording to an H.265 Matroska file.
// Paradigm: Push copied frames into a leaky appsrc; GStreamer threads convert, encode and write.
// Role: Stores camera video to disk on the encoder's threads, like Tongji's Recorder.

#include "recorder.hpp"

#include <gst/app/gstappsrc.h>

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace io
{
namespace
{
constexpr GstClockTime kFinishTimeout = 5 * GST_SECOND;

// BGRx from system memory -> VIC converts to NV12 in NVMM -> NVENC H.265 -> Matroska. The appsrc
// holds at most three frames and drops new ones beyond that, so a slow disk never backs up into
// the caller. The fixed framerate drives rate control; buffer timestamps carry the real timing.
std::string describe(const int width, const int height, const RecorderOptions & o)
{
  return "appsrc name=source is-live=true format=time block=false max-buffers=3 "
         "leaky-type=upstream caps=video/x-raw,format=BGRx,width=" +
         std::to_string(width) + ",height=" + std::to_string(height) +
         ",framerate=" + std::to_string(o.fps) +
         "/1 ! nvvidconv ! video/x-raw(memory:NVMM),format=NV12 ! nvv4l2h265enc bitrate=" +
         std::to_string(o.bitrate) + " ! h265parse ! queue ! matroskamux ! filesink name=sink";
}

}  // namespace

void Recorder::Stop::operator()(GstElement * const pipeline) const noexcept
{
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
}

Recorder::Recorder(
  const std::string & path, const int width, const int height, const RecorderOptions options)
: min_gap_(std::chrono::nanoseconds(900'000'000) / std::max(options.fps, 1))  // 10 % jitter slack
{
  gst_init(nullptr, nullptr);
  GError * error = nullptr;
  pipeline_.reset(gst_parse_launch(describe(width, height, options).c_str(), &error));
  if (error) {
    const std::string message = error->message;
    g_error_free(error);
    throw std::runtime_error("encoder pipeline: " + message);
  }

  const auto find = [this](const char * const name) {
    return std::unique_ptr<GstElement, Unref>(gst_bin_get_by_name(GST_BIN(pipeline_.get()), name));
  };
  source_ = find("source");
  g_object_set(find("sink").get(), "location", path.c_str(), nullptr);

  if (gst_element_set_state(pipeline_.get(), GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    throw std::runtime_error("cannot start the encoder pipeline");
}

Recorder::~Recorder()
{
  gst_app_src_end_of_stream(GST_APP_SRC(source_.get()));
  const std::unique_ptr<GstBus, Unref> bus(gst_element_get_bus(pipeline_.get()));
  GstMessage * const done = gst_bus_timed_pop_filtered(
    bus.get(), kFinishTimeout, static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
  if (done)
    gst_message_unref(done);
  else
    std::fputs("recorder: encoder did not finish in time\n", stderr);
}

void Recorder::record(const GpuFrame & frame)
{
  if (first_ && frame.timestamp - last_ < min_gap_) return;
  if (!first_) first_ = frame.timestamp;
  last_ = frame.timestamp;

  cuda::check(cudaStreamSynchronize(frame.stream), "cudaStreamSynchronize");
  const gsize bytes = static_cast<gsize>(frame.width) * static_cast<gsize>(frame.height) * 4;
  GstBuffer * const buffer = gst_buffer_new_memdup(frame.bgra, bytes);
  GST_BUFFER_PTS(buffer) = static_cast<GstClockTime>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(frame.timestamp - *first_).count());
  gst_app_src_push_buffer(GST_APP_SRC(source_.get()), buffer);  // takes ownership
}

}  // namespace io
