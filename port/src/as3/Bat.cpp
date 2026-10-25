#include "as3/Bat.h"

#include <cmath>

#include "as3/Animation.h"
#include "as3/Creature.h"
#include "as3/Log.h"
#include "as3/SoundMgr.h"
#include "as3/StatsMgr.h"
#include "as3/ToolState.h"

namespace mt {

using namespace cocos2d;
using namespace creature;

Bat::Bat() {
    // (0x99c2c, the factory's tuning)
    zOrder = 100;
    priority = 2;
    sendsGraphicsEvent = false;
}

void Bat::start() {
    // (vf2: not the base's: no events at its start)
    dying = false;
    spawnsMiniBats = true;
    tazerHits = 0;
    lastSide = -1;
    fastLeft = 0;
    cutTimer = cutEvery;
    if (hasStage()) state = stage().state;
}

void Bat::vf3() {
    // (vf3) its piece for the tongs (vf2's in the original: the port's
    // pictures make the pieces again), and it hangs
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0xb);
    if (Animation* a = anim(*this)) a->play("hanging", -1, false);
}

void Bat::vf4(float dt) {
    // (vf4) flying: round the canvas' middle on its wavy path, faster for a
    // while once tazered, cutting the patient as it goes
    tracePosition(*this);
    Animation* a = anim(*this);
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    if (hasStage() && stage().state == kBatFlying && playing(a) == "flying") {
        const int speed = fastLeft > 0 ? fastSpeed : slowSpeed;
        pathTime += static_cast<float>(speed) * dt;
        if (fastLeft - dt <= 0) sound->playOnChannel("bat_flying_loop_slow", soundChannel, true, true, false, false);
        fastLeft -= dt;
        canvas.x = std::sin(pathTime * xFreq) * xAmp + 320.0f;
        canvas.y = std::cos(pathTime * yFreq) * yAmp + 192.0f + std::cos(pathTime * yFreq2) * yAmp2;
        CCPoint p = fromCanvas(canvas);
        moveTo(*this, p);
        if (a) a->setPosition(p);
        cutTimer -= dt;
        if (cutTimer < 0) {
            const char* cut = randInt(1, 100) <= 25 ? "LargeCut" : "SmallCut";
            post(createEvent(cut, p.x, p.y, static_cast<float>(randInt(0, 359))));
            cutTimer = cutEvery * 0.5f;
        }
        return;
    }
    // dead: the mini bats are born out of it at the 16th frame of "dead"
    if (!dying || !spawnsMiniBats) return;
    std::string cur = playing(a);
    if (!cur.empty() && (cur != "dead" || a->frame() < 16)) return;
    dying = false;
    sound->playSound("bat_mini_birth", false);
    for (int i = 0; i < miniBats; ++i) post(createEvent("MiniBat", position.x, position.y, 0));
}

void Bat::takeFlight() {
    // (0x183fc0) over every other injury, flying from where it hung
    canvas = toCanvas(position);
    CCPoint to = pointOnPlayAreaSide(lastSide);
    float dx = to.x - canvas.x, dy = to.y - canvas.y, len = std::sqrt(dx * dx + dy * dy);
    dir = len > 0 ? ccp(dx / len, dy / len) : CCPointZero;
    if (root) root->setZOrder(200);
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->playRandomSound("bat_take_flight", 2, false);
    sound->playOnChannel("bat_flying_loop_slow", soundChannel, true, false, false, false);
    // (the port's: its wings open first, the unused "opening" of
    // anim_creature_bat_anim, then it flies)
    if (Animation* a = anim(*this)) {
        a->play("opening", 1, false);
        a->queue("flying", -1, false);
    }
    log("creature #%d Bat opens its wings", soundChannel);  // (for test scripts)
    finishStage();
}

HitResult Bat::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8) any tool on it while it hangs makes it shudder, the lighter makes
    // it fly; flying, the tazer stuns it (three times kill it); dead, the
    // tongs take it
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
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    const int st = stage().state;
    if (st == kBatFlying) {
        if (tool->type == kTazer) {
            sound->playOnChannel("bat_taser", 2, soundChannel, false, true, false, false);
            if (playing(a) != "tazer") {
                ++tazerHits;
                if (a) a->stopAllActions();
                fastLeft = fastTime;
                if (tazerHits < hitsToKill) {
                    giveCombo(r, *this);
                    if (a) {
                        a->play("tazer", tazerLoops, false);
                        a->queue("flying", -1, false);
                    }
                    sound->playOnChannel("bat_flying_loop_fast", soundChannel, true, false, false, false);
                } else {
                    dying = true;
                    if (a) {
                        a->play("tazer", 1, false);
                        a->queue("death", 1, false);
                    }
                    sound->playOnChannel("bat_death", soundChannel, false, true, false, false);
                    if (spawnsMiniBats && a) a->queue("dead", 1, false);
                    r.stageDone = true;
                    giveCombo(r, *this);
                    finishStage();
                }
            }
        }
    } else if (st == kBatHanging) {
        if (tool->type == kLighter) {
            takeFlight();
            r.stageDone = true;
            giveCombo(r, *this);
        } else if (playing(a) != "reaction" && a) {
            a->play("reaction", 1, false);
            sound->playRandomSound("bat_shudder", 2, false);
            a->queue("hanging", -1, false);
        }
    } else {
        r.piece = grabPiece(*this);
    }
    if (r.hit) r.injury = this;
    return r;
}

HitResult Bat::injectionHitTest(const CCPoint& touch, int mode, float fill) {
    // (vf9) an injection given while it hangs (mode 0) and before the
    // syringe filled to a tenth leaves it without mini bats (and the stat)
    HitResult r;
    if (mode == 0 && hasStage() && stage().useSizeRect && touchInRect(*this, touch)) {
        r.hit = true;
        // (1.45 Bat::vf9 0x211a08: the mutagen drawn, stat 8, StatsMgr.h)
        if (fill < 0.1 && spawnsMiniBats) {
            spawnsMiniBats = false;
            StatsMgr::sharedInstance()->incrementStat(StatTypes::kExtractBatMutagen);
        }
    }
    return r;
}

void Bat::completeStage() {
    // (vf18) its stage done for it (the tutorial, a tag partner)
    const int st = stage().state;
    if (st == kBatHanging) {
        takeFlight();
        return;
    }
    if (st == kBatFlying) {
        if (++tazerHits < hitsToKill) return;
        dying = true;
        if (Animation* a = anim(*this)) a->queue("dead", 1, false);
    } else if (st != kBatDied) {
        return;
    }
    finishStage();
}

int Bat::toolFor(const CCPoint& touch) {
    // (vf27)
    if (!touchInRect(*this, touch)) return kNoTool;
    switch (stage().state) {
    case kBatDied:
        return kTongs;
    case kBatFlying:
        return kTazer;
    case kBatHanging:
        return spawnsMiniBats ? kInjection : kLighter;
    default:
        return kNoTool;
    }
}

}  // namespace mt
