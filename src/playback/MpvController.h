#pragma once
#include "playback/VolumeController.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <sys/types.h>

extern std::atomic<pid_t> g_vkMpvPid;
extern std::atomic<bool> g_pause;

void mpvSetVolume_(uint8_t v, pid_t pid = g_vkMpvPid);
void mpvSetVolume(uint8_t v, pid_t pid = g_vkMpvPid);
void mpvTogglePause(pid_t pid = g_vkMpvPid);
pid_t spawnMPV(const std::string &url, uint8_t volume = g_volume);
