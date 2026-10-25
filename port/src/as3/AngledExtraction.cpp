#include "as3/AngledExtraction.h"

#include <cmath>
#include <cstdlib>

#include "as3/ArtHit.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SoundMgr.h"
#include "as3/StatsMgr.h"
#include "as3/ToolState.h"
#include "as3/UpgradeInfoUtils.h"
#include "engine/cocos/CCClippingNode.h"
#include "engine/cocos/CCDrawNode.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

AngledExtraction::AngledExtraction() {
    // (0x99f6c) a random turn of the object of nothing (random() % 1)
    angleOffset = 0;
    zOrder = 50;
    tolerance = isSmallScreen() ? 45 : 30;
    sendsGraphicsEvent = false;
}

void AngledExtraction::start() {
    // (vf2) the heart rate upgrade's perfect extraction is possible until it slips
    perfect = SaveData::shared().currentSlot().owns(SavedGame::kItemUpgrade, 23);
    comboGiven = false;
    if (!hasStage() || stage().createAtStart.empty()) return;
    // its wound under it, turned with it (by -90 more when embedded with no offset)
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    InjuryEvent e;
    e.type = InjuryEvent::kCreateInjury;
    e.name = stage().createAtStart;
    bool turn = def->embedded && stage().createAtStartOffset.x == 0 && stage().createAtStartOffset.y == 0;
    e.rotation = std::floor(turn ? rotation - 90.0f : rotation);
    e.x = std::floor(position.x + stage().createAtStartOffset.x * s);
    e.y = std::floor(position.y + stage().createAtStartOffset.y * s);
    e.allowedOverLock = true;
    e.countsTowardCap = false;
    // (0x8790c: +0x70 = this in the same case) an embedded object's wound
    // can't be treated while the object is in it (the port's could)
    if (turn) e.interactionDependency = this;
    post(e);
    state = stage().state;
}

bool AngledExtraction::pieceContains(const PieceState& piece, const CCPoint& touch) const {
    // (vf13) Fixed: where its art shows above the skin (all of it when not
    // embedded), or within a finger's slack of it (8 points, 10 where the
    // hit areas are big), ArtHit.h. The original took the touch anywhere in
    // its rect (0x77c4c: the picture's whole width, from the skin line to
    // its end), so of objects near each other the one first in the list took
    // what was meant for the others: the whale's shark, a fin over a buried
    // body as wide as the patient's middle, took the idols' and the puffin's.
    if (sprites.empty() || !clip || sprites[0]->getParent() != clip) return InjuryState::pieceContains(piece, touch);
    // the touch in the clipping node's space (the injury's place and turn)
    const float a = rotation * 3.14159265f / 180.0f;
    const float dx = touch.x - position.x, dy = touch.y - position.y;
    const CCPoint local = ccp(dx * std::cos(a) - dy * std::sin(a), dx * std::sin(a) + dy * std::cos(a));
    const float skin = 0;
    const float slack = 8.0f * (hasBigHitAreas(*def) ? 1.25f : 1.0f);
    return artNear(sprites[0], local, slack, def->embedded ? &skin : nullptr);
}

int AngledExtraction::toolFor(const CCPoint& touch) {
    if (pieces.empty() || (!pieceContains(pieces[0], touch) && !holding)) return kNoTool;
    return stage().tools.empty() ? kNoTool : stage().tools[0];
}

void AngledExtraction::placeNodes() {
    if (clip) clip->setPosition(position);
}

void AngledExtraction::setRotation(float degrees) {
    rotation = degrees;
    if (clip) clip->setRotation(degrees);
    rect = CCRectMake(position.x - def->size.width * 0.5f, position.y - def->size.height * 0.5f, def->size.width,
                      def->size.height);
    placeHitPoints();
    placeNodes();
}

void AngledExtraction::onFinished() {
    // (Android 1.43, AngledExtraction::onHealed 0x567e94) the Hospital's
    // coffee beans (child classes 117-119) hurt by 5 as they come out (event
    // 9, as iOS's).
    if (def->classIndex >= 117 && def->classIndex <= 119) {
        InjuryEvent e;
        e.type = InjuryEvent::kDamage;
        e.damage = 5;
        post(e);
    }
    // (1.45 AngledExtraction::onHealed 0xafa78) an object pulled out: stat 9 (StatsMgr.h)
    StatsMgr::sharedInstance()->incrementStat(StatTypes::kExtractItem);
}

void AngledExtraction::released() {
    takenOut = false;
    holding = false;
    path.resize(path.empty() ? 0 : 1);
}

void AngledExtraction::slipBack() {
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    if (!sprites.empty()) sprites[0]->setPosition(ccp(0, -(def->depth * s)));
    holding = false;
    perfect = false;
}

