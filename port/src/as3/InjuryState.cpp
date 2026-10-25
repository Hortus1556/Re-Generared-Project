#include "as3/InjuryState.h"

#include <map>

#include <cmath>

#include "as3/Animation.h"
#include "as3/AngledExtraction.h"
#include "as3/Cheats.h"
#include "as3/ClampsController.h"
#include "as3/Creature.h"
#include "as3/InjuryClasses.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/ToolState.h"
#include "as3/UpgradeInfoUtils.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

constexpr float kPi = 3.14159265358979f;

bool ownsUpgrade(const char* id) {
    SavedGame& slot = SaveData::shared().currentSlot();
    return slot.owns(SavedGame::kItemUpgrade, upgradeIndex(id)) || slot.allUpgrades;
}

}  // namespace

InjuryState::InjuryState() {
    static int next = 1000;
    soundChannel = next++;
}

InjuryState::~InjuryState() {
    if (root) {
        root->removeAllChildrenWithCleanup(true);
        root->release();
    }
}

InjuryState* InjuryState::create(const InjuryDef* def) {
    // (0x908c4: each base class's subclass, with its tuning; the subclasses
    // are being ported, the base runs the rest meanwhile)
    // (the creatures' cases: creature::make, Creature.cpp)
    // (the other injuries' cases: injuries::make, InjuryClasses.cpp)
    InjuryState* s = creature::make(def->base);
    if (!s) s = injuries::make(def->base);
    if (!s) s = def->base == kAngledExtraction ? new AngledExtraction : new InjuryState;
    switch (def->base) {
    case kFlea:
        s->zOrder = 100;
        break;
    default:
        break;
    }
    s->def = def;
    return s;
}

void InjuryState::setup(const CCPoint& p, float degrees, bool later) {
    // (0x84e08)
    finished = false;
    stageFound = false;
    m_stage = 1;
    damageNow = damageTimer = particleTimer = eventTimer = 0;
    rotation = degrees;
    position = p;
    // its rect: the def's size about its position (0x84e44; the size has the
    // small screens' 1.25 already)
    rect = CCRectMake(p.x - def->size.width * 0.5f, p.y - def->size.height * 0.5f, def->size.width,
                      def->size.height);
    placeHitPoints();
    hasDependencies = false;
    toSurgeryStage = 0;
    stageEndToSwap = static_cast<int>(def->stages.size());
    rebuildHitAreas();
    hasDependencies = false;
    // (0x75ee0: +0xfd97 set before vf2. The port first set createdLater
    // here, +0xfda4, which only a tool's events set: SurgeryScreen::createInjury)
    if (later) sendsGraphicsEvent = true;
    start();
}

std::string InjuryState::frameName() const {
    return "injury-" + def->name + "-" + (hasStage() ? stage().stateName : std::string()) + ".png";
}

void InjuryState::post(const InjuryEvent& e) {
    if (events.size() < 15) events.push_back(e);
}

std::vector<HitAreaState*> InjuryState::areasOf(int s, bool withClamp) {
    std::vector<HitAreaState*> out;
    for (HitAreaState& a : areas)
        if (a.def->state == s && (!a.def->forClamp || withClamp)) out.push_back(&a);
    return out;
}

void InjuryState::traceIfWorn() const {
    static std::map<const InjuryState*, size_t> left;
    // (for test scripts, as a touch ends: the points left, against the last
    // count, or all of them as its stage showed)
    size_t n = 0, all = 0;
    for (const HitAreaState& a : areas)
        for (const HitPointState& p : a.points) {
            n += p.done ? 0 : 1;
            ++all;
        }
    auto it = left.find(this);
    const size_t before = it != left.end() ? it->second : all;
    left[this] = n;
    if (n != before && n != 0) traceStage();
}

void InjuryState::traceStage() const {
    if (!hasStage()) return;
    std::string tools, points;
    for (int t : stage().tools) tools += std::string(tools.empty() ? "" : ",") + (t < kToolCount ? kToolNames[t] : "?");
    for (const HitAreaState& a : areas)
        for (const HitPointState& p : a.points)
            if (!p.done) points += " " + std::to_string(static_cast<int>(p.position.x)) + "," +
                                   std::to_string(static_cast<int>(p.position.y));
    // (and how fast it hurts, as its blood shows a player: damage a second)
    const float drain = stage().damageTimer > 0 ? stage().damage / stage().damageTimer : 0;
    // (and whether it waits for others to be treated first: no tool works on it)
    log("injury #%d %s state %s tools %s at %.0f,%.0f rot %.0f size %.0f,%.0f points%s drain %.3f%s", soundChannel,
        def->name.c_str(), stage().stateName.c_str(), tools.c_str(), position.x, position.y, rotation,
        def->size.width, def->size.height, points.c_str(), drain, hasDependencies ? " waits" : "");
}

