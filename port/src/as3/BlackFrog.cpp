#include "as3/BlackFrog.h"

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
constexpr float kDegToRad = 0.017453292519943295f;
}

BlackFrog::BlackFrog() {
    // (0x98a54)
    level = levels;
    health = burnHealth;
    zOrder = 101;
    priority = 3;
    sendsGraphicsEvent = false;
    timer = randFloat(0, 2.0f);
    hops = randInt(0, 10);
}

void BlackFrog::start() {
    // (vf2: not the base's)
    if (hasStage()) state = stage().state;
}

void BlackFrog::vf3() {
    // (vf3) its piece for the tongs; still on its first frame; its first hop
    m_flag58 = true;
    pieces.clear();
    addCreaturePiece(*this, 0x3a);
    if (Animation* a = anim(*this)) {
        a->play("hop", 1, false);
        a->stop();
    }
    target = hopTarget(position, 100, 0);
    heading();
}

void BlackFrog::heading() { angle = headingTo(position, target, dir); }

void BlackFrog::targetInArea() {
    // (0x1525ec)
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float mx = (kCanvasW - r->width / s) * 0.5f, my = (kCanvasH - r->height / s) * 0.5f;
    target.x = static_cast<float>(randInt(0, static_cast<int>(s * r->rect.size.width))) + s * (r->rect.origin.x - mx);
    target.y = static_cast<float>(randInt(0, static_cast<int>(s * r->rect.size.height))) + s * (r->rect.origin.y - my);
    heading();
}

void BlackFrog::targetOffScreen() {
    // (0x152c44) a random way out, the screen's width from its middle
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    float x = randFloat(-1, 1), y = randFloat(-1, 1), len = std::sqrt(x * x + y * y);
    if (len == 0) x = 1, len = 1;
    target.x = static_cast<float>(r->width / 2) + x / len * static_cast<float>(r->width);
    target.y = static_cast<float>(r->height / 2) + y / len * static_cast<float>(r->width);
    heading();
}

void BlackFrog::flash(float dt) {
    // (0x152548) red and white for a while once burnt
    Animation* a = anim(*this);
    if (flashLeft <= 0 || !a) return;
    flashLeft -= dt;
    if (flashLeft <= 0) {
        flashOn = false;
        a->setColor(ccc3(255, 255, 255));
        return;
    }
    flashTick -= dt;
    if (flashTick > 0) return;
    const bool was = flashOn;
    flashOn = !flashOn;
    a->setColor(was ? ccc3(255, 255, 255) : ccc3(255, 0, 0));
    flashTick = flashEvery;
}

