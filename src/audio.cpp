#include "audio.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include <fstream>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <string>

AudioController::AudioController(std::string sample_rate, std::string channels,
                                 std::string appName) {
  avdevice_register_all();
  const AVInputFormat *inputFmt = av_find_input_format("pulse");
  if (!inputFmt) {
    throw std::runtime_error("Не найден формат pulseaudio");
  }

  AVDictionary *options = nullptr;
  av_dict_set(&options, "sample_rate", sample_rate.c_str(), 0);
  av_dict_set(&options, "channels", channels.c_str(), 0);
  av_dict_set(&options, "application_name", appName.c_str(), 0);

  AVFormatContext *fmtCtx = nullptr;
  int ret = avformat_open_input(&fmtCtx, "default", inputFmt, &options);
  av_dict_free(&options);

  if (ret < 0) {
    char errbuf[1024];
    av_strerror(ret, errbuf, sizeof(errbuf));
    throw std::runtime_error(std::string("Не удалось открыть устройство: ") +
                             errbuf);
  }
  this->fmtCtx = fmtCtx;

  int audioStreamIndex = -1;
  for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
    if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      audioStreamIndex = i;
      break;
    }
  }
  if (audioStreamIndex == -1) {
    avformat_close_input(&fmtCtx);
    throw std::runtime_error("Аудиопоток не найден");
  }
  this->audioStreamIndex = audioStreamIndex;
}

bool AudioController::read(AVPacket &pkt) {
  av_packet_unref(&pkt);

  while (av_read_frame(fmtCtx, &pkt) >= 0) {
    if (pkt.stream_index == audioStreamIndex)
      return true;
    av_packet_unref(&pkt);
  }
  return false;
}
void AudioController::saveWav(const std::string &path,
                              const std::vector<uint8_t> &pcm,
                              uint32_t sampleRate, uint16_t channels,
                              uint16_t bits) {
  std::ofstream f(path, std::ios::binary);
  uint32_t dataSize = pcm.size();
  uint32_t byteRate = sampleRate * channels * bits / 8;
  uint16_t blockAlign = channels * bits / 8;

  f.write("RIFF", 4);
  uint32_t riffSize = 36 + dataSize; // общий размер - 8
  f.write(reinterpret_cast<const char *>(&riffSize), 4);
  f.write("WAVE", 4);
  f.write("fmt ", 4);
  uint32_t fmtSize = 16;
  f.write(reinterpret_cast<const char *>(&fmtSize), 4);
  uint16_t audioFormat = 1; // 1 = PCM без сжатия
  f.write(reinterpret_cast<const char *>(&audioFormat), 2);
  f.write(reinterpret_cast<const char *>(&channels), 2);
  f.write(reinterpret_cast<const char *>(&sampleRate), 4);
  f.write(reinterpret_cast<const char *>(&byteRate), 4);   // 96000
  f.write(reinterpret_cast<const char *>(&blockAlign), 2); // 2
  f.write(reinterpret_cast<const char *>(&bits), 2);
  f.write("data", 4);
  f.write(reinterpret_cast<const char *>(&dataSize), 4);
  f.write(reinterpret_cast<const char *>(pcm.data()), pcm.size());
}

AudioController::~AudioController() { avformat_close_input(&fmtCtx); }
