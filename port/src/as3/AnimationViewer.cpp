#include "as3/AnimationViewer.h"

#include <cstdlib>
#include <sstream>

#include "as3/Animation.h"
#include "as3/SceneMgr.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCLayer.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;

namespace {

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream in(s);
    std::string part;
    while (std::getline(in, part, sep))
        if (!part.empty()) out.push_back(part);
    return out;
}

// "name[~][*loops]" → name, backwards (~), loops (-1: for ever)
void parseName(const std::string& entry, std::string& name, int& loops, bool& reverse) {
    size_t star = entry.find('*');
    name = entry.substr(0, star);
    loops = star == std::string::npos ? -1 : std::atoi(entry.c_str() + star + 1);
    reverse = !name.empty() && name.back() == '~';
    if (reverse) name.pop_back();
}

}  // namespace

AnimationViewer* AnimationViewer::createFromEnv() {
    if (!platform::debugMode()) return nullptr;
    const char* spec = std::getenv("AS_ANIM");
    if (!spec || !*spec) return nullptr;
    std::vector<std::string> parts = split(spec, ',');
    if (parts.size() < 2) return nullptr;
    std::vector<std::string> names(parts.begin() + 2, parts.end());
    return create(parts[0], split(parts[1], '+'), names);
}

AnimationViewer* AnimationViewer::create(const std::string& file, const std::vector<std::string>& sheets,
                                         const std::vector<std::string>& names) {
    AnimationViewer* s = new AnimationViewer;
    s->setName("AnimationViewer");
    s->m_file = file;
    s->m_sheets = sheets;
    s->m_names = names;
    s->init();
    s->autorelease();
    return s;
}

void AnimationViewer::load() {
    loadSheets(this, m_sheets);
    CCSize win = CCDirector::sharedDirector()->getWinSize();
    addChild(CCLayerColor::create(ccc4(60, 70, 90, 255), win.width, win.height));
    m_anim = Animation::create(m_file.c_str(), 1);
    m_anim->setPosition(ccp(win.width * 0.5f, win.height * 0.5f));
    addChild(m_anim);
    bool first = true;
    for (const std::string& entry : m_names) {
        if (entry[0] == '@') {  // a marker on that control point
            CCLayerColor* dot = CCLayerColor::create(ccc4(255, 220, 0, 255), 6, 6);
            dot->ignoreAnchorPointForPosition(false);
            dot->setAnchorPoint(ccp(0.5f, 0.5f));
            dot->retain();  // (an attached node is not the animation's: its owner keeps it)
            m_markers.push_back(dot);
            m_anim->attach(entry.c_str() + 1, dot);
            continue;
        }
        std::string name;
        int loops;
        bool reverse;
        parseName(entry, name, loops, reverse);
        if (first) m_anim->play(name.c_str(), loops, reverse);
        else m_anim->queue(name.c_str(), loops, reverse);
        first = false;
    }
    m_label = createLabel("", "font-UIGlobalsSmall", win.width, kCCTextAlignmentLeft);
    if (m_label) {
        m_label->setAnchorPoint(ccp(0, 1));
        m_label->setPosition(ccp(4, win.height - 4));
        addChild(m_label, 10);
    }
    tick(0);
}

void AnimationViewer::unload() {
    unloadSheets(m_sheets);
    for (CCNode* n : m_markers) n->release();
    m_markers.clear();
}

void AnimationViewer::onShown() {
    Scene::onShown();
    setTouchRegistered(true);
    schedule(schedule_selector(AnimationViewer::tick));
}

void AnimationViewer::tick(float) {
    if (!m_label || !m_anim) return;
    const char* name = m_anim->currentName();
    std::string text = m_file + "\n" + (name ? name : "(stopped)");
    if (name) text += " frame " + std::to_string(m_anim->frame()) + "/" + std::to_string(m_anim->lengthOf(name));
    m_label->setString(text.c_str());
}

bool AnimationViewer::ccTouchBegan(CCTouch*, CCEvent*) {
    if (shownForLessThanMinimum()) return false;
    std::vector<std::string> names = m_names;
    if (names.size() > 1) {
        names.push_back(names.front());
        names.erase(names.begin());
    }
    SceneMgr::sharedSceneMgr()->replaceScene(create(m_file, m_sheets, names));
    return true;
}

}  // namespace mt
