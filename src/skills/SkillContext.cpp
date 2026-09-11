#include "core/Config.h"
#include "skills/Router.h"
#include "skills/Skill.h"
#include "skills/SkillContext.h"
#include <memory>

struct Skills g_skills;

void skill_init() {
  qwen1_7Data["model"] = Config::instance().get("ROUTER_MODEL");
  g_skills.weather = std::make_unique<WeatherSkill>();
  // g_skills.ytmusic = std::make_unique<YTMusicSkill>();
  g_skills.llmskill = std::make_unique<LLMSkill>();
  g_skills.vkmusic = std::make_unique<VKMusicSkill>();
  g_skills.timerskill = std::make_unique<TimerSkill>();
}