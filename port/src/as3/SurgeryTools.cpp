// SurgeryScreen's parts for particular tools (re/as3/notes/surgery.md,
// "Tools"): kept apart from SurgeryScreen.cpp, which calls in here from its
// touches, update, buttons and events.
//
// - Each frame (0x34470) every tool cools down, and the selected one runs
//   (its update even when not held: the chainsaw slides on); held, it is
//   moved to the touch (0x34434; not the clamps, whose node holds the clamps
//   on the screen's own coordinates), its frame runs, and its damage is done
//   when it was of no use.
// - Before the hit test (0x39624): the scanner scans (0x39104), the clamps
//   take a touch on a placed clamp (its wheel, or a double tap to take it
//   off), the injection in its "in" mode is only tested over an injury's
//   area (event 12's rects): elsewhere it heals freely, over an injury it
//   would be misused.
// - The injection's HUD: its button opens a second one beside it ("out", to
//   extract) and the graph; the mode chosen moves to the injection's place
//   (0x3e4e4); its cooldown is shown by the bars over its button (0x34580).
// - Event 17 freezes the tools for a while (their buttons off, the tool put
//   down: 0x33f68), event 14 takes every clamp off, event 13 looks at the
//   stage again (a clamp taken off).
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "as3/Cheats.h"
#include "as3/ClampsController.h"
#include "as3/Injection.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/SurgeryScreen.h"
#include "as3/ToolEffects.h"
#include "as3/Tools2.h"
#include "engine/cocos/CCMenu.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;

namespace {

constexpr int kStateHidden = 19;

CCMenuItemToggle* toggle(CCNode* n) { return dynamic_cast<CCMenuItemToggle*>(n); }

}  // namespace

// ---- the injuries' areas (event 12) ----

void SurgeryScreen::addGraphicsRect(const InjuryEvent& e, InjuryState* injury) {
    // (0x3435c)
    GraphicsRect g;
    g.rect = e.rect;
    g.injury = injury;
    g.rotation = e.rotation;
    g.flag18 = true;
    g.injectable = injury ? !injury->m_fd94 : true;
    m_graphics.push_back(g);
    // (traced for test scripts: where the injection does not heal, cocos points)
    if (g.injectable)
        log("area #%d %.0f,%.0f size %.0f,%.0f rot %.0f", injury ? injury->soundChannel : -1, g.rect.origin.x,
            g.rect.origin.y, g.rect.size.width, g.rect.size.height, g.rotation);
}

bool SurgeryScreen::inGraphicsRect(const CCPoint& p, bool injectableOnly) {
    // (0x343a0) the point turned back by the injury's turn about its place
    // (0x799f0: (90 - (rotation + 90)) × -pi/180), in the rect
    for (const GraphicsRect& g : m_graphics) {
        if (injectableOnly && !g.injectable) continue;
        if (g.injury && g.injury->finished) continue;
        CCPoint c = g.injury ? g.injury->position : CCPointZero;
        float a = (90.0f - (static_cast<float>(static_cast<int>(g.rotation)) + 90.0f)) * -0.017453292519943295f;
        float dx = p.x - c.x, dy = p.y - c.y;
        float x = c.x + dx * std::cos(a) - dy * std::sin(a);
        float y = c.y + dx * std::sin(a) + dy * std::cos(a);
        if (g.rect.origin.x < x && x < g.rect.origin.x + g.rect.size.width && g.rect.origin.y < y &&
            y < g.rect.origin.y + g.rect.size.height)
            return true;
    }
    return false;
}

void SurgeryScreen::injuryRemoved(InjuryState* injury) {
    // (0x33188) its area off the list; its clamps off
    m_graphics.erase(std::remove_if(m_graphics.begin(), m_graphics.end(),
                                    [injury](const GraphicsRect& g) { return g.injury == injury; }),
                     m_graphics.end());
    if (!injury->clamps.empty())
        if (ClampsController* c = clamps()) c->releaseFor(injury);
    // (the port's) a loop still on its channel faded out: the original
    // stopped an injury's loop only where its class did, so a poison cut open
    // (PoisonArea vf4 0x19df78, SuperPoisonArea vf4 0x19b524 call their loop
    // again while their drops spill, and only the syringe's vf9 stops it)
    // went on looping until the surgery ended. Its last one-shot (a death, a
    // burst) still plays out.
    SoundMgr::sharedSoundMgr()->stopLoops(injury->soundChannel, true);
}

