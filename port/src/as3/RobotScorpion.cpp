#include "as3/RobotScorpion.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "as3/Animation.h"
#include "as3/ClampsController.h"
#include "as3/Creature.h"
#include "as3/InjuryKit.h"
#include "as3/Log.h"
#include "as3/ParticleSystem.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"
#include "engine/cocos/CCAction.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

namespace {
constexpr double kDegToRad = 0.017453292519943295;

// an effect event (3) at whole points
InjuryEvent effectAt(int kind, const CCPoint& at) {
    InjuryEvent e;
    e.type = InjuryEvent::kEffect;
    e.effect = kind;
    e.x = static_cast<float>(static_cast<int>(at.x));
    e.y = static_cast<float>(static_cast<int>(at.y));
    return e;
}

// what it does to a particle system it keeps (each tolerating one that
// could not be read)
void stopFX(ParticleSystem* p) {
    if (p) p->stopSystem();
}
void resetFX(ParticleSystem* p) {
    if (p) p->resetSystem();
}
void showFX(ParticleSystem* p, bool on) {
    if (p) p->setVisible(on);
}
}  // namespace

RobotScorpion::RobotScorpion() {
    // (0x994ec)
    zOrder = 100;
    priority = 3;
    sendsGraphicsEvent = false;
    state = kRobotScorpionEmerging;
}

RobotScorpion::~RobotScorpion() {
    // (0x15ef6c) its systems off their parent and let go
    for (ParticleSystem* p : {chargeFX, shardsFX, burrowFX})
        if (p) {
            if (p->getParent()) p->removeFromParentAndCleanup(true);
            p->release();
        }
    if (swoosh) {
        if (swoosh->getParent()) swoosh->removeFromParentAndCleanup(true);
        swoosh->release();
    }
}

void RobotScorpion::showSwoosh() {
    // (the port's) the streaks of the unused "swoosh" (5 frames) at its
    // place and turn as a dash starts
    Animation* a = anim(*this);
    if (!a) return;
    if (!swoosh) {
        swoosh = Animation::create("common/anim_creature_roborpion_anim.json", 1);
        if (!swoosh) return;
        swoosh->retain();
        kit::reparent(spritesNode, swoosh, 1);
    }
    swoosh->setPosition(a->getPosition());
    swoosh->setRotation(a->getRotation());
    swoosh->setVisible(true);
    swoosh->play("swoosh", 1, false);
    log("robotscorpion swoosh");  // (for test scripts)
}

void RobotScorpion::start() {
    // (vf2: not the base's) its distances by the canvas scale; its three
    // systems at its animation's origin
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    margin = marginBase * s;
    emergeHole = emergeHoleBase * s;
    burrowHole = burrowHoleBase * s;
    chargeFX = kit::particles("particle_scorpionChargeFX", CCPointZero);
    burrowFX = kit::particles("particle_scorpionBurrowFX", CCPointZero);
    shardsFX = kit::particles("particle_scorpionShardsFX", CCPointZero);
}

void RobotScorpion::vf3() {
    // (vf3) its timers cleared; its systems placed, stopped; put at a random
    // place, it comes up
    m_flag58 = true;
    holdTimer = 0;
    chargeTimer = 0;
    zapPhase = 0;
    turnTimer = 0;
    hiddenTimer = 0;
    // The original added its three systems to its animation here and
    // stopped them, after its first emerging had started the dirt and the
    // shards; later it took them off the animation (CCNode::removeFromParent,
    // 0x1613f4 and Firing) before starting them again, and every animation
    // played takes its node's children off anyway (0x5a800): they were never
    // seen. The port keeps them beside the animation (under it, at its origin
    // and turn each frame, as its children would have been: placeFX), starts
    // them where the original did (the first emerging's too: stopped before
    // it, not after), and stops them when what they show ends: the dirt and
    // the shards with the emerging (or as the burrowing ends, when the
    // original hid them), the charge with the zap or the tazer.
    for (ParticleSystem* p : {shardsFX, burrowFX, chargeFX}) {
        kit::reparent(spritesNode, p, -1);
        stopFX(p);
    }
    relocate();
    emerge();
    placeFX();
}

