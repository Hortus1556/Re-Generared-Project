#include "as3/Jellyfish.h"

#include <cmath>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "as3/ToolState.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

namespace {
// an effect event (3) at whole points
InjuryEvent effectAt(int kind, const CCPoint& at) {
    InjuryEvent e;
    e.type = InjuryEvent::kEffect;
    e.effect = kind;
    e.x = static_cast<float>(static_cast<int>(at.x));
    e.y = static_cast<float>(static_cast<int>(at.y));
    return e;
}
}  // namespace

Jellyfish::Jellyfish() {
    // (1.45 0xc3994) zOrder +0x10230, no graphics event +0x10247, its
    // priority +0x10250
    zOrder = 100;
    sendsGraphicsEvent = false;
    priority = 2;
}

void Jellyfish::start() {
    // (vf2 0x4c0fac) alive, untouched; its clock somewhere along its figure,
    // its first sting within half the usual wait. (1.45 writes the def's
    // first piece slot over here, kind 0xf, its rect at a random place
    // 0..100: the port's piece is made in vf3)
    dying = false;
    hits = 0;
    side = -1;
    dartTimer = 0;
    clock = randFloat(0, 50);
    stingTimer = randFloat(0, stingEvery * 0.5f);
    if (hasStage()) state = stage().state;
}

void Jellyfish::vf3() {
    // (vf3 0x4c10b4) swimming; its animation is its piece's node
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0xf);
    if (!pieces.empty())
        pieces[0].rect.origin = ccp(static_cast<float>(randInt(0, 100)), static_cast<float>(randInt(0, 100)));
    startSwimming();
}

void Jellyfish::startSwimming() {
    // (0x4c10d4) its figure's centre from its place (canvas points); the
    // injury's root raised to 250, over the others
    centre = toCanvas(position);
    setNewRandomPositionToGo();
    if (root) root->setZOrder(250);
    if (Animation* a = anim(*this)) {
        a->stopAllActions();
        a->play("swimming", -1, false);
    }
}

void Jellyfish::setNewRandomPositionToGo() {
    // (0x4c247c) a random side of the play area (0 top, 1 bottom, 2 left,
    // 3 right), a random place along it, the unit way there from the centre
    const CCRect& a = ResolutionMgr::sharedResolutionMgr()->rect;
    const float l = a.origin.x, b = a.origin.y, w = a.size.width, h = a.size.height;
    side = randInt(0, 3);
    CCPoint to;
    switch (side) {
    case 0:
        to = ccp(static_cast<float>(randInt(static_cast<int>(l), static_cast<int>(l + w))), b + h);
        break;
    case 1:
        to = ccp(static_cast<float>(randInt(static_cast<int>(l), static_cast<int>(l + w))), b);
        break;
    case 2:
        to = ccp(l, static_cast<float>(randInt(static_cast<int>(b), static_cast<int>(b + h))));
        break;
    default:
        to = ccp(l + w, static_cast<float>(randInt(static_cast<int>(b), static_cast<int>(b + h))));
        break;
    }
    const float dx = to.x - centre.x, dy = to.y - centre.y, len = std::sqrt(dx * dx + dy * dy);
    heading = len > 0 ? ccp(dx / len, dy / len) : ccp(0, 0);
}