ClampsController* SurgeryScreen::clamps() {
    auto it = m_tools.find(kClamp);
    return it == m_tools.end() ? nullptr : static_cast<ClampsController*>(it->second);
}

// ---- each frame ----

void SurgeryScreen::placeTool() {
    // (0x34434) the tool at the touch, its node at the tool
    if (!m_tool || m_tool->type == kClamp) return;
    m_tool->setPosition(m_touch);
    if (m_tool->sprite) m_tool->sprite->setPosition(m_tool->position);
}

void SurgeryScreen::toolsFrame(float dt) {
    // (0x34470)
    for (ToolState* t : m_toolList) t->cooldownLeft -= dt;
    refreshInjectionBars();
    if (!m_tool) return;
    const bool wasActive = m_tool->active;
    m_tool->update(dt);
    if (m_tool->active) {
        placeTool();
        m_tool->frame(dt);
        // of no use this frame: its damage (not in the Reverend's power;
        // the original also spared the tutorial's steps 10 and 7; nor with
        // cheat 10 or 7, 1.45 runToolLogic 0x41e34: Cheats.h)
        if (!m_tool->usefulThisFrame && m_reverendTimer == 0 && !cheat(Cheats::kReverendForever) &&
            !cheat(Cheats::kNoMisuseDamage) && m_tool->damage > 0)
            damage(m_tool->damage);
        m_tool->usefulThisFrame = false;
    }
    // put down by itself (the tazer after its zap, the chainsaw getting
    // away, the injection done): the injuries are told, and its effect
    // (0x34cbc)
    if (wasActive && !m_tool->active) {
        std::vector<InjuryState*> list = run().injuries;
        for (InjuryState* i : list)
            if (!i->finished && !i->hasDependencies) i->toolStopped(m_tool->type);
        m_effects.toolStopped();
    }
}

bool SurgeryScreen::toolBeforeHitTest() {
    // (0x39624's start)
    if (!m_tool || !m_tool->active) return true;
    switch (m_tool->type) {
    case kScanner:
        scan();
        return true;
    case kClamp:
        return !static_cast<ClampsController*>(m_tool)->touchClamps(m_touch);
    case kInjection:
        // in: tested over the injuries' areas only
        if (static_cast<Injection*>(m_tool)->mode == 1) return inGraphicsRect(m_tool->position, true);
        return true;
    default:
        return true;
    }
}

void SurgeryScreen::toolHit(const HitResult& r) {
    // (0x39624) a clamp point double tapped: a clamp on it; the injection's
    // fluid takes the colour of what it extracts (0x398e4)
    if (m_tool->type == kClamp) {
        ClampsController* c = static_cast<ClampsController*>(m_tool);
        c->place(r);
        c->target = r.injury;
    } else if (m_tool->type == kInjection) {
        Injection* inj = static_cast<Injection*>(m_tool);
        if (inj->mode == 0 && r.injury) inj->setFluidColour(Injection::colourForClass(r.injury->def->classIndex));
    }
}

void SurgeryScreen::scan() {
    // (0x39104) the stage's hidden injuries by their squared distance from
    // the touch; every interval the effect shows: a black blip at the touch
    // if none, a green one on each with the global upgrade, else one at the
    // touch coloured by the nearest's distance (and the arrow upgrade's
    // arrow turned to it), with its sound
    Scanner* sc = static_cast<Scanner*>(m_tool);
    ScannerEffect* fx = dynamic_cast<ScannerEffect*>(m_effects.currentToolEffect);
    std::vector<std::pair<InjuryState*, float>> found;
    for (InjuryState* i : run().injuries) {
        if (!i->hasStage() || i->stage().state != kStateHidden) continue;
        float dx = m_touch.x - i->position.x, dy = m_touch.y - i->position.y;
        found.push_back({i, dx * dx + dy * dy});
    }
    InjuryState* nearest = nullptr;
    float nearestSq = -1;
    for (const auto& f : found)
        if (f.second < nearestSq || nearestSq == -1) {
            nearest = f.first;
            nearestSq = f.second;
        }
    if (fx && fx->tick()) {
        fx->clear();
        const char* sound;
        if (found.empty()) {
            fx->addBlip(m_touch, ScannerEffect::kBlack);
            sound = "radar_blip_negative";
            sc->removeArrow();
        } else if (sc->upgradeLevel == 3) {
            for (const auto& f : found) fx->addBlip(f.first->position, ScannerEffect::kGreen);
            sound = "radar_blip_positive";
        } else {
            fx->addBlip(m_touch, sc->colourFor(nearestSq));
            if (sc->upgradeLevel == 2) sc->pointArrow(m_touch, nearest->position);
            sound = sc->soundFor(nearestSq);
        }
        SoundMgr::sharedSoundMgr()->playSound(sound, false);
        fx->restart();
        log("scanner %s found %d", sound, static_cast<int>(found.size()));  // (for test scripts)
    } else if (sc->upgradeLevel == 2 && nearest) {
        sc->pointArrow(m_touch, nearest->position);
    }
}

