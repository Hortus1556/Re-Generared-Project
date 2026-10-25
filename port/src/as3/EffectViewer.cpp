#include "as3/EffectViewer.h"

#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "as3/EffectList.h"
#include "as3/EventEffects.h"
#include "as3/HudEffects.h"
#include "as3/PartnerEffects.h"
#include "as3/ResolutionMgr.h"
#include "as3/Scene.h"
#include "as3/SurgeryData.h"
#include "as3/ToolEffects.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCTouch.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;

namespace {

const char* const kPartnerIds[8] = {"giblets", "saturday", "reverend", "flow", "bot", "nowe", "crumpet", "brutality"};
const char* const kPartnerArt[8] = {"Giblets", "Saturday", "Reverand", "Flow", "Bot", "Nowe", "Crumpet", "Brutality"};

struct Item {
    std::string name, arg;  // "name[=arg]"
    CCPoint at;
};

class EffectViewer : public Scene, public EffectList::Listener {
public:
    explicit EffectViewer(const std::string& spec) : m_spec(spec), m_list(this) {}

    void load() override {
        CCSize win = CCDirector::sharedDirector()->getWinSize();
        m_repeat = std::getenv("AS_EFFECTS_REPEAT") != nullptr;
        // the items
        size_t from = 0;
        while (from <= m_spec.size()) {
            size_t comma = m_spec.find(',', from);
            if (comma == std::string::npos) comma = m_spec.size();
            if (comma > from) {
                std::string s = m_spec.substr(from, comma - from);
                Item it;
                it.at = ccp(win.width / 2, win.height / 2);
                size_t atSign = s.find('@');
                if (atSign != std::string::npos) {
                    std::string where = s.substr(atSign + 1);
                    s = s.substr(0, atSign);
                    size_t colon = where.find(':');
                    if (colon != std::string::npos)
                        it.at = ccp(std::strtof(where.c_str(), nullptr), std::strtof(where.c_str() + colon + 1, nullptr));
                }
                size_t eq = s.find('=');
                it.name = s.substr(0, eq);
                if (eq != std::string::npos) it.arg = s.substr(eq + 1);
                m_items.push_back(it);
            }
            from = comma + 1;
        }
        // the art: the first surgery's, and the partners' named
        std::vector<std::string> sheets = {"surgery_common1_sheet", "surgery_mrGiblets1_sheet", "anim_iceant_sheet",
                                           "anim_poison_sheet"};
        for (const Item& it : m_items) {
            std::string id = it.name == "highfive" ? it.arg
                             : it.name == "giblets" ? "giblets"
                             : it.name == "baton"   ? "brutality"
                                                    : "";
            for (int i = 0; i < 8; ++i)
                if (id == kPartnerIds[i]) sheets.push_back(std::string("surgery_") + kPartnerArt[i] + "_sheet");
        }
        loadSheets(this, sheets);
        addChild(CCLayerColor::create(ccc4(40, 40, 40, 255)), -2);
        m_bg = CCSprite::createWithSpriteFrameName("bg_mrgibletsChest.png");
        if (m_bg) {
            setPositionOnCanvas(m_bg, 320, 192);
            addChild(m_bg, 0);
        }
        m_injuries = CCNode::create();
        addChild(m_injuries, 2);
        m_extras = CCNode::create();
        addChild(m_extras, 1);
        // a sprite on the "injuries" layer, for the chainsaw's shake
        if (CCSprite* s = CCSprite::createWithSpriteFrameName("ingame_comboheart-normal.png")) {
            s->setPosition(ccp(win.width * 0.5f, win.height * 0.35f));
            m_injuries->addChild(s);
        }
        m_hud = CCNode::create();
        m_hudDef = loadUIDefinition("common/in_game_ui.json");
        buildUI(ui, m_hud, m_hudDef, nullptr, nullptr);
        addChild(m_hud, 350);
        for (const Item& it : m_items) spawn(it);
        scheduleUpdate();
        setTouchRegistered(true);
    }

    void update(float dt) override {
        ++m_frames;
        // the scanner, as the screen's 0x39104 drives it while held
        if (m_held && m_tool == kScanner)
            if (ScannerEffect* s = dynamic_cast<ScannerEffect*>(m_list.toolEffect()))
                if (s->tick()) {
                    s->clear();
                    s->addBlip(m_touch, m_blip++ % 4);
                    s->restart();
                }
        if (m_dying) m_dying->setHealth(m_health);
        m_list.update(dt, this);
    }