void InjuryState::rebuildHitAreas() {
    // (0x852e4: the hit areas of the current state, every point whole)
    // The original's areas are slots inside the injury, so a clamp's point
    // stays on its slot (area, point) across a rebuild; here they are made
    // anew: the clamps are moved onto the same slots (or off, if the new
    // state has none there), never left on freed points.
    std::vector<std::pair<int, int>> clampSlots;
    for (Clamp* c : clamps) {
        std::pair<int, int> slot(-1, -1);
        for (size_t i = 0; c && c->point && i < areas.size(); ++i)
            for (size_t j = 0; j < areas[i].points.size(); ++j)
                if (&areas[i].points[j] == c->point) slot = {static_cast<int>(i), static_cast<int>(j)};
        clampSlots.push_back(slot);
        if (c) c->point = nullptr;
    }
    areas.clear();
    if (hasStage())
        for (const HitArea& a : def->hitAreas) {
            if (a.state != stage().state) continue;
            HitAreaState st;
            st.def = &a;
            for (const HitPoint& p : a.points) {
                HitPointState ps;
                ps.def = &p;
                ps.health = 1;
                st.points.push_back(ps);
            }
            areas.push_back(st);
        }
    for (size_t k = 0; k < clamps.size(); ++k) {
        const int i = clampSlots[k].first, j = clampSlots[k].second;
        if (!clamps[k] || i < 0 || i >= static_cast<int>(areas.size()) || j >= static_cast<int>(areas[i].points.size()))
            continue;
        clamps[k]->point = &areas[i].points[j];
        areas[i].points[j].clamp = clamps[k];
    }
    if (!hasStage()) return;
    placeHitPoints();
}

void InjuryState::placeHitPoints() {
    // (0x84ec0) a point is given on the canvas about the injury's place and
    // turned with it; its touch rect is its radius around it
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float a = -rotation * kPi / 180.0f;
    const float c = std::cos(a), sn = std::sin(a);
    for (HitAreaState& area : areas)
        for (HitPointState& p : area.points) {
            float dx = p.def->x * s, dy = p.def->y * s;
            float x = position.x + dx * c - dy * sn;
            float y = position.y + dx * sn + dy * c;
            p.position = ccp(std::floor(x), std::floor(y));
            float d = p.def->radius * s;
            p.rect = CCRectMake(p.position.x - d * 0.5f, p.position.y - d * 0.5f, d, d);
            if (p.marker) p.marker->setPosition(p.position);
        }
}

void InjuryState::start() {
    // (vf2)
    if (sendsGraphicsEvent) {
        InjuryEvent e;
        e.type = InjuryEvent::kGraphics;
        e.x = std::floor(position.x);
        e.y = std::floor(position.y);
        e.rect = rect;
        e.rotation = std::floor(rotation);
        post(e);
    }
    for (HitAreaState& a : areas)
        for (HitPointState& p : a.points) p.health = pointHealth;
    postCreateAtStart();
    if (hasStage()) state = stage().state;
}

void InjuryState::postCreateAtStart() {
    // (0x83ac0)
    if (!hasStage() || stage().createAtStart.empty()) return;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    InjuryEvent e;
    e.type = InjuryEvent::kCreateInjury;
    e.name = stage().createAtStart;
    e.x = std::floor(position.x + stage().createAtStartOffset.x * s);
    e.y = std::floor(position.y + stage().createAtStartOffset.y * s);
    e.rotation = std::floor(rotation);
    e.countsTowardCap = false;
    e.allowedOverLock = true;
    post(e);
}

void InjuryState::postCreateAtEnd() {
    // (0x836f0) not when the state has clamp areas all clamped shut
    if (!hasStage() || stage().createAtEnd.empty()) return;
    bool clampAreas = false, clampOpen = false;
    for (HitAreaState* a : areasOf(stage().state, true)) {
        if (!a->def->forClamp) continue;
        clampAreas = true;
        for (const HitPointState& p : a->points)
            if (!p.done) clampOpen = true;
    }
    if (clampAreas && !clampOpen) return;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    InjuryEvent e;
    e.type = InjuryEvent::kCreateInjury;
    e.name = stage().createAtEnd;
    e.x = std::floor(position.x + stage().createAtEndOffset.x * s);
    e.y = std::floor(position.y + stage().createAtEndOffset.y * s);
    e.rotation = std::floor(rotation);
    e.countsTowardCap = false;
    e.allowedOverLock = true;
    post(e);
}