// ---- the injection's HUD ----

void SurgeryScreen::refreshInjectionBars() {
    // (0x34580; not while Brutality's power has the tools away) cooling
    // down, the green bar shows the time left
    if (m_brutalityTimer > 0) return;
    auto it = m_tools.find(kInjection);
    if (it == m_tools.end()) return;
    ToolState* inj = it->second;
    CCNode* button = ui.node("ingame_tool_Injection");
    if (inj->cooldownLeft <= 0) {
        ui.setVisible("ingame_injection_bar_grey", false);
        ui.setVisible("ingame_injection_bar_green", false);
        if (CCMenuItem* item = dynamic_cast<CCMenuItem*>(button)) item->setEnabled(true);
    } else {
        ui.setVisible("ingame_injection_bar_grey", true);
        ui.setVisible("ingame_injection_bar_green", true);
        if (CCNode* green = ui.node("ingame_injection_bar_green"))
            green->setScaleX((inj->cooldown > 0 ? inj->cooldownLeft / inj->cooldown : 0) *
                             (button ? button->getScale() : 1));  // (the button's scale: SurgeryHud.cpp)
    }
    // (the injection's button moved aside, the menu shut: no bars)
    if (m_injectionSecond && m_injectionSecond == button && !m_injectionMenu) {
        ui.setVisible("ingame_injection_bar_grey", false);
        ui.setVisible("ingame_injection_bar_green", false);
    }
}

void SurgeryScreen::showInjectionMenu() {
    // (0x3e1b4; not while the tutorial holds the tools) both buttons and the
    // graph; the in button only once cooled down
    if (!m_injectionFirst) {
        m_injectionFirst = ui.node("ingame_tool_Injection");
        m_injectionSecond = ui.node("ingame_tool_Injection_out");
    }
    ui.setVisible("ingame_tool_Injection", true);
    ui.setVisible("ingame_tool_Injection_out", true);
    ui.setVisible("ingame_injection_graph", true);
    auto it = m_tools.find(kInjection);
    if (CCMenuItem* item = dynamic_cast<CCMenuItem*>(ui.node("ingame_tool_Injection")))
        item->setEnabled(it == m_tools.end() || it->second->cooldownLeft <= 0);
    m_injectionMenu = true;
    if (m_injectionSecond) {  // (for test scripts: the button beside, cocos points)
        CCRect bb = m_injectionSecond->boundingBox();
        log("injection menu second %s %.0f,%.0f", ui.nameOf(m_hudDef, m_injectionSecond).c_str(), bb.getMidX(),
            bb.getMidY());
    }
}

void SurgeryScreen::hideInjectionMenu() {
    // (0x33fa4) the button beside and the graph
    if (!m_injectionFirst) {
        m_injectionFirst = ui.node("ingame_tool_Injection");
        m_injectionSecond = ui.node("ingame_tool_Injection_out");
    }
    if (m_injectionSecond) m_injectionSecond->setVisible(false);
    ui.setVisible("ingame_injection_graph", false);
    if (m_injectionMenu) log("injection menu hidden");  // (for test scripts)
    m_injectionMenu = false;
}

