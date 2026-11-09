// Resets the USB port of every device with a given vendor id (default:
// Hikrobot 2bdf) via the USBDEVFS_RESET ioctl, the software equivalent of a
// replug. A field tool, and the way to exercise the camera watchdog.

#include <dirent.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {

std::string ReadLine(const std::string& path) {
  std::ifstream f(path);
  std::string s;
  std::getline(f, s);
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string vendor = argc > 1 ? argv[1] : "2bdf";
  DIR* dir = opendir("/sys/bus/usb/devices");
  if (dir == nullptr) return 1;
  int resets = 0;
  while (const dirent* e = readdir(dir)) {
    const std::string base = std::string("/sys/bus/usb/devices/") + e->d_name;
    if (ReadLine(base + "/idVendor") != vendor) continue;
    char node[64];
    std::snprintf(node, sizeof(node), "/dev/bus/usb/%03d/%03d",
                  std::atoi(ReadLine(base + "/busnum").c_str()),
                  std::atoi(ReadLine(base + "/devnum").c_str()));
    const int fd = open(node, O_WRONLY);
    if (fd < 0) {
      std::perror(node);
      continue;
    }
    const int r = ioctl(fd, USBDEVFS_RESET, 0);
    std::printf("%s (%s): reset %s\n", node,
                ReadLine(base + "/product").c_str(), r == 0 ? "ok" : "failed");
    resets += r == 0;
    close(fd);
  }
  closedir(dir);
  return resets ? 0 : 1;
}