void BlackFrog::vf4(float dt) {
    // (vf4)
    tracePosition(*this);
    flash(dt);
    const int st = hasStage() ? stage().state : -1;
    if (st != kBlackFrogNormal && st != kBlackFrogAngry) return;
    const bool angry = st == kBlackFrogAngry;
    const float before = timer;
    timer -= dt;
    if (timer > 0) return;
    Animation* a = anim(*this);
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    const Level& lv = table[std::max(0, level - 1)];
    if (layNext) {
        layNext = false;
        laying = true;
        laid = false;
        if (a) a->play("spawn", 1, false);
        sound->playSound("black_frog_egg_lay", false);
        return;
    }
    if (laying) {
        if (!a || !a->isPlaying()) {
            laying = false;
            timer = 0;
            if (!spitNext) timer = lv.wait;
        } else if (!laid && static_cast<float>(a->lengthOf("spawn")) * 0.6666667f <= static_cast<float>(a->frame())) {
            // its eggs behind it (five when burnt)
            const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
            const int n = angry ? 5 : 1;
            for (int i = 0; i < n; ++i) {
                const float t = (rotation + eggs[i][0]) * kDegToRad;
                post(createEvent("MiniFrog", position.x - eggs[i][1] * s * std::sin(t),
                                 position.y - eggs[i][1] * s * std::cos(t), rotation));
            }
            laid = true;
        }
        return;
    }
    if (spitNext) {
        spitNext = false;
        spitting = true;
        spat = false;
        setRotation(rotation + 180.0f);
        if (a) a->play("tongue", 1, false);
        sound->playRandomSound("poison_frog_dart_spit", 2, false);
        return;
    }
    if (spitting) {
        if (!a || !a->isPlaying()) {
            spitting = false;
            timer = lv.wait;
        } else if (!spat && static_cast<float>(a->lengthOf("tongue")) * 0.6666667f <= static_cast<float>(a->frame())) {
            // its poison where its tongue reaches
            const float t = rotation * kDegToRad, d = def->size.height * 0.75f;
            post(createEvent("FrogPoisonArea", position.x + d * std::sin(t), position.y + d * std::cos(t), 0));
            spat = true;
        }
        return;
    }
    // a hop (a leap when burnt)
    if (before > 0) {
        setRotation(angle);
        if (angry) {
            if (!off) {
                if (a) a->play("jump_off", 1, false);
                sound->playSound("black_frog_leap", false);
            } else {
                sound->playSound("black_frog_landing", false);
            }
        } else {
            if (a) a->play("hop", 1, false);
            sound->playSound("black_frog_hop", false);
        }
    }
    const float v = (angry ? angrySpeed : speed) * ResolutionMgr::sharedResolutionMgr()->canvasScale;
    const float nx = position.x + dir.x * v * dt, ny = position.y + dir.y * v * dt;
    const bool overX = (dir.x > 0 && target.x < nx) || (dir.x < 0 && nx < target.x);
    const bool overY = (dir.y > 0 && target.y < ny) || (dir.y < 0 && ny < target.y);
    if (!overX && !overY) {
        moveTo(*this, ccp(nx, ny));
        return;
    }
    // landed
    if (angry) {
        if (!off) {
            off = true;
            away = true;
            timer = offTime;
        } else {
            if (a) a->play("jump_on", 1, false);
            goToState(kBlackFrogNormal, true, true);
            off = false;
            away = false;
            timer = backDelay;
        }
    } else {
        ++hops;
        layNext = hops % lv.eggEvery == 0;
        spitNext = hops % lv.spitEvery == 0;
        if (!layNext && !spitNext) timer = lv.wait;
    }
    moveTo(*this, target);
    if (hasStage() && stage().state == kBlackFrogNormal) {
        target = hopTarget(position, 100, 0);
        heading();
    } else {
        targetInArea();
    }
}

void BlackFrog::die() {
    SoundMgr::sharedSoundMgr()->playOnChannel("black_frog_final_burn", soundChannel, false, true, false, false);
    if (Animation* a = anim(*this)) a->play("dead", 1, false);
    goToState(kBlackFrogDead, true, true);
}

HitResult BlackFrog::hitTest(const CCPoint& touch, ToolState* tool) {
    // (vf8) burnt down a level; dead, the tongs take it
    HitResult r;
    if (stage().useSizeRect && touchInRect(*this, touch)) r.touched = true;
    if (!takesTool(tool->type)) {
        if (r.touched) r.injury = this;
        return r;
    }
    if (!r.touched) return r;
    r.hit = true;
    r.injury = this;
    const int st = stage().state;
    if (st == kBlackFrogDead) {
        r.piece = grabPiece(*this);
        return r;
    }
    if (st != kBlackFrogNormal) return r;
    flashLeft = flashTime;
    health -= tool->power;
    if (health > 0) return r;
    if (--level < 1) {
        die();
    } else {
        // burnt: it lays, then leaps off the screen
        health = burnHealth;
        timer = burntDelay;
        layNext = true;
        spitNext = false;
        laying = false;
        spitting = false;
        targetOffScreen();
        SoundMgr::sharedSoundMgr()->playOnChannel("black_frog_burning_loop", soundChannel, false, true, false, false);
        goToState(kBlackFrogAngry, true, true);
    }
    giveCombo(r, *this);
    return r;
}

void BlackFrog::completeStage() {
    // (vf18)
    if (stage().state == kBlackFrogDead) {
        finishStage();
        return;
    }
    if (--level < 1) die();
}

int BlackFrog::toolFor(const CCPoint& touch) {
    // (vf27)
    if (!touchInRect(*this, touch)) return kNoTool;
    const int st = stage().state;
    if (st == kBlackFrogNormal) return kLighter;
    if (st == kBlackFrogDead) return kTongs;
    return kNoTool;
}

}  // namespace mt