void SurgeryScreen::swapInjectionButtons() {
    // (0x3e4e4) the two change places; the one now nearer the edge is the
    // first
    CCNode* a = m_injectionFirst;
    CCNode* b = m_injectionSecond;
    if (!a || !b) return;
    CCPoint pa = a->getPosition(), pb = b->getPosition();
    a->setPosition(pb);
    b->setPosition(pa);
    CCNode* in = ui.node("ingame_tool_Injection");
    CCNode* out = ui.node("ingame_tool_Injection_out");
    // (the port's bar across the top, SurgeryHud.cpp: the one at the top)
    bool inFirst = m_topBar ? out->getPositionY() < in->getPositionY() : out->getPositionX() < in->getPositionX();
    m_injectionFirst = inFirst ? in : out;
    m_injectionSecond = inFirst ? out : in;
    placeInjectionBars();
}

void SurgeryScreen::placeInjectionBars() {
    // (0x3e768) on the in button, wherever it is
    CCNode* b = ui.node("ingame_tool_Injection");
    if (!b) return;
    // (scaled with the button: the port's bar across the top, SurgeryHud.cpp)
    const float k = b->getScaleX(), ky = b->getScaleY();
    const CCSize sz = CCSizeMake(b->getContentSize().width * k, b->getContentSize().height * ky);
    if (CCNode* g = ui.node("ingame_injection_bar_grey")) {
        g->setScaleX(k);
        g->setScaleY(ky);
        g->setPosition(ccp(b->getPositionX() + sz.width * 0.05f, b->getPositionY() + sz.height * 0.1f));
    }
    if (CCNode* g = ui.node("ingame_injection_bar_green")) {
        g->setScaleY(ky);
        g->setPosition(ccp(b->getPositionX() + sz.width * 0.105f, b->getPositionY() + sz.height * 0.115f));
    }
}

bool SurgeryScreen::injectionButton(const std::string& name, CCObject* sender) {
    // (0x30d9c's injection part) the injection's buttons: the tool, in or
    // out by the button; pressed in the injection's place the menu opens,
    // pressed beside, the mode chosen moves into its place; the menu shuts
    // once one is chosen
    if (name != "ingame_tool_Injection" && name != "ingame_tool_Injection_out") return false;
    if (!m_injectionFirst) {
        m_injectionFirst = ui.node("ingame_tool_Injection");
        m_injectionSecond = ui.node("ingame_tool_Injection_out");
    }
    const bool menuWasOpen = m_injectionMenu;
    // (the out button is on with the injection's: 0x3a2b0 gives it the
    // injection's tool)
    const bool enabled = m_enabledItems["ingame_tool_Injection"] && !m_toolsFrozen;
    bool swapped = false;
    if (enabled) {
        // (the original played "tool_selected" here and again below)
        selectTool(kInjection);
        if (CCMenuItemToggle* t = toggle(ui.node("ingame_tool_Injection"))) t->setSelectedIndex(1);
        auto it = m_tools.find(kInjection);
        if (it != m_tools.end()) static_cast<Injection*>(it->second)->mode = name != "ingame_tool_Injection_out";
        if (sender == m_injectionSecond) {
            swapInjectionButtons();
            swapped = true;
        }
        // (the tool put away while the menu is up)
        if (m_tool) m_tool->stop();
        if (m_trail) m_trail->setVisible(false);
        showInjectionMenu();
    }
    if (swapped) hideInjectionMenu();
    // only the chosen toggle on (0x39090), the turned-off ones left off
    for (int i = 0; i < kToolCount; ++i)
        if (CCMenuItemToggle* tg = toggle(ui.node(std::string("ingame_tool_") + kToolNames[i])))
            if (tg->getSelectedIndex() != 2) tg->setSelectedIndex(0);
    if (CCMenuItemToggle* tg = toggle(ui.node("ingame_tool_Injection_out")))
        if (tg->getSelectedIndex() != 2) tg->setSelectedIndex(0);
    if (CCMenuItemToggle* s = toggle(dynamic_cast<CCNode*>(sender))) {
        if (enabled) {
            SoundMgr::sharedSoundMgr()->playSound("tool_selected", false);
            s->setSelectedIndex(1);
        } else {
            SoundMgr::sharedSoundMgr()->playSound("button_click_denied", false);
            s->setSelectedIndex(2);
        }
    }
    if (menuWasOpen) hideInjectionMenu();
    return true;
}