HitResult AngledExtraction::hitTest(const CCPoint& touch, ToolState* tool) {
    HitResult r;
    if (!takesTool(tool->type)) {
        perfect = false;
        return r;
    }
    if (takenOut) {
        r.hit = true;
        r.injury = this;
        return r;
    }
    if (pieces.empty()) return r;
    if (!holding) {
        // taken hold of
        if (pieceContains(pieces[0], touch)) {
            log("injury #%d taken hold at %.0f,%.0f", soundChannel, touch.x, touch.y);  // (for test scripts)
            lastTouch = touch;
            holding = true;
            path.clear();
            path.push_back(touch);
            samples.clear();
            r.hit = true;
            r.injury = this;
            r.piece = &pieces[0];
        } else {
            perfect = false;
            holding = false;
        }
        return r;
    }
    // pulled: the direction from where the pull began against the object's
    float dx = touch.x - path[0].x, dy = touch.y - path[0].y;
    float angle = std::atan2(dy, dx) * 57.29577951f;
    float moved2 = (touch.x - lastTouch.x) * (touch.x - lastTouch.x) + (touch.y - lastTouch.y) * (touch.y - lastTouch.y);
    lastTouch = touch;
    float expected = 90.0f - (rotation + angleOffset);
    if (moved2 > 0 && def->embedded && samples.size() < 5)
        samples.push_back(static_cast<int>(std::fabs(angle - static_cast<float>(static_cast<int>(expected)))));
    if (samples.size() > 2) {
        float sum = 0;
        for (int v : samples) sum += static_cast<float>(v);
        int off = std::abs(static_cast<int>(sum / static_cast<float>(samples.size())) % 360);
        if (off >= tolerance) off = 360 - off;
        if (off >= tolerance) {
            // the wrong way: it slips back
            SoundMgr::sharedSoundMgr()->playOnChannel("organ_placement", soundChannel, false, true, false, false);
            slipBack();
            return r;
        }
        SoundMgr::sharedSoundMgr()->playOnChannel("extraction_1", soundChannel, false, false, false, false);
        samples.clear();
    }
    const float reach = static_cast<const Tongs*>(tool)->reach;
    if (moved2 <= reach * reach || !def->embedded) {
        // it comes out by the pull's length (fixed: 0x86fa0 moved it by the
        // length squared, 0x64584's, so it leapt out faster than the finger)
        if (!sprites.empty()) {
            CCSprite* sp = sprites[0];
            sp->setPositionY(sp->getPositionY() + std::sqrt(moved2));
            CCPoint p = sp->getPosition();
            if (static_cast<float>(extractDistance * extractDistance) < p.x * p.x + p.y * p.y || !def->embedded) {
                SoundMgr::sharedSoundMgr()->stopChannel(soundChannel, false);
                if (!extractedEventSent && !stage().createOnExtraction.empty()) {
                    extractedEventSent = true;
                    InjuryEvent e;
                    e.type = InjuryEvent::kCreateInjury;
                    e.name = stage().createOnExtraction;
                    e.x = std::floor(position.x);
                    e.y = std::floor(position.y);
                    e.rotation = std::floor(rotation);
                    // (0x86fa0 leaves the event's flags as made: not over a
                    // locked area, counted toward the cap, unlike vf2's)
                    post(e);
                }
                holding = false;
                if (!createdLater && !comboGiven) {
                    r.comboAction = true;
                    comboGiven = true;
                } else {
                    r.comboBonus = true;
                }
            }
        }
        r.hit = true;
        r.injury = this;
        r.piece = &pieces[0];
    } else {
        // too fast: the tongs lose it
        static_cast<Tongs*>(tool)->dropped = true;
        SoundMgr::sharedSoundMgr()->playOnChannel("organ_placement", soundChannel, false, true, false, false);
        slipBack();
    }
    return r;
}

void AngledExtraction::buildPictures() {
    // (0x77c4c, base 0) the object's sprite in a clipping node at the
    // injury's place and turn: only what is above the skin line shows (all of
    // it when not embedded); it starts `depth` into the body
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    CCSprite* sp = CCSprite::create();
    if (CCSpriteFrame* f = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(frameName().c_str()))
        sp->setDisplayFrame(f);
    sp->setRotation(static_cast<float>(angleOffset));
    sp->setAnchorPoint(ccp(0.5f, 0.5f));
    CCSize size = sp->getContentSize();
    float k = hasBigHitAreas(*def) ? 1.25f : 1.0f;
    float w = size.width * k, h = size.height;
    // the stencil: the space above the skin line (0x2eae4's rectangle)
    float bottom = def->embedded ? 0 : -h * 2;
    CCPoint verts[4] = {ccp(-w, bottom), ccp(w, bottom), ccp(w, h * 2), ccp(-w, h * 2)};
    CCDrawNode* stencil = CCDrawNode::create();
    stencil->drawPolygon(verts, 4, ccc4f(1, 1, 1, 1), 1, ccc4f(1, 1, 1, 1));
    clip = CCClippingNode::create(stencil);
    clip->addChild(sp);
    clip->setPosition(position);
    clip->setRotation(rotation);
    spritesNode->addChild(clip);
    sp->setPosition(ccp(0, -(def->depth * s)));
    sprites.push_back(sp);
    extractDistance = static_cast<int>(size.height * 0.5f);
    if (!pieces.empty()) {
        // the object's own rect, in its unturned frame: what sticks out of
        // the skin, from the skin line (the injury's place) to its end; the
        // whole of it when not embedded (0x77c4c: x - k·w/2, y (y - k·h/2),
        // k·w, k·(h/2 − depth·s) (k·(h − depth·s))). (The port's covered the
        // whole object, the buried half too, so a touch beside an object
        // took one buried under the skin there: of overlapping objects the
        // wrong one came up.)
        PieceState& p = pieces[0];
        p.sprite = sp;
        p.rect = def->embedded ? CCRectMake(position.x - w * 0.5f, position.y, w, k * (h * 0.5f - def->depth * s))
                               : CCRectMake(position.x - w * 0.5f, position.y - k * h * 0.5f, w, k * (h - def->depth * s));
        // (traced for test scripts: how far it sticks out along its axis,
        // and how far it must be pulled to come out)
        log("injury #%d sticks out %.0f pull %.0f", soundChannel, h * 0.5f - def->depth * s,
            extractDistance + def->depth * s);
    }
}

}  // namespace mt
