#pragma once
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include <iostream>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <optional>
#include <string>
#include <vector>

class AudioController {
  AVFormatContext *fmtCtx = nullptr;
  int audioStreamIndex = -1;

public:
  AudioController(std::string sample_rate, std::string channels,
                  std::string appName);
  bool read(AVPacket &pkt);
  void saveWav(const std::string &path, const std::vector<uint8_t> &pcm,
               uint32_t sampleRate = 48000, uint16_t channels = 1,
               uint16_t bits = 16);
  ~AudioController();
};
