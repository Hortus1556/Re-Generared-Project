#include "as3/ClampsController.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>

#include "as3/Log.h"
#include "platform/Platform.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/StatsMgr.h"
#include "as3/Tools2.h"

namespace mt {

using namespace cocos2d;

namespace {

double now() {
    // (0xaa048: CFAbsoluteTimeGetCurrent; the port reads the pass clock,
    // as touches come once a pass: it also holds in fast-forwarded test runs)
    return platform::passTime();
}

CCSprite* spriteNamed(const char* name) {
    CCSprite* s = CCSprite::createWithSpriteFrameName(name);
    return s ? s : CCSprite::create();
}

}  // namespace

ClampsController::ClampsController(int max) : maxClamps(max) {
    power = 0;
    damage = 0;
    target = nullptr;
    zOrder = 7;
    // (its node: the clamps, on the screen's own coordinates)
    sprite = CCSprite::create();
    sprite->retain();
}

ClampsController::~ClampsController() {
    for (Clamp* c : clamps) delete c;
}

bool ClampsController::doubleTapped() const {
    return m_taps.size() >= 2 && m_taps[1] - m_taps[0] < doubleTap;
}

void ClampsController::begin(const CCPoint& touch) {
    // (vf2) the base's; the press's time kept, of the last two
    ToolState::begin(touch);
    m_taps.push_back(now());
    if (m_taps.size() > 2) m_taps.erase(m_taps.begin());
    m_tapUsed = m_taps.size() < 2;
}

void ClampsController::end(const CCPoint&) {
    stop();
    m_lastAngle = -1;
    m_tapUsed = m_taps.size() < 2;
}

int ClampsController::angleOf(Clamp* c, const CCPoint& touch) {
    // the wheel's angle from the finger, in cocos' degrees
    CCPoint local = c->node->convertToNodeSpace(touch);
    float a = std::atan2(c->wheel->getPositionY() - local.y, c->wheel->getPositionX() - local.x);
    return static_cast<int>(a * -57.29577951308232f + 90.0f);
}

bool ClampsController::onWheel(Clamp* c, const CCPoint& touch) {
    // (1.45 0x8a4ac) within 1.9 of the wheel's size about it (1.02's
    // 0x6875c: 1.25, a smaller wheel to hit)
    const CCSize sz = c->wheel->getContentSize();
    CCRect r = CCRectMake(c->wheel->getPositionX() - sz.width * 1.9f, c->wheel->getPositionY() - sz.height * 1.9f,
                          sz.width * 3.8f, sz.height * 3.8f);
    return r.containsPoint(c->node->convertToNodeSpace(touch));
}

Clamp* ClampsController::clampAt(const CCPoint& touch) {
    // (0x69048) within 1.15 of its picture's size about it, or on its wheel
    for (Clamp* c : clamps) {
        CCSize sz = c->anim->getSpriteSize();
        CCRect r = CCRectMake(c->node->getPositionX() - sz.width * 1.15f, c->node->getPositionY() - sz.height * 1.15f,
                              sz.width * 2.3f, sz.height * 2.3f);
        if (r.containsPoint(touch)) return c;
        if (onWheel(c, touch)) return c;
    }
    return nullptr;
}

bool ClampsController::touchClamps(const CCPoint& touch) {
    // (0x68ec0)
    Clamp* c = clampAt(touch);
    if (!c) return false;
    if (c->hintShown) {
        c->arrow->setVisible(false);
        c->hand->setVisible(false);
        c->hintShown = false;
    }
    c->hintTimer = 0;
    target = nullptr;
    bool wheel = onWheel(c, touch);
    if (doubleTapped() && m_current == c) {
        // a double tap on it: off (not while its wheel turns)
        if (!m_tapUsed) {
            m_tapUsed = true;
            if (m_lastAngle == -1) {
                if (c->injury) c->injury->removeClamp(c);
                target = nullptr;
                releaseClamp(c);
                m_current = nullptr;
                m_taps.clear();
                m_tapUsed = true;
                stop();
            }
        }
        return true;
    }
    m_current = c;
    if (wheel) m_lastAngle = angleOf(c, touch);
    return true;
}

void ClampsController::place(const HitResult& r) {
    // (0x694cc) the second tap of a double tap on the same injury's clamp
    // point: a clamp there, turned as the point
    if (m_tapUsed) return;
    if (doubleTapped() && count() < maxClamps && r.injury == target && m_lastAngle == -1 && r.hitPoint) {
        // the popup (tag popup kind 0 the first time on this point, else 5)
        InjuryEvent pop;
        pop.type = InjuryEvent::kTagPopup;
        pop.kind = r.hitPoint->clampTagged ? 5 : 0;
        pop.x = std::floor(position.x);
        pop.y = std::floor(position.y);
        post(pop);
        r.hitPoint->clampTagged = true;
        int rot = (r.hitPoint->def ? r.hitPoint->def->angle : 0) + static_cast<int>(r.injury->rotation);
        Clamp* c = makeClamp(r.hitPoint->position, rot);
        c->point = r.hitPoint;
        r.hitPoint->clamp = c;
        log("clamp placed at %.0f,%.0f wheel %.0f,%.0f", r.hitPoint->position.x, r.hitPoint->position.y,
            c->node->convertToWorldSpace(c->wheel->getPosition()).x,
            c->node->convertToWorldSpace(c->wheel->getPosition()).y);  // (for test scripts)
        // (1.45 checkClampCreation 0x8b224: past 3 clamps on, stat 18, StatsMgr.h)
        if (count() > 3) StatsMgr::sharedInstance()->incrementStat(StatTypes::kUse4ClampsAtOnce);
        target = nullptr;
        m_current = nullptr;
        m_taps.clear();
        m_tapUsed = false;
        stop();
    }
    m_tapUsed = true;
}

Clamp* ClampsController::makeClamp(const CCPoint& at, int rotation) {
    // (0x69aa8)
    SoundMgr::sharedSoundMgr()->playSound("clamp_placement", false);
    Clamp* c = new Clamp;
    c->boltTravel = 30.0f * ResolutionMgr::sharedResolutionMgr()->canvasScale;
    c->node = CCNode::create();
    c->node->setAnchorPoint(ccp(0.5f, 0.5f));
    c->anim = Animation::create("common/anim_tool_clamp_anim.json", 1);
    c->anim->play("close", -1, false);
    for (int i = 0; i < 2; ++i) {
        c->bolts[i] = spriteNamed("tool_clampbolt.png");
        c->node->addChild(c->bolts[i]);
        c->bolts[i]->setPosition(ccp(c->bolts[i]->getContentSize().width * (i == 0 ? -1.2f : 1.2f), 0));
        c->bolts[i]->setRotation(i == 0 ? 180.0f : 0.0f);
        c->boltX[i] = c->bolts[i]->getPositionX();
    }
    // (the original also made two cogs it never showed)
    c->cogs[2] = spriteNamed("tool_clampcog.png");
    c->node->addChild(c->cogs[2]);
    c->cogs[2]->setPositionY(c->cogs[2]->getContentSize().height * -0.5f);
    c->cogs[2]->setScale(1.7f);
    c->node->addChild(c->anim);
    c->wheel = spriteNamed("tool_clampwheel.png");
    c->node->addChild(c->wheel);
    c->cogs[2]->setRotation(-c->wheel->getRotation());
    clamps.push_back(c);
    c->injury = target;
    if (target) target->clamps.push_back(c);
    c->anim->showFrame("close", static_cast<int>(c->tightness / c->maxTightness * static_cast<float>(frameCount)));
    c->node->setPosition(at);
    sprite->addChild(c->node);
    c->node->setAnchorPoint(ccp(0.5f, 0.5f));
    c->node->setRotation(static_cast<float>(rotation));
    c->arrow = spriteNamed("tool_clamparrow.png");
    c->hand = spriteNamed("tool_clamphand.png");
    c->hand->setRotation(-c->node->getRotation());
    c->wheel->setRotation(-c->node->getRotation());
    c->hand->setPosition(ccp(0, static_cast<float>(c->hand->getContentSize().height / 4.7)));
    c->node->addChild(c->arrow);
    c->node->addChild(c->hand);
    c->hintShown = false;
    c->arrow->setVisible(false);
    c->hand->setVisible(false);
    return c;
}

void ClampsController::unclampPoint(Clamp* c) {
    // its hit point open and whole again
    if (HitPointState* p = c->point) {
        p->done = false;
        p->clamp = nullptr;
        p->health = 1;
        if (c->injury)
            for (HitAreaState& a : c->injury->areas)
                for (HitPointState& q : a.points)
                    if (&q == p) a.done = false;
    }
    c->point = nullptr;
}

void ClampsController::releaseClamp(Clamp* c) {
    // (0x69150) off: the point open again, the stage looked at again
    // (event 13)
    SoundMgr::sharedSoundMgr()->playSound("clamp_release", false);
    log("clamp released");  // (for test scripts)
    clamps.erase(std::remove(clamps.begin(), clamps.end(), c), clamps.end());
    unclampPoint(c);
    // (the original emptied its node and left it: taken away here)
    c->node->removeFromParentAndCleanup(true);
    InjuryEvent e;
    e.type = InjuryEvent::kEvent13;
    post(e);
    delete c;
}

void ClampsController::releaseAll() {
    // (0x6a290) every clamp off its point and its injury (the original
    // forgot the records: freed here)
    for (Clamp* c : clamps) {
        unclampPoint(c);
        if (c->injury) c->injury->removeClamp(c);
        c->node->removeFromParentAndCleanup(true);
        delete c;
    }
    clamps.clear();
    m_current = nullptr;
}

void ClampsController::releaseFor(InjuryState* injury) {
    // (0x6a2f8)
    SoundMgr::sharedSoundMgr()->playSound("clamp_release", false);
    for (size_t i = 0; i < clamps.size();) {
        Clamp* c = clamps[i];
        if (c->injury != injury) {
            ++i;
            continue;
        }
        unclampPoint(c);
        c->node->removeFromParentAndCleanup(true);
        InjuryEvent e;
        e.type = InjuryEvent::kEvent13;
        post(e);
        if (m_current == c) m_current = nullptr;
        delete c;
        clamps.erase(clamps.begin() + static_cast<long>(i));
    }
}

void ClampsController::moved(const CCPoint& touch) {
    // (vf3) the wheel turned with the finger: each degree tightens it by the
    // rate (a turn back loosens it); off the wheel, or tight, it lets go
    if (m_lastAngle == -1 || !m_current) return;
    Clamp* c = m_current;
    if (!onWheel(c, touch) || !(c->tightness < c->maxTightness)) {
        stop();
        m_current = nullptr;
        m_lastAngle = -1;
        m_tapUsed = true;
        return;
    }
    CCPoint local = c->node->convertToNodeSpace(touch);
    float a = std::atan2(c->wheel->getPositionY() - local.y, c->wheel->getPositionX() - local.x);
    float angle = a * -57.29577951308232f + 90.0f;
    int x = static_cast<int>(angle - static_cast<float>(m_lastAngle) + 180.0f);
    float delta = static_cast<float>(x % 360 - 180);
    if (180.0f < std::fabs(delta)) delta = 0;
    float wheelRot = c->wheel->getRotation();
    float tight = c->tightness + delta * rate;
    HitPointState* p = c->point;
    if (p) {
        p->done = false;
        p->health = 1;
    }
    float to = 1;
    if (!(tight < 1)) {
        to = c->maxTightness;
        if (c->maxTightness <= tight) {
            if (c->tightness != c->maxTightness) log("clamp tightened");  // (for test scripts)
            if (p) {
                p->done = true;
                p->health = 0;
            }
        } else {
            c->wheel->setRotation(delta + wheelRot);
            c->cogs[2]->setRotation(-(delta + wheelRot));
            to = tight;
        }
    }
    if (c->tightness != to) {
        if (to == c->maxTightness)
            SoundMgr::sharedSoundMgr()->playOnChannel("clamp_fully_tightened", channel(), false, true, false, false);
        else
            SoundMgr::sharedSoundMgr()->playOnChannel("clamp_tighten_loop", channel(), false, false, false, false);
    }
    float old = c->tightness;
    c->tightness = to;
    // (the picture and the bolts by the tightness it had)
    float ratio = old / c->maxTightness;
    c->anim->showFrame("close", static_cast<int>(ratio * static_cast<float>(frameCount)));
    c->bolts[0]->setPositionX(c->boltX[0] + ratio * c->boltTravel);
    c->bolts[1]->setPositionX(c->boltX[1] - ratio * c->boltTravel);
    m_lastAngle = static_cast<int>(angle);
}

void ClampsController::update(float dt) {
    // (vf5, each frame) a loose clamp shows its hint after a while: the
    // arrow turning, the hand going round
    for (Clamp* c : clamps) {
        if (!c->hintShown) {
            if (c->tightness < c->maxTightness) {
                c->hintTimer += dt;
                if (c->hintDelay <= c->hintTimer) {
                    c->hintShown = true;
                    c->arrow->setVisible(true);
                    c->hand->setVisible(true);
                }
            }
        } else {
            c->arrow->setRotation(static_cast<float>(static_cast<int>(c->arrow->getRotation() + c->hintSpeed)));
            float a = static_cast<float>(c->handAngle * 0.017453292519943295);
            CCPoint hp = c->hand->getPosition();
            c->hand->setPosition(ccp(hp.x + c->handRadius * std::cos(a), hp.y + c->handRadius * std::sin(a)));
            c->handAngle -= static_cast<float>(c->hintSpeed);
        }
    }
}

void ClampsController::stop() {
    // (vf10) the hints hidden; its taps and target kept
    if (active) SoundMgr::sharedSoundMgr()->stopChannel(channel(), true);
    m_damageFrame = 0;
    active = false;
    m_lastAngle = -1;
    for (Clamp* c : clamps) {
        c->hintShown = false;
        c->arrow->setVisible(false);
        c->hand->setVisible(false);
    }
}

void ClampsController::misuse(const CCPoint&) {
    if (toolMisuseDue(this)) postToolDamageEffect(this);
    stop();
}

void ClampsController::setEfficiency(float e) {
    damage *= (1.0f - e) + 1.0f;
    rate *= e;
}

// ---- the injury's side (InjuryState's clamp virtuals) ----

HitResult InjuryState::clampHitTest(const CCPoint& touch, ToolState* tool) {
    // (vf10 0x84c6c) a clamp point of the current state under the touch,
    // while the tool has clamps to spare and the injury is not clamped
    HitResult r;
    if (tool->type == kClamp) {
        ClampsController* cc = static_cast<ClampsController*>(tool);
        if (cc->count() >= cc->maxClamps || !hasStage() || stage().state == 37) return r;  // 37: Clamped
        for (HitAreaState* a : areasOf(stage().state, true)) {
            if (!a->def->forClamp) continue;
            HitPointState* p = wear(*a, touch, tool->power, false, false);
            if (!r.hit && p) {
                r.hit = true;
                r.injury = this;
                r.hitPoint = p;
            }
        }
    }
    if (!r.hit) tool->m_damageFrame = tool->damage;
    return r;
}

void InjuryState::removeClamp(Clamp* c) {
    // (vf24)
    clamps.erase(std::remove(clamps.begin(), clamps.end(), c), clamps.end());
}

void InjuryState::releaseClamps() {
    // (0x856bc) its clamps forgotten, and every clamp asked off (event 14)
    clamps.clear();
    InjuryEvent e;
    e.type = InjuryEvent::kClamps;
    post(e);
}

}  // namespace mt