void RobotScorpion::placeFX() {
    Animation* a = anim(*this);
    if (!a) return;
    for (ParticleSystem* p : {shardsFX, burrowFX, chargeFX})
        if (p) {
            p->setPosition(a->getPosition());
            p->setRotation(a->getRotation());
        }
}

CCRect RobotScorpion::playArea() const {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float mx = (kCanvasW - static_cast<float>(r->width) / s) * 0.5f;
    const float my = (kCanvasH - static_cast<float>(r->height) / s) * 0.5f;
    return CCRectMake(s * (r->rect.origin.x - mx) + margin, s * (r->rect.origin.y - my) + margin,
                      s * r->rect.size.width - (margin + margin), s * r->rect.size.height - (margin + margin));
}

CCPoint RobotScorpion::randomPlace() const {
    // (0x1617b0)
    const CCRect a = playArea();
    const int x = randInt(static_cast<int>(a.origin.x), static_cast<int>(a.origin.x + a.size.width));
    const int y = randInt(static_cast<int>(a.origin.y), static_cast<int>(a.origin.y + a.size.height));
    return ccp(static_cast<float>(x), static_cast<float>(y));
}

CCPoint RobotScorpion::nextTarget() {
    // (0x1611b0) by the quarter of the screen it is in (the screen's middle,
    // whole points)
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const CCRect a = playArea();
    const float cx = static_cast<float>(r->width / 2), cy = static_cast<float>(r->height / 2);
    Animation* an = anim(*this);
    const CCPoint p = an ? an->getPosition() : position;
    float x0 = a.origin.x, w = a.size.width, y0 = cy, h = a.size.height;
    if (p.x <= cx) {
        if (p.y <= cy) {
            // the bottom left: anywhere in the top half
            h = cy - a.origin.y;
        } else {
            // the top left: the right half
            w = cx - a.origin.x;
            x0 = cx;
            y0 = a.origin.y;
        }
    } else {
        // the right: the bottom left quarter
        w = cx - a.origin.x;
        h = cy - a.origin.y;
        y0 = a.origin.y;
    }
    const int x = randInt(static_cast<int>(x0), static_cast<int>(w + x0));
    const int y = randInt(static_cast<int>(y0), static_cast<int>(h + y0));
    const CCPoint t = ccp(static_cast<float>(x), static_cast<float>(y));
    // its heading (0 up, clockwise) and the turn's time, the short way round
    heading = static_cast<float>(std::atan2(static_cast<double>(t.y - p.y), static_cast<double>(t.x - p.x)) *
                                 -57.29577951308232) +
              90.0f;
    const float rot = an ? an->getRotation() : rotation;
    float d = static_cast<float>(std::abs(static_cast<int>(rot - heading)));
    while (180.0f < d) d += -360.0f;
    turnTimer = static_cast<float>(std::abs(static_cast<int>(d))) / turnSpeed;
    return t;
}

void RobotScorpion::relocate() {
    // (0x15f670)
    home = randomPlace();
    Animation* a = anim(*this);
    if (a) a->setPosition(home);
    target = nextTarget();
    if (a) a->setRotation(heading);
}

void RobotScorpion::emerge() {
    // (0x15f6d0)
    SoundMgr::sharedSoundMgr()->playSound("roboscorpion_reveal", false);
    target = nextTarget();
    Animation* a = anim(*this);
    if (a) a->setRotation(heading);
    postHole(emergeHole);
    if (a) a->play("emerge", 1, false);
    startBurrowFX();
}

void RobotScorpion::startBurrowFX() {
    // (0x1613f4; see vf3 for the port's change: not taken off anything)
    placeFX();
    resetFX(shardsFX);
    resetFX(burrowFX);
    showFX(burrowFX, true);
    showFX(shardsFX, true);
}

void RobotScorpion::postHole(float distance) {
    // (0x16143c) the distance (already by the canvas scale) by the canvas
    // scale again, as the original did; turned as it stands
    Animation* a = anim(*this);
    if (!a) return;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    const double t = (90.0f - a->getRotation()) * kDegToRad;
    const CCPoint p = a->getPosition();
    const float x = static_cast<float>(static_cast<int>(static_cast<float>(p.x + (s * distance) * std::cos(t))));
    const float y = static_cast<float>(static_cast<int>(static_cast<float>(p.y + (s * distance) * std::sin(t))));
    post(createEvent("LargeCutRobot", x, y, static_cast<float>(static_cast<int>(a->getRotation() + 90.0f))));
}

