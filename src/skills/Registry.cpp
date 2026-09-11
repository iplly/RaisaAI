#include "skills/Registry.h"
#include "skills/SkillContext.h"
#include "skills/ToolDefs.h"
#include <nlohmann/json.hpp>
#include <vector>

std::vector<SkillReg> g_registry = {
    {"WeatherSkill",
     weatherTool,
     {"сводк", "подробн", "весь день"},
     [](json a) { g_skills.weather->execute(a); }},
    {"VKMusicSkill",
     vkmusicTool,
     {},
     [](json a) { g_skills.vkmusic->execute(a); }},
    {"TimerSkill", timerTool, {}, [](json a) {
       g_skills.timerskill->execute(a);
     }}};
