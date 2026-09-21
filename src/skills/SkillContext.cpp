#include "skills/SkillContext.h"
#include "skills/Router.h"
#include "skills/Skill.h"
#include "skills/model/llmprompts.h"
#include <memory>

struct Skills g_skills;

void skill_init() {
  g_skills.weather = std::make_unique<WeatherSkill>();
  // g_skills.ytmusic = std::make_unique<YTMusicSkill>();
  g_skills.llmskill = std::make_unique<LLMSkill>();
  g_skills.vkmusic = std::make_unique<VKMusicSkill>();
  g_skills.timerskill = std::make_unique<TimerSkill>();

  router = {.model = Config::instance().get("ROUTER_MODEL"),
            .stream = false,
            .keep_alive = -1,
            .think = false,
            .options =
                {
                    .temperature = 0,
                    .top_p = 0,
                    .num_ctx = 65536,
                    .num_predict = 64,
                },
            .messages = {{.role = "system", .content = routerSystemPrompt}}};
}