void RobotScorpion::hurtFromUnder() {
    // (0x15f090) the damage effect (the screen's flash: no place) and the scream
    InjuryEvent e;
    e.type = InjuryEvent::kEffect;
    e.effect = 0;
    post(e);
    InjuryEvent s;
    s.type = InjuryEvent::kEvent19;
    post(s);
}

void RobotScorpion::postShard(int n) {
    // (0x161c80)
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    float dx = 0, dy = 0;
    switch (n) {
    case 1: dx = 50, dy = 50; break;
    case 2: dx = -50, dy = 50; break;
    case 3: dx = 50, dy = -50; break;
    case 4: dx = -50, dy = -50; break;
    default: break;
    }
    const int x = static_cast<int>(dx * s + position.x), y = static_cast<int>(dy * s + position.y);
    const int rot = randInt(0, 60) - 30;
    const CCRect a = playArea();
    const float fx = static_cast<float>(x), fy = static_cast<float>(y);
    if (fx < a.origin.x + a.size.width && a.origin.x < fx && fy < a.origin.y + a.size.height && a.origin.y < fy) {
        char name[32];
        std::snprintf(name, sizeof name, "RoborpionShard%d", n);
        post(createEvent(name, fx, fy, static_cast<float>(rot)));
    }
}

Clamp* RobotScorpion::tailClamp() {
    // the clamp on the first clamp point of its Tazered areas (+0x14 of the area)
    std::vector<HitAreaState*> list = areasOf(kRobotScorpionTazered, true);
    if (list.empty() || list[0]->points.empty()) return nullptr;
    return list[0]->points[0].clamp;
}

void RobotScorpion::burrow() {
    Animation* a = anim(*this);
    if (a) {
        a->stopAllActions();
        a->play("burrow", 1, false);
    }
    SoundMgr::sharedSoundMgr()->playSound("roboscorpion_burrow", false);
    startBurrowFX();
    postHole(burrowHole);
    state = kRobotScorpionBurying;
}