void InjuryState::update(float dt) {
    // (vf5) the current stage's timers: its damage every damage_timer
    m_flag48 = true;
    damageNow = 0;
    if (!hasStage()) return;
    const InjuryStageDef& st = stage();
    particleTimer += dt;
    if (st.particleTimer <= particleTimer) particleTimer = 0;
    eventTimer += dt;
    if (st.eventTimer <= eventTimer) eventTimer = 0;
    damageTimer += dt;
    if (st.damageTimer <= damageTimer) {
        damageTimer = 0;
        damageNow = st.damage;
    }
}

void InjuryState::finishStage() {
    // (vf6)
    if (m_stage + 1 > static_cast<int>(def->stages.size())) {
        if (hasStage() && stage().state == 8 && perfect) {
            // (vf6 0x82af4) a perfect extraction with the heart rate upgrade:
            // time and health back
            InjuryEvent t;
            t.type = InjuryEvent::kAddTime;
            t.value = 2;
            post(t);
            InjuryEvent h;
            h.type = InjuryEvent::kHeal;
            h.value = 2;
            post(h);
            log("injury #%d perfect extraction", soundChannel);  // (for test scripts)
        }
        postCreateAtEnd();
        m_stage = static_cast<int>(def->stages.size()) + 1;
        finished = true;
        log("injury #%d finished", soundChannel);
        onFinished();
        return;
    }
    // a wound closed or burnt shut heals a little with the upgrade
    if ((stage().state | 2) == 2 && ownsUpgrade("binding_heal_boost_upgrade")) {
        InjuryEvent h;
        h.type = InjuryEvent::kHeal;
        h.value = 1;
        post(h);
    }
    // a piece cut free gives time with the chainsaw's upgrade
    if (def->stages[m_stage].state == kFree && ownsUpgrade("cutting_chainsaw_timer_boost")) {
        InjuryEvent t;
        t.type = InjuryEvent::kAddTime;
        t.value = 3;
        post(t);
    }
    goToState(def->stages[m_stage].state, true, true);
    stageFound = true;
}

void InjuryState::backToPreviousStage() {
    // (vf7)
    if (m_stage >= 2) goToState(def->stages[m_stage - 2].state, true, true);
}

void InjuryState::goToState(int s, bool startEvents, bool endEvents) {
    // (0x8364c)
    if (endEvents) postCreateAtEnd();
    if (markersNode) markersNode->removeAllChildrenWithCleanup(true);
    for (HitAreaState& a : areas)
        for (HitPointState& p : a.points) p.marker = nullptr;
    int found = def->stageOf(s);
    if (found) {
        m_stage = found;
        stageFound = true;
    }
    if (!stageFound) {
        logError("Stage %d not found", s);
        return;
    }
    state = stage().state;
    if (startEvents) postCreateAtStart();
}

bool InjuryState::takesTool(int tool) const {
    if (!hasStage()) return false;
    for (int t : stage().tools)
        if (t == tool) return true;
    return false;
}

bool InjuryState::pieceContains(const PieceState& piece, const CCPoint& touch) const {
    // (vf13) the touch turned into the injury's frame, in the piece's rect
    // (the original: (90 - (rotation + 90)) times -pi/180: the touch turned back by the injury's turn)
    const float a = rotation * kPi / 180.0f;
    float dx = touch.x - position.x, dy = touch.y - position.y;
    CCPoint local = ccp(position.x + dx * std::cos(a) - dy * std::sin(a),
                        position.y + dx * std::sin(a) + dy * std::cos(a));
    return piece.rect.containsPoint(local);
}

