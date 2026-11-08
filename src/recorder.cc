// Hardware-encoded recording of NV12 frames to H.265 Matroska.
//
// appsrc wraps each frame's pinned NV12 plane (zero copy; GStreamer releases
// the FrameRef); leaky-downstream drops the oldest queued frame; pad probes
// measure the encoder.

#include "src/recorder.h"

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace jetson_camera {
namespace {

constexpr std::size_t kMaxPending = 256;

std::int64_t SteadyUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// NV12 from system memory -> VIC copies it into NVMM (no colour conversion)
// -> NVENC H.265.
std::string Describe(int width, int height, const RecorderOptions& o) {
  const int rate = o.fps > 0 ? o.fps : 240;
  return "appsrc name=source is-live=true format=time block=false "
         "leaky-type=downstream max-buffers=" +
         std::to_string(o.queue) +
         " caps=video/x-raw,format=NV12,colorimetry=bt709,width=" +
         std::to_string(width) + ",height=" + std::to_string(height) +
         ",framerate=" + std::to_string(rate) +
         "/1 ! nvvidconv ! video/x-raw(memory:NVMM),format=NV12 ! "
         "nvv4l2h265enc bitrate=" +
         std::to_string(o.bitrate) +
         " ! h265parse name=parse ! queue ! matroskamux ! filesink name=sink";
}

void ReleaseFrame(gpointer frame) { delete static_cast<FrameRef*>(frame); }

struct Unref {
  void operator()(gpointer object) const noexcept { gst_object_unref(object); }
};
struct StopPipeline {
  void operator()(GstElement* pipeline) const noexcept {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
  }
};

}  // namespace

struct Recorder::Impl {
  RecorderOptions options;
  std::size_t frame_bytes = 0;
  std::unique_ptr<GstElement, StopPipeline> pipeline;
  std::unique_ptr<GstElement, Unref> source;
  // Producer-only pacing state.
  std::optional<std::int64_t> first_us;
  std::int64_t next_due_us = 0;

  mutable std::mutex mutex;  // Guards stats and pending (GStreamer threads).
  RecorderStats stats;
  std::unordered_map<std::uint64_t, std::int64_t> pending;  // PTS -> push us.

  static GstPadProbeReturn OnEncoded(GstPad*, GstPadProbeInfo* info,
                                     gpointer self_ptr) {
    auto* self = static_cast<Impl*>(self_ptr);
    const GstClockTime pts = GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER(info));
    const std::int64_t now = SteadyUs();
    std::lock_guard lock(self->mutex);
    ++self->stats.encoded;
    if (const auto it = self->pending.find(pts); it != self->pending.end()) {
      const double ms = static_cast<double>(now - it->second) / 1000.0;
      self->stats.latency_ms_sum += ms;
      self->stats.latency_ms_max = std::max(self->stats.latency_ms_max, ms);
      self->pending.erase(it);
    }
    return GST_PAD_PROBE_OK;
  }

  static GstPadProbeReturn OnWritten(GstPad*, GstPadProbeInfo* info,
                                     gpointer self_ptr) {
    auto* self = static_cast<Impl*>(self_ptr);
    std::lock_guard lock(self->mutex);
    self->stats.bytes += gst_buffer_get_size(GST_PAD_PROBE_INFO_BUFFER(info));
    return GST_PAD_PROBE_OK;
  }
};

Recorder::Recorder(const std::string& path, int width, int height,
                   RecorderOptions options)
    : impl_(std::make_unique<Impl>()) {
  Impl& m = *impl_;
  m.options = options;
  m.frame_bytes = static_cast<std::size_t>(width) *
                  static_cast<std::size_t>(height) * 3 / 2;
  m.pending.reserve(kMaxPending);
  gst_init(nullptr, nullptr);
  GError* error = nullptr;
  m.pipeline.reset(
      gst_parse_launch(Describe(width, height, options).c_str(), &error));
  if (error != nullptr) {
    const std::string message = error->message;
    g_error_free(error);
    throw std::runtime_error("encoder pipeline: " + message);
  }
  const auto find = [&m](const char* name) {
    return std::unique_ptr<GstElement, Unref>(
        gst_bin_get_by_name(GST_BIN(m.pipeline.get()), name));
  };
  m.source = find("source");
  g_object_set(find("sink").get(), "location", path.c_str(), nullptr);

  const std::unique_ptr<GstPad, Unref> encoded(
      gst_element_get_static_pad(find("parse").get(), "src"));
  gst_pad_add_probe(encoded.get(), GST_PAD_PROBE_TYPE_BUFFER, &Impl::OnEncoded,
                    &m, nullptr);
  const std::unique_ptr<GstPad, Unref> written(
      gst_element_get_static_pad(find("sink").get(), "sink"));
  gst_pad_add_probe(written.get(), GST_PAD_PROBE_TYPE_BUFFER, &Impl::OnWritten,
                    &m, nullptr);

  if (gst_element_set_state(m.pipeline.get(), GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    throw std::runtime_error("cannot start the encoder pipeline");
  }
}

Recorder::~Recorder() {
  gst_app_src_end_of_stream(GST_APP_SRC(impl_->source.get()));
  const std::unique_ptr<GstBus, Unref> bus(
      gst_element_get_bus(impl_->pipeline.get()));
  GstMessage* const done = gst_bus_timed_pop_filtered(
      bus.get(),
      static_cast<GstClockTime>(impl_->options.finish_timeout_ms) * GST_MSECOND,
      static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
  if (done != nullptr) {
    gst_message_unref(done);
  } else {
    std::fputs("recorder: encoder did not finish in time\n", stderr);
  }
  // The pipeline goes to NULL next, releasing every frame GStreamer holds.
}

void Recorder::Push(const FrameRef& frame) {
  Impl& m = *impl_;
  const std::int64_t t = frame.info().exposure_start_us;
  // Deadline pacing: record the first frame at or after each 1/fps tick (a
  // quarter period early is allowed), so the average rate matches `fps`.
  if (m.options.fps > 0) {
    const std::int64_t period = 1'000'000 / m.options.fps;
    if (m.first_us && t + period / 4 < m.next_due_us) {
      std::lock_guard lock(m.mutex);
      ++m.stats.skipped;
      return;
    }
    m.next_due_us = (m.first_us && t - m.next_due_us < period)
                        ? m.next_due_us + period
                        : t + period;
  }
  if (!m.first_us) m.first_us = t;

  // Wrap the frame's NV12 plane; GStreamer owns a FrameRef until it is done.
  auto* owner = new FrameRef(frame);
  GstBuffer* const buffer = gst_buffer_new_wrapped_full(
      GST_MEMORY_FLAG_READONLY, frame.buffers().nv12, m.frame_bytes, 0,
      m.frame_bytes, owner, ReleaseFrame);
  const auto pts = static_cast<std::uint64_t>(t - *m.first_us) * 1000;
  GST_BUFFER_PTS(buffer) = pts;
  {
    std::lock_guard lock(m.mutex);
    ++m.stats.pushed;
    if (m.pending.size() >= kMaxPending) m.pending.clear();  // Bounded.
    m.pending[pts] = SteadyUs();
  }
  gst_app_src_push_buffer(GST_APP_SRC(m.source.get()), buffer);
}

RecorderStats Recorder::stats() const {
  std::lock_guard lock(impl_->mutex);
  return impl_->stats;
}

}  // namespace jetson_camera