void RobotScorpion::vf4(float dt) {
    // (vf4) the injury follows its animation, which its actions move
    tracePosition(*this);
    Animation* a = anim(*this);
    if (!a) return;
    moveTo(*this, a->getPosition());
    placeFX();
    if (swoosh && swoosh->isVisible()) {
        // (the streaks go with it, and away once played)
        swoosh->setPosition(a->getPosition());
        if (!swoosh->isPlaying()) swoosh->setVisible(false);
    }
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    switch (state) {
    case kRobotScorpionEmerging:
        if (!a->isPlaying()) {
            // up: the first dash
            a->runAction(CCSequence::create(CCMoveTo::create(dashTime, target), nullptr));
            a->play("dash", 1, false);
            sound->playSound("roboscorpion_dash", false);
            showSwoosh();
            dashesLeft = static_cast<int>(dashes - 1.0f);
            state = kRobotScorpionDashing;
            // (the port's: the dirt of its coming up ends here)
            stopFX(burrowFX);
            stopFX(shardsFX);
        }
        break;
    case kRobotScorpionDashing:
        if (a->isPlaying()) break;
        if (dashesLeft == 0) {
            // it stops and charges its tail
            a->stopAllActions();
            state = kRobotScorpionFiring;
            a->play("electrocuting_start", 1, false);
            chargeTimer = chargeTime;
            zapPhase = 1;
            dashesLeft = static_cast<int>(dashes - 1.0f);
        } else {
            // it turns towards its next place
            state = kRobotScorpionTurning;
            target = nextTarget();
            a->runAction(CCSequence::create(CCRotateTo::create(turnTimer, heading), nullptr));
            a->play("walk", -1, false);
        }
        break;
    case kRobotScorpionTurning:
        turnTimer -= dt;
        if (turnTimer <= 0) {
            a->stopAllActions();
            --dashesLeft;
            state = kRobotScorpionDashing;
            a->runAction(CCSequence::create(CCMoveTo::create(dashTime, target), nullptr));
            a->play("dash", 1, false);
            sound->playSound("roboscorpion_dash", false);
            showSwoosh();
        }
        break;
    case kRobotScorpionBurying:
        if (!a->isPlaying()) {
            // under the skin, somewhere else
            showFX(shardsFX, false);
            showFX(burrowFX, false);
            stopFX(shardsFX);
            stopFX(burrowFX);
            a->stopAllActions();
            hiddenTimer = hiddenTime;
            state = kHidden;
            relocate();
            goToState(kHidden, true, true);
        }
        break;
    case kRobotScorpionFiring:
        if (!a->isPlaying()) {
            if (zapPhase == 1) {
                a->play("electrocuting_loop", -1, false);
                // (see vf3: the original took the charge off its animation here)
                placeFX();
                resetFX(chargeFX);
            } else if (zapPhase == 2) {
                dashesLeft = static_cast<int>(dashes - 1.0f);
                state = kRobotScorpionDashing;
            }
        } else if (zapPhase == 1) {
            chargeTimer -= dt;
            if (chargeTimer <= 0) {
                // the zap: the electricity, the shock, its damage, a Rust where it stands
                a->play("electrocuting_end", 1, false);
                stopFX(chargeFX);
                sound->playRandomSound("battery_zap", 3, false);
                const CCPoint p = a->getPosition();
                post(effectAt(2, p));
                post(effectAt(1, position));
                post(damageEvent(static_cast<int>(zapDamage)));
                zapPhase = 2;
                post(createEvent("Rust", static_cast<float>(static_cast<int>(p.x)),
                                 static_cast<float>(static_cast<int>(p.y)),
                                 static_cast<float>(static_cast<int>(a->getRotation()))));
            }
        }
        break;
    case kHidden: {
        hurtTimer -= dt;
        if (hurtTimer <= 0) {
            // (0.7 damage made whole points in the event, as the original's:
            // none; its stage's damage hurts meanwhile)
            post(damageEvent(static_cast<int>(hurtAmount)));
            hurtTimer = hurtEvery;
        }
        hiddenTimer -= dt;
        if (hiddenTimer <= 0) {
            // a cut where it was, then it goes on elsewhere
            const CCPoint p = a->getPosition();
            post(createEvent("SmallCutRobot", static_cast<float>(static_cast<int>(p.x)),
                             static_cast<float>(static_cast<int>(p.y)),
                             static_cast<float>(static_cast<int>(a->getRotation()))));
            hiddenTimer = hiddenTime;
            relocate();
            hurtFromUnder();
        }
        break;
    }
    case kRobotScorpionTazered:
        clamp = tailClamp();
        if (!clamp) {
            holdTimer -= dt;
            if (holdTimer <= 0) {
                // let go: it burrows
                goToState(kRobotScorpionEmerging, true, true);
                burrow();
            }
        } else {
            // a clamp on its tail holds it: the clamp put on its tail
            const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
            const double t = (270.0f - a->getRotation()) * kDegToRad;
            const CCPoint p = a->getPosition();
            clamp->node->setRotation(a->getRotation());
            clamp->node->setPosition(ccp(static_cast<float>(45.0f * s * std::cos(t)) + p.x,
                                         static_cast<float>(std::sin(t) * (45.0f * s)) + p.y));
            log("robotscorpion clamped: wheel at %.0f,%.0f",
                clamp->node->convertToWorldSpace(clamp->wheel->getPosition()).x,
                clamp->node->convertToWorldSpace(clamp->wheel->getPosition()).y);  // (for test scripts)
            a->play("clamped", -1, false);
            holdTimer = clampTime;
            state = kRobotScorpionClamped;
            openness = 1;
            kit::postGoodPopup(*this);
        }
        break;
    case kRobotScorpionClamped: {
        holdTimer -= dt;
        if (holdTimer <= 0) {
            // not cut in time: it shrugs the clamp off and burrows
            clamp = nullptr;
            releaseClamps();
            burrow();
            // (the port's: it shakes itself free first, the unused
            // "clamped_released" of its animation, then burrows)
            a->play("clamped_released", 1, false);
            a->queue("burrow", 1, false);
            log("robotscorpion clamp released");  // (for test scripts)
            break;
        }
        clamp = tailClamp();
        float open = 1;
        if (clamp) {
            open = 1.0f - clamp->tightness / clamp->maxTightness;
            if (open == 0 && openness != 0) {
                // tightened fully
                kit::postGoodPopup(*this);
                openness = open;
                break;
            }
        }
        // loosened (or taken off) after being tightened: it hurts
        if (openness < open) kit::postHurt(*this, 5);
        openness = open;
        break;
    }
    default:
        break;
    }
    dashing = state == kRobotScorpionDashing;
}