    void effectFinished(Effect* e) override {
        platform::debugTrace("effect viewer: type %d ended at frame %d", e->getType(), m_frames);
        if (!m_repeat) return;
        for (const Item& it : m_items)
            if (it.name == m_names[e]) {
                spawn(it);
                break;
            }
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        m_touch = touch->getLocation();
        m_held = true;
        m_list.partnerTouchBegan(m_touch);
        if (m_cursor) {
            m_cursor->setPosition(m_touch);
            m_cursor->setVisible(true);
        }
        m_list.toolTouchBegan(m_touch);
        return true;
    }
    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        m_touch = touch->getLocation();
        m_list.partnerTouchMoved(m_touch);
        if (m_cursor) m_cursor->setPosition(m_touch);
        m_list.toolTouchMoved(m_touch);
    }
    void ccTouchEnded(CCTouch* touch, CCEvent*) override {
        m_touch = touch->getLocation();
        m_held = false;
        m_list.partnerTouchEnded(m_touch);
        if (m_cursor) m_cursor->setVisible(false);
        m_list.toolTouchEnded(m_touch);
    }

private:
    void spawn(const Item& it) {
        Effect* e = nullptr;
        const std::string& n = it.name;
        int tool = n == "lighter" ? kLighter : n == "cream" ? kCream : n == "scanner" ? kScanner : n == "chainsaw" ? kChainsaw : -1;
        static const char* const kEvents[12] = {"damage", "shock", "electricity", "blood", "poison", "bluescreen",
                                                "explosion", "frozen", "whitescreen", "", "poisoned", "greenscreen"};
        int event = -1;
        for (int k = 0; k < 12; ++k)
            if (*kEvents[k] && n == kEvents[k]) event = k;
        if (tool >= 0) {
            if (m_list.toolEffect()) return;  // one tool
            EffectList::ToolContext c;
            c.touch = m_touch;
            c.stageBackground = "bg_mrgibletsChest.png";
            c.bg = m_bg;
            c.injuryLayer = m_injuries;
            c.bgExtras = m_extras;
            e = m_list.selectTool(tool, c);
            m_tool = tool;
            if (tool == kChainsaw) {
                m_cursor = ChainsawTrailEffect::create();
                m_cursor->setVisible(false);
                addChild(m_cursor, 0xe1);
            }
        } else if (event >= 0) {
            e = m_list.eventEffect(event, it.at, &ui);
        } else {
            if (n == "dying") {
                m_dying = DyingFadeEffect::create();
                m_health = it.arg.empty() ? 0 : std::strtof(it.arg.c_str(), nullptr);
                m_list.dyingFade = m_dying;
                e = m_dying;
            } else if (n == "success") e = SuccessEffect::create();
            else if (n == "outoftime") e = OutOfTimeEffect::create();
            else if (n == "sparkle") {
                SparkleEffect* s = SparkleEffect::create();
                s->setPosition(it.at);
                m_list.sparkle = s;
                e = s;
            } else if (n.compare(0, 3, "tag") == 0 && n.size() == 4) {
                int points = it.arg.empty() ? 30 : std::atoi(it.arg.c_str());
                e = TagEffect::create(n[3] - '0', points, false);
                e->setPosition(ccp(it.at.x, it.at.y + e->getContentSize().height * 0.5f));
            } else if (n == "cointag") {
                e = CoinTagEffect::create();
                e->setPosition(it.at);
            } else if (n == "arrow") {
                CCNode* tool = ui.node(it.arg.empty() ? "ingame_tool_Lighter" : it.arg);
                int mode = kUIAbsolute;
                for (const UIItemDef& d : m_hudDef.items)
                    if (tool && ui.node(d.name) == tool) mode = d.mode;
                e = TutorialArrowEffect::create(tool, mode);
            } else if (n == "gradient") e = TutorialGradientEffect::create();
            else if (n == "highfive") {
                e = HighFiveEffect::create(it.arg.empty() ? "giblets" : it.arg);
                int index = m_list.add(e, true);
                m_list.highFiveIndex = index;
                m_names[e] = n;
                return;
            } else if (n == "giblets") e = GibletsEffect::create();
            else if (n == "saturdayheart") {
                SaturdayHeartEffect* h = SaturdayHeartEffect::create();
                h->setPosition(it.at);
                h->moveTo(ccp(it.at.x + 100, it.at.y + 60));
                e = h;
            } else if (n == "saturdayparticle") {
                SaturdayParticleEffect* p = SaturdayParticleEffect::create();
                p->setPosition(it.at);
                p->moveTo(ccp(it.at.x + 100, it.at.y + 60));
                e = p;
            } else if (n == "bot") {
                e = BotEffect::create();
                e->setPosition(it.at);
            } else if (n == "flow") {
                e = FlowMasterEffect::create();
                e->setPosition(it.at);
            } else if (n == "brutality") {
                e = BrutalityParticlesEffect::create();
                e->setPosition(it.at);
            } else if (n == "baton") {
                e = BatonInEffect::create();
                e->setPosition(it.at);
            } else if (n == "reverand") {
                e = ReverandCursorEffect::create();
                e->setPosition(it.at);
                int index = m_list.add(e, true);
                m_list.partnerIndex = index;
                m_names[e] = n;
                return;
            } else if (n == "reverandtools") {
                ReverandToolsEffect* r = ReverandToolsEffect::create();
                r->addToolParticles(ui);
                e = r;
            } else if (n == "reverandend") e = ReverandEndEffect::create();
            else if (n == "sheen") {
                ToolsSheenEffect* s = ToolsSheenEffect::create();
                s->addToolSheens(ui);
                e = s;
            }
            if (e) m_list.add(e, true);
        }
        if (e) {
            m_names[e] = n;
            platform::debugTrace("effect viewer: %s (type %d, z %d) at frame %d", n.c_str(), e->getType(),
                                 EffectList::zOrderOf(e->getType()), m_frames);
        } else {
            platform::debugTrace("effect viewer: %s made nothing", n.c_str());
        }
    }

    std::string m_spec;
    std::vector<Item> m_items;
    std::map<Effect*, std::string> m_names;
    EffectList m_list;
    UIDefinition m_hudDef;
    CCNode* m_hud = nullptr;
    CCSprite* m_bg = nullptr;
    CCNode* m_injuries = nullptr;
    CCNode* m_extras = nullptr;
    ChainsawTrailEffect* m_cursor = nullptr;
    DyingFadeEffect* m_dying = nullptr;
    float m_health = 0;
    CCPoint m_touch;
    bool m_held = false;
    bool m_repeat = false;
    int m_tool = -1;
    int m_blip = 0;
    int m_frames = 0;
};

}  // namespace

Scene* effectViewer() {
    if (!platform::debugMode()) return nullptr;
    const char* spec = std::getenv("AS_EFFECTS");
    if (!spec || !*spec) return nullptr;
    EffectViewer* v = new EffectViewer(spec);
    v->setName("EffectViewer");
    v->init();
    v->autorelease();
    return v;
}

}  // namespace mt
