// Purpose: Declare an RAII handle to a Hikrobot USB camera opened through the MVS SDK.
// Paradigm: Object-oriented resource wrapper; the SDK handle is owned by a unique_ptr.
// Role: Keeps every vendor call and error code out of the capture logic.

#ifndef IO__MVS_DEVICE_HPP
#define IO__MVS_DEVICE_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace io
{
class MvsDevice
{
public:
  struct Geometry
  {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t payload = 0;     // bytes per frame
    std::uint32_t pixel_type = 0;  // SDK GVSP pixel type
    friend bool operator==(const Geometry &, const Geometry &) = default;
  };

  // Opens the first USB camera and applies the MVS feature file at `ini_path`.
  // Throws std::runtime_error if there is no camera or any setup call fails.
  explicit MvsDevice(const std::string & ini_path);

  [[nodiscard]] const Geometry & geometry() const noexcept { return geometry_; }

  void start_grabbing();
  void stop_grabbing() noexcept;

  // Waits up to `timeout` for one frame and writes it to `out`, which must hold geometry().payload
  // bytes. Returns the camera's frame counter, or nothing on timeout or error.
  [[nodiscard]] std::optional<std::uint32_t> get_frame(
    std::span<std::uint8_t> out, std::chrono::milliseconds timeout) noexcept;

private:
  struct HandleClose
  {
    void operator()(void * handle) const noexcept;
  };

  std::unique_ptr<void, HandleClose> handle_;
  Geometry geometry_;
};

}  // namespace io

#endif  // IO__MVS_DEVICE_HPP