HitPointState* InjuryState::wear(HitAreaState& area, const CCPoint& touch, float amount, bool evenIfDone,
                                 bool allPoints) {
    // (vf14) the points under the touch lose `amount` of health; at 0 they
    // are done. The stapler (allPoints false) staples one point a touch.
    if ((area.done && !evenIfDone) || area.points.empty()) return nullptr;
    HitPointState* hit = nullptr;
    bool allDone = true;
    // (debug: cheat 0, "hit anywhere": every point is worn wherever the
    // touch is, 0xac26c; Cheats.h)
    const bool anywhere = cheat(Cheats::kHitAnywhere);
    for (size_t i = 0; i < area.points.size(); ++i) {
        HitPointState& p = area.points[i];
        if (!p.rect.containsPoint(touch) && !anywhere) {
            allDone = allDone && p.done;
            continue;
        }
        hit = &p;
        if (evenIfDone) {
            allDone = allDone && p.done;
            continue;
        }
        p.health -= amount;
        if (p.health <= 0) {
            if (p.marker) p.marker->setVisible(false);
            p.done = true;
        }
        allDone = allDone && p.done;
        if (!allPoints && !anywhere) {
            allDone = allDone && i == area.points.size() - 1;
            break;
        }
    }
    if (hit && allDone && !evenIfDone) area.done = true;
    return hit;
}

bool InjuryState::areaDone(const HitAreaState& area) const {
    // (vf15)
    if (area.done) return true;
    for (const HitPointState& p : area.points)
        if (!p.done) return false;
    return true;
}

void InjuryState::resetArea(HitAreaState& area) {
    // (vf16)
    area.done = false;
    for (HitPointState& p : area.points) p.done = false;
}

void InjuryState::completeStage() {
    // (vf18) the current state's areas all done at once (a tutorial's or a
    // tag partner's doing), and on to the next stage
    for (HitAreaState* a : areasOf(state, false)) {
        a->done = true;
        for (HitPointState& p : a->points) {
            if (p.marker) p.marker->setVisible(false);
            p.done = true;
        }
    }
    finishStage();
    if (!finished && hasStage() && stage().needsGraphic && stageFound) postFrameChange();
}

void InjuryState::postFrameChange() {
    // (0x82540) the new state's picture on the first sprite
    InjuryEvent e;
    e.type = InjuryEvent::kSetFrame;
    e.frame = frameName();
    e.sprite = sprites.empty() ? nullptr : sprites[0];
    post(e);
}

HitResult InjuryState::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8)
    HitResult r;
    if (!takesTool(tool->type)) return r;
    if (stage().canBeGrabbed && tool->type == kTongs) {
        if (!pieces.empty() && pieceContains(pieces[0], touch)) {
            r.piece = &pieces[0];
            r.hit = true;
            r.injury = this;
        }
        return r;
    }
    bool anyHit = false;
    HitPointState* point = nullptr;
    std::vector<HitAreaState*> list = areasOf(stage().state, false);
    for (HitAreaState* a : list) {
        HitPointState* p = wear(*a, touch, tool->power, false, tool->type != kStapler);
        if (!r.hit && p) {
            r.hit = true;
            anyHit = true;
            point = p;
            r.hitPoint = p;
        }
    }
    if (stage().useSizeRect) {
        // the injury's rect, turned with it (the touch turned back)
        const float a = rotation * kPi / 180.0f;
        float dx = touch.x - position.x, dy = touch.y - position.y;
        float lx = dx * std::cos(a) - dy * std::sin(a), ly = dx * std::sin(a) + dy * std::cos(a);
        if (std::fabs(lx) <= rect.size.width * 0.5f && std::fabs(ly) <= rect.size.height * 0.5f) r.hit = true;
    }
    if (anyHit || r.hit) {
        if (tool->type == kStapler && point && (def->base == kLargeCut || def->base == kSnakeCut))
            onStaple(touch, tool);
        else
            onTouch(touch);
    }
    bool allDone = true;
    for (HitAreaState* a : list)
        if (!areaDone(*a)) allDone = false;
    if (allDone && r.hit) {
        r.stageDone = true;
        if (!createdLater) r.comboAction = true;
        else r.comboBonus = true;
        finishStage();
        if (!finished && hasStage() && stage().needsGraphic && stageFound) postFrameChange();
    }
    if (r.hit) r.injury = this;
    return r;
}

int InjuryState::toolFor(const CCPoint& touch) {
    // (vf27) the tool the injury wants where touched (kNoTool if none)
    if (!hasStage()) return kNoTool;
    bool touched = false;
    for (HitAreaState* a : areasOf(stage().state, false))
        for (const HitPointState& p : a->points)
            if (p.rect.containsPoint(touch)) touched = true;
    if (stage().useSizeRect && rect.containsPoint(touch)) touched = true;
    return touched && !stage().tools.empty() ? stage().tools[0] : kNoTool;
}

