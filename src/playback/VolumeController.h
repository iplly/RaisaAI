#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>

extern std::atomic<uint8_t> g_volume;
extern std::atomic<uint8_t> g_actual;
extern std::mutex mtx;

void setVolume(uint8_t v);