void Jellyfish::vf4(float dt) {
    // (vf4 0x4c1190) its bob runs always, a second a second between
    // -bobRange and bobRange
    tracePosition(*this);
    if (bobUp) {
        bob += dt;
        if (bob >= bobRange) bobUp = false;
    } else {
        bob -= dt;
        if (bob <= -bobRange) bobUp = true;
    }
    if (!hasStage()) return;
    Animation* a = anim(*this);
    if (stage().state == kSwimming) {
        // its clock at 4 a second (9 darting); as the dart ends it swims
        // again (the frame before the timer runs out)
        const int speed = dartTimer > 0 ? dartSpeed : swimSpeed;
        const float left = dartTimer - dt;
        clock += static_cast<float>(speed) * dt;
        dartTimer = left;
        if (left > 0 && left - dt <= 0 && a) {
            a->stopAllActions();
            a->play("swimming", -1, false);
        }
        centre = ccp(std::sin(clock * swayXRate) * swayX + 320.0f, std::cos(clock * swayYRate) * swayY + 192.0f);
        const CCPoint at = fromCanvas(centre);
        moveTo(*this, at);
        if (a) a->setPosition(at);
        stingTimer -= dt;
        if (stingTimer < 0) sting();
    } else if (dying) {
        // dead: it drifts up and down with its bob. (Fixed: not once the
        // tongs have it; the original's piece was the animation itself, which
        // this moved under the tongs)
        if (!pieces.empty() && pieces[0].sprite) return;
        centre.y += deadDrift * (bob / bobRange);
        const CCPoint at = fromCanvas(centre);
        moveTo(*this, at);
        if (a) a->setPosition(at);
    }
}

void Jellyfish::sting() {
    // (in 0x4c1190) "sting", then swimming again; electricity where it is,
    // the zap, the shock, its damage, and a JellyfishSting where it is,
    // turned as it is
    Animation* a = anim(*this);
    stingTimer = stingEvery;
    const CCPoint at = a ? a->getPosition() : position;
    if (a) {
        a->stopAllActions();
        a->play("sting", 1, false);
        a->queue("swimming", -1, false);
    }
    post(effectAt(2, at));
    SoundMgr::sharedSoundMgr()->playRandomSound("battery_zap", 3, false);
    post(effectAt(1, fromCanvas(centre)));
    post(damageEvent(static_cast<int>(stingDamage)));
    post(createEvent("JellyfishSting", at.x, at.y, a ? a->getRotation() : rotation));
}

HitResult Jellyfish::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8 0x4c262c) the tazer takes a life a zap (not while "tazer" plays),
    // sending it darting; the last kills it. Dead, the tongs take it
    HitResult r;
    Animation* a = anim(*this);
    if (stage().useSizeRect && touchInRect(*this, touch)) r.touched = true;
    if (!takesTool(tool->type)) {
        if (r.touched) r.injury = this;
        return r;
    }
    if (r.touched) r.hit = true;
    if (!r.hit) return r;
    onTouch(touch);
    if (stage().state == kSwimming) {
        if (tool->type == kTazer && playing(a) != "tazer") {
            ++hits;
            if (a) a->stopAllActions();
            if (hits < lives) {
                giveCombo(r, *this);
                dartTimer = dartTime;
                if (a) {
                    a->play("tazer", tazerLoops, false);
                    a->queue("darting", -1, false);
                }
            } else {
                dying = true;
                SoundMgr::sharedSoundMgr()->playOnChannel("jellyfish_death", soundChannel, false, true, false, false);
                if (a) a->play("dead", -1, false);
                r.stageDone = true;
                giveCombo(r, *this);
                finishStage();
            }
        }
    } else {
        r.piece = grabPiece(*this);
    }
    if (r.hit) r.injury = this;
    return r;
}

void Jellyfish::completeStage() {
    // (vf18 0x4c2968) dead: its stage finished (taken out); swimming: a life
    // taken, the last killing it
    if (stage().state == kDied) {
        finishStage();
        return;
    }
    if (stage().state != kSwimming) return;
    if (++hits < lives) return;
    SoundMgr::sharedSoundMgr()->playOnChannel("jellyfish_death", soundChannel, false, true, false, false);
    dying = true;
    if (Animation* a = anim(*this)) {
        a->stopAllActions();
        a->play("dead", 1, false);
    }
    finishStage();
}

int Jellyfish::toolFor(const CCPoint& touch) {
    // (vf27 0x4c242c) (no use_size_rect test, as the flea's has)
    if (!touchInRect(*this, touch)) return kNoTool;
    if (stage().state == kSwimming) return kTazer;
    if (stage().state == kDied) return kTongs;
    return kNoTool;
}

}  // namespace mt