void InjuryState::setRotation(float degrees) {
    // (vf21)
    rotation = degrees;
    for (CCSprite* s : sprites) s->setRotation(degrees);
    for (CCNode* n : animations) n->setRotation(degrees);
    rect = CCRectMake(position.x - def->size.width * 0.5f, position.y - def->size.height * 0.5f, def->size.width,
                      def->size.height);
    placeHitPoints();
    placeNodes();
}

void InjuryState::buildGraphics() {
    if (root) {
        root->removeFromParentAndCleanup(true);
        root->release();
    }
    root = CCNode::create();
    root->retain();
    spritesNode = CCNode::create();
    markersNode = CCNode::create();
    topNode = CCNode::create();
    root->addChild(spritesNode);
    root->addChild(markersNode);
    root->addChild(topNode);
    sprites.clear();
    animations.clear();
    pieces.clear();
    for (const InjuryPieceDef& p : def->pieces) {
        PieceState ps;
        ps.def = &p;
        // its active rect is given from the injury's corner
        ps.rect = CCRectMake(p.activeRect.origin.x + rect.origin.x, p.activeRect.origin.y + rect.origin.y,
                             p.activeRect.size.width, p.activeRect.size.height);
        pieces.push_back(ps);
    }
    if (!hasStage()) return;
    buildPictures();
    // (no markers while it waits for others: checkDependencies shows them)
    if (!hasDependencies) buildMarkers();
    vf3();  // (0x77c4c ends with it)
}

void InjuryState::buildPictures() {
    // (the picture an event asked for is shown whatever the stage says)
    if (stage().needsGraphic || !graphicFrame.empty()) {
        CCSprite* sp = CCSprite::create();
        const std::string fn = graphicFrame.empty() ? frameName() : graphicFrame;
        if (CCSpriteFrame* f = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(fn.c_str()))
            sp->setDisplayFrame(f);
        sp->setPosition(position);
        sp->setRotation(rotation);
        spritesNode->addChild(sp);
        sprites.push_back(sp);
    }
    for (const std::string& name : stage().animationFilenames) {
        Animation* a = Animation::create(("common/" + name + ".json").c_str(), 1);
        if (!a) continue;
        a->setPosition(position);
        a->setRotation(rotation);
        spritesNode->addChild(a);
        animations.push_back(a);
    }
}

void InjuryState::buildMarkers(bool always) {
    // (0x796dc) a dash (hitArea-<colour>.png, surgery_common1) on every point
    // of the areas not yet done (a clamp's areas have none), its colour by the
    // stage's state: the lighter's (Open, Replaced, Shut) black, the cream's
    // (Burnt) green, an incision's cutter (IncisionShut) orange, any other
    // yellow; turned by the point's angle plus the injury's, so the dashes
    // run along the cut. The original only adds: the port clears the node
    // first (it is also called again over markers already there) and keeps
    // a point already worn hidden.
    static_assert(creature::kShut == 64 && creature::kIncisionShut == 81, "the state enum's values");
    markersNode->removeAllChildrenWithCleanup(true);
    if (!hasStage() || (!always && !stage().showHitAreas)) return;
    const char* frame = "hitArea-yellow.png";
    switch (stage().state) {
    case kOpen:
    case kReplaced:
    case creature::kShut: frame = "hitArea-black.png"; break;
    case kBurnt: frame = "hitArea-green.png"; break;
    case creature::kIncisionShut: frame = "hitArea-orange.png"; break;
    default: break;
    }
    CCSpriteFrame* f = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(frame);
    for (HitAreaState& a : areas) {
        if (a.done || a.def->forClamp) continue;
        for (HitPointState& p : a.points) {
            CCSprite* m = f ? CCSprite::createWithSpriteFrame(f) : CCSprite::create();
            m->setPosition(p.position);
            m->setRotation(static_cast<float>(p.def->angle) + rotation);
            m->setVisible(!p.done);
            markersNode->addChild(m);
            p.marker = m;
        }
    }
}

void InjuryState::placeNodes() {
    // (vf30)
    for (CCSprite* s : sprites) s->setPosition(position);
    for (CCNode* n : animations) n->setPosition(position);
    for (HitAreaState& a : areas)
        if (!a.done)
            for (HitPointState& p : a.points)
                if (p.marker) p.marker->setPosition(p.position);
}

}  // namespace mt
