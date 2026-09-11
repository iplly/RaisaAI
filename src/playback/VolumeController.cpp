#include "playback/VolumeController.h"
#include "core/Paths.h"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>

std::atomic<uint8_t> g_volume{100};
std::atomic<uint8_t> g_actual{100};
std::mutex mtx;

void setVolume(uint8_t v) {
  g_volume.store(v);
  std::filesystem::create_directories(dirControl);
  std::ofstream out(dirControl + "volume");
  if (out)
    out << (int)v;
}
