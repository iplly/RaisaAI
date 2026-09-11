#pragma once
#include "skills/Skill.h"
#include <memory>

struct Skills {
  std::unique_ptr<WeatherSkill> weather;
  // std::unique_ptr<YTMusicSkill> ytmusic;
  std::unique_ptr<VKMusicSkill> vkmusic;
  std::unique_ptr<LLMSkill> llmskill;
  std::unique_ptr<TimerSkill> timerskill;
};

extern struct Skills g_skills;
void skill_init();