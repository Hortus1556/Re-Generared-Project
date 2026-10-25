#include "as3/ToolArt.h"

#include <map>

#include "as3/SurgeryData.h"
#include "core/Json.h"
#include "engine/cocos/CCFileUtils.h"
#include "engine/cocos/CCTexture2D.h"

namespace mt {
namespace toolart {

using namespace cocos2d;

namespace {

const char* const kStates[3] = {"normal", "selected", "disabled"};

CCTexture2D* picture(const std::string& file) {
    if (!CCFileUtils::sharedFileUtils()->isFileExist(file)) return nullptr;
    return CCTextureCache::sharedTextureCache()->addImage(file.c_str());
}

const json::Value& layouts() {
    static json::Value root;
    static bool read = false;
    if (!read) {
        read = true;
        std::string text;
        if (CCFileUtils::sharedFileUtils()->readText("toolart/tools.json", text)) json::parse(text, root);
    }
    return root;
}

}  // namespace

std::string fromSpritePrefix(const std::string& prefix) {
    std::string n = prefix;
    if (n.compare(0, 7, "ingame_") == 0) n.erase(0, 7);
    const std::string tail = "_button";
    if (n.size() > tail.size() && n.compare(n.size() - tail.size(), tail.size(), tail) == 0) n.erase(n.size() - tail.size());
    return n;
}

std::string ofTool(int tool, bool drawOut) {
    // (the tool types' buttons, in_game_ui.json's sprite prefixes)
    static const char* const kNames[kToolCount] = {"lighter",   "gel",       "stapler", "pizzacutter", "battery",      "tongs",
                                                   "chainsaw",  "injection", "vacuum",  "clamps",      "metaldetector"};
    if (tool < 0 || tool >= kToolCount) return std::string();
    if (tool == kInjection && drawOut) return "injection_out";
    return kNames[tool];
}

CCTexture2D* card(int state) { return picture(std::string("toolart/card-") + kStates[state < 0 || state > 2 ? 0 : state] + ".png"); }

CCTexture2D* tool(const std::string& name, int state) {
    if (name.empty()) return nullptr;
    return picture("toolart/" + name + "-" + kStates[state < 0 || state > 2 ? 0 : state] + ".png");
}

namespace {
float at(const json::Value& v, size_t i) { return static_cast<float>(v[i].num()); }
}  // namespace

bool layout(const std::string& name, ToolLayout& out) {
    const json::Value& t = layouts()["tools"][name];
    const json::Value& c = t["card"];
    const json::Value& b = t["tool"];
    const json::Value& r = t["circle"];
    if (c.size() != 2 || b.size() != 4 || r.size() != 3) return false;
    out.cardTopLeft = ccp(at(c, 0), at(c, 1));
    out.bounds = CCRectMake(at(b, 0), at(b, 1), at(b, 2), at(b, 3));
    out.circleMiddle = ccp(at(r, 0), at(r, 1));
    out.circleRadius = at(r, 2);
    return true;
}

bool cardLayout(float& longer, CCRect& inside) {
    const json::Value& c = layouts()["card"];
    const json::Value& i = c["inside"];
    if (!c.has("longer") || i.size() != 4) return false;
    longer = static_cast<float>(c["longer"].num());
    inside = CCRectMake(at(i, 0), at(i, 1), at(i, 2) - at(i, 0), at(i, 3) - at(i, 1));
    return true;
}

}  // namespace toolart
}  // namespace mt