void RobotScorpion::tazered(HitResult& r) {
    Animation* a = anim(*this);
    if (a) a->stopAllActions();
    goToState(kRobotScorpionTazered, true, true);
    if (a) a->play("tazer", -1, false);
    // (the port's: its charge ends with it)
    stopFX(chargeFX);
    kit::comboFor(*this, r);
    state = kRobotScorpionTazered;
    holdTimer = tazerTime;
    kit::postGoodPopup(*this);
}

void RobotScorpion::explode() {
    // (vf18's body) the explosion where it is, three shards about it, Dead,
    // finished at once (its last stage)
    Animation* a = anim(*this);
    const CCPoint p = a ? a->getPosition() : position;
    post(effectAt(6, p));
    postShard(1);
    postShard(2);
    postShard(3);
    if (a) a->setVisible(false);
    goToState(kRobotScorpionDead, true, true);
    finishStage();
}

HitResult RobotScorpion::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8) the tazer stuns it; the cutter brings it up; the chainsaw cuts
    // it, tightly clamped
    HitResult r;
    r.injury = this;
    if (stage().useSizeRect && touchInPlainRect(*this, touch)) r.touched = true;
    if (!takesTool(tool->type)) return r;
    if (!r.touched) return r;
    r.hit = true;
    onTouch(touch);
    switch (tool->type) {
    case kCutter:
        if (state == kHidden) {
            state = kRobotScorpionEmerging;
            goToState(kRobotScorpionEmerging, true, true);
            emerge();
        }
        break;
    case kChainsaw:
        if (state == kRobotScorpionClamped) {
            r.b1d = true;
            clamp = tailClamp();
            if (clamp && 1.0f - clamp->tightness / clamp->maxTightness == 0) {
                --sawHits;
                if (sawHits < 1) {
                    releaseClamps();
                    explode();
                    kit::postGoodPopup(*this);
                } else if (sawHits % sawPopupEvery == 0) {
                    kit::postGoodPopup(*this);
                }
            }
        }
        break;
    case kTazer:
        switch (state) {
        case kRobotScorpionDashing:
        case kRobotScorpionTurning:
        case kRobotScorpionCharging:
            tazered(r);
            break;
        case kRobotScorpionEmerging:
        case kRobotScorpionBurying:
            break;
        case kRobotScorpionFiring:
            // charging its tail it can be stunned; once it zapped, it hurts
            if (zapPhase == 1) tazered(r);
            else kit::postHurt(*this, 5);
            break;
        case kRobotScorpionTazered:
        case kRobotScorpionClamped:
            holdTimer = tazerTime;
            break;
        default:
            kit::postHurt(*this, 5);
            break;
        }
        break;
    default:
        // (the original gave its first piece: it has none)
        r.piece = pieces.empty() ? nullptr : &pieces[0];
        break;
    }
    return r;
}

void RobotScorpion::completeStage() {
    // (vf18: the tutorial's, a partner's) blown apart
    explode();
}

int RobotScorpion::toolFor(const CCPoint& touch) {
    // (vf27) the tazer; stunned a clamp; tightly clamped the chainsaw; under
    // the skin the cutter (the scanner to find it); dead the tongs
    if (!stage().useSizeRect) return kNoTool;
    const int st = stage().state;
    if (!touchInRect(*this, touch)) return st == kHidden ? kScanner : kNoTool;
    if (st == kRobotScorpionDead) return kTongs;
    if (st == kHidden) return kCutter;
    const bool tight = !clamps.empty() && clamps[0]->tightness == clamps[0]->maxTightness;
    if (clamp && tight) return kChainsaw;
    if (st == kRobotScorpionTazered) return kClamp;
    if (st == kClamped) return tight ? kTazer : kClamp;
    return kTazer;
}

}  // namespace mt