// ---- frozen tools (event 17) ----

void SurgeryScreen::deselectTool() {
    // (0x33ea4) the tool's effect ends, the tongs' tray goes, the tool is let
    // go of and put down (it stays the one selected)
    m_effects.toolTouchEnded(m_touch);
    if (!m_tool) return;
    if (m_tool->type == kTongs) {
        static_cast<Tongs*>(m_tool)->holdingPiece = false;
        if (m_trayShown) hideTray();
    }
    if (m_tool->active) touchEnded();
    else if (m_trail) m_trail->setVisible(false);
    m_tool->end(m_touch);
    m_tool->stop();
}

void SurgeryScreen::freezeTools(float seconds) {
    // (0x33f68) every tool's button off for a while (not the tag partner's)
    m_toolsFrozen = true;
    m_frozenTimer = seconds;
    log("tools frozen %.1f s", seconds);  // (for test scripts)
    if (!m_tool) return;
    deselectTool();
    if (m_injectionMenu) hideInjectionMenu();
    for (int i = 0; i < kToolCount; ++i) {
        std::string n = std::string("ingame_tool_") + kToolNames[i];
        if (CCMenuItemToggle* tg = toggle(ui.node(n))) {
            tg->setSelectedIndex(2);
            m_enabledItems[n] = false;
        }
    }
    if (CCMenuItemToggle* tg = toggle(ui.node("ingame_tool_Injection_out"))) {
        tg->setSelectedIndex(2);
        m_enabledItems["ingame_tool_Injection_out"] = false;
    }
}

void SurgeryScreen::unfreezeTools() {
    // (0x3a2b0) the buttons of the tools the surgery has on again
    m_toolsFrozen = false;
    log("tools unfrozen");  // (for test scripts)
    auto set = [&](const std::string& n, int tool) {
        if (CCMenuItemToggle* tg = toggle(ui.node(n))) {
            // (the original's tools list holds the surgery's own: +0x900)
            bool has = std::find(m_def.tools.begin(), m_def.tools.end(), tool) != m_def.tools.end();
            tg->setSelectedIndex(has ? 0 : 2);
            m_enabledItems[n] = has;
        }
    };
    for (int i = 0; i < kToolCount; ++i) set(std::string("ingame_tool_") + kToolNames[i], i);
    set("ingame_tool_Injection_out", kInjection);
}

// (the port's) A finger is wider than a pointer: on a touch device a touch
// the tongs begin just off something they take (a piece, a creature, a
// point of theirs to work) begins on it. On the touch's first frame of work,
// a miss tries the tools' own hit test again round the touch, nearest
// first, out to kTongsReach canvas points; the first hit is where the touch
// is, and the rest of the touch is moved by as much (a pull, a carry to the
// tray). (AS_TOUCH_REACH=1, debug mode: on the PC too, for the play-tests.)
bool SurgeryScreen::tongsReachWanted() const {
    static const bool forced = platform::debugMode() && std::getenv("AS_TOUCH_REACH");
    return (platform::touchDevice() || forced) && m_tool && m_tool->type == kTongs &&
           !static_cast<Tongs*>(m_tool)->holdingPiece && !m_tool->target;
}

HitResult SurgeryScreen::tongsReach() {
    constexpr float kTongsReach = 16, kRings = 3;
    constexpr int kAround = 12;
    const float c = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    const CCPoint at = m_touch;
    HitResult r;
    m_probing = true;
    for (int ring = 1; ring <= kRings && !r.hit; ++ring) {
        const float d = kTongsReach * c * static_cast<float>(ring) / kRings;
        for (int k = 0; k < kAround; ++k) {
            const float a = 6.2831853f * (static_cast<float>(k) + (ring % 2 ? 0.0f : 0.5f)) / kAround;
            const CCPoint off = ccp(d * std::cos(a), d * std::sin(a));
            r = hitInjuries(ccpAdd(at, off));
            if (r.hit) {
                m_touchShift = off;
                m_touch = ccpAdd(at, off);
                log("tongs reach %.0f,%.0f", off.x, off.y);
                break;
            }
        }
    }
    m_probing = false;
    return r;
}

}  // namespace mt
