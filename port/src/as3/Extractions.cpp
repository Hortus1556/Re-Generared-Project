#include "as3/Extractions.h"

#include <cmath>
#include <cstdlib>

#include "as3/ArtHit.h"
#include "as3/InjuryKit.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SoundMgr.h"
#include "as3/StatsMgr.h"
#include "as3/ToolState.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

CCSpriteFrame* frameNamed(const std::string& name) {
    return CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(name.c_str());
}

}  // namespace

// ---- SurfaceExtraction

SurfaceExtraction::SurfaceExtraction() {
    // (0x99e24)
    angleOffset = static_cast<int>(std::rand() % 1);
    zOrder = 50;
    sendsGraphicsEvent = false;
}

void SurfaceExtraction::start() {
    // the heart rate upgrade's perfect extraction; its stage's
    // injury_to_create_at_start (allowed over a locked area, not counted)
    perfect = SaveData::shared().currentSlot().owns(SavedGame::kItemUpgrade, 23);
    if (!hasStage()) return;
    state = stage().state;
    if (stage().createAtStart.empty()) return;
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    InjuryEvent e = kit::createEvent(stage().createAtStart, position.x + stage().createAtStartOffset.x * s,
                                     position.y + stage().createAtStartOffset.y * s, rotation);
    e.allowedOverLock = true;
    e.countsTowardCap = false;
    post(e);
}

void SurfaceExtraction::buildPictures() {
    // (0x77c4c, base 26) its picture at its place and turn, at its piece's
    // z order; the piece's rect its picture's size (1.25 times on a small
    // screen) about its place
    CCSprite* sp = CCSprite::create();
    if (CCSpriteFrame* f = frameNamed(frameName())) sp->setDisplayFrame(f);
    const int z = def->pieces.empty() ? 0 : def->pieces[0].zOrder;
    spritesNode->addChild(sp, z);
    sp->setPosition(position);
    sp->setRotation(rotation);
    sprites.push_back(sp);
    const float k = hasBigHitAreas(*def) ? 1.25f : 1.0f;
    const int w = static_cast<int>(k * sp->getContentSize().width), h = static_cast<int>(k * sp->getContentSize().height);
    if (pieces.empty()) pieces.push_back(PieceState());
    PieceState& p = pieces[0];
    p.sprite = sp;
    p.grabOffset = CCPointZero;
    p.rect = CCRectMake(position.x - static_cast<float>(w / 2), position.y - static_cast<float>(h / 2),
                        static_cast<float>(w), static_cast<float>(h));
}

void SurfaceExtraction::setRotation(float degrees) {
    rotation = degrees;
    rect = CCRectMake(position.x - def->size.width * 0.5f, position.y - def->size.height * 0.5f, def->size.width,
                      def->size.height);
    placeHitPoints();
    placeNodes();
}

bool SurfaceExtraction::pieceContains(const PieceState& piece, const CCPoint& touch) const {
    // (vf13) Fixed: where its art is, or within a finger's slack of it (8
    // points, 10 where the hit areas are big), ArtHit.h; the original took
    // the touch anywhere in its picture's rect, over whatever lay beside its
    // art (AngledExtraction::pieceContains)
    if (sprites.empty() || !spritesNode || sprites[0]->getParent() != spritesNode)
        return InjuryState::pieceContains(piece, touch);
    return artNear(sprites[0], touch, 8.0f * (hasBigHitAreas(*def) ? 1.25f : 1.0f));
}

int SurfaceExtraction::toolFor(const CCPoint& touch) {
    if (hasStage() && stage().useSizeRect && kit::inInjury(*this, touch)) return kTongs;
    return kNoTool;
}

HitResult SurfaceExtraction::hitTest(const CCPoint& touch, ToolState* tool) {
    HitResult r;
    if (!takesTool(tool->type)) return r;
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
            r.hit = true;
            r.injury = this;
            r.piece = &pieces[0];
        } else {
            holding = false;
        }
        return r;
    }
    lastTouch = touch;
    // off: its picture is off its place by more than its extract distance
    // (never set: at once), or it lies on the skin
    const CCPoint at = sprites.empty() ? CCPointZero : sprites[0]->getPosition();
    if (static_cast<float>(extractDistance * extractDistance) < at.x * at.x + at.y * at.y || !def->embedded) {
        if (!extractedEventSent && hasStage() && !stage().createOnExtraction.empty()) {
            extractedEventSent = true;
            InjuryEvent e = kit::createEvent(stage().createOnExtraction, position.x, position.y, rotation);
            post(e);
        }
        holding = false;
        if (!comboGiven) {
            comboGiven = true;
            r.comboAction = true;
        } else {
            r.comboBonus = true;
        }
    }
    r.hit = true;
    r.injury = this;
    r.piece = &pieces[0];
    return r;
}

// ---- UnderSurfaceExtraction

void UnderSurfaceExtraction::onFinished() {
    SoundMgr::sharedSoundMgr()->playRandomSound("incision", 3, false);
    post(kit::effectEvent(3, position.x, position.y));  // BloodSpurt
}

int UnderSurfaceExtraction::toolFor(const CCPoint& touch) {
    if (!hasStage() || !stage().useSizeRect) return kNoTool;
    return kit::inInjury(*this, touch) ? kCutter : kScanner;
}

// ---- Transplant

Transplant::Transplant() {
    zOrder = 25;
    sendsGraphicsEvent = false;
}

Transplant::~Transplant() { kit::dropParticles(dust); }

void Transplant::start() {
    // (vf2) its pieces' records (no stage events, no graphics event)
    if (hasStage()) state = stage().state;
    flags.clear();
    int last = -1;
    for (const InjuryPieceDef& p : def->pieces) {
        Piece f;
        // (the original's record kept the state of its last dependency's hit
        // area, or the previous piece's when it had none; 6 for the
        // replacement)
        if (!p.dependencies.empty()) last = p.dependencies.back().state;
        f.state = p.isReplacement ? kReplacing : last;
        if (p.isReplacement) last = kReplacing;
        f.interactable = p.interactable;
        f.shown = !p.isReplacement;
        flags.push_back(f);
    }
    // the giant worm's wrap raises dust as it is cut
    if (def->classIndex == 92) {  // SpikeClumpTransplant
        dust = kit::particles("particle_dustFX", position);
        kit::stop(dust);
    }
}

void Transplant::placePieces() {
    // each piece's rect about its place (its active rect is from its
    // middle), held by its active rect's middle
    for (size_t i = 0; i < pieces.size(); ++i) {
        PieceState& p = pieces[i];
        const CCRect& a = p.def->activeRect;
        p.rect = CCRectMake(a.origin.x + position.x, a.origin.y + position.y, a.size.width, a.size.height);
        p.grabOffset = ccp(position.x - (p.rect.origin.x + p.rect.size.width * 0.5f),
                           position.y - (p.rect.origin.y + p.rect.size.height * 0.5f));
    }
}

void Transplant::buildPictures() {
    // (0x77c4c, base 20)
    placePieces();
    for (size_t i = 0; i < pieces.size() && i < flags.size(); ++i) {
        if (!flags[i].shown) continue;
        PieceState& p = pieces[i];
        CCSprite* sp = CCSprite::create();
        p.sprite = sp;
        spritesNode->addChild(sp, p.def->zOrder);
        if (CCSpriteFrame* f = frameNamed(p.def->sprite)) sp->setDisplayFrame(f);
        sp->setColor(p.def->tint);
        sp->setPosition(position);
        sp->setRotation(rotation);
        sprites.push_back(sp);
    }
}

std::vector<size_t> Transplant::piecesOf(int s) const {
    std::vector<size_t> out;
    for (size_t i = 0; i < flags.size() && i < pieces.size(); ++i)
        if (flags[i].state == s && (flags[i].interactable || pieces[i].def->isReplacement)) out.push_back(i);
    return out;
}

HitAreaState* Transplant::areaNamed(const std::string& n) {
    HitAreaState* found = nullptr;
    for (HitAreaState& a : areas)
        if (a.def->name == n) found = &a;
    return found;
}

bool Transplant::allOnTray() const {
    if (!hasStage()) return true;
    for (size_t i : piecesOf(stage().state))
        if (flags[i].inBody) return false;
    return true;
}

int Transplant::replacementIndex() const {
    std::vector<size_t> list = piecesOf(kReplacing);
    return list.empty() ? -1 : static_cast<int>(list[0]);
}

void Transplant::placeReplacement(size_t i) {
    if (i >= flags.size()) return;
    flags[i].shown = true;
}

void Transplant::vf4(float dt) {
    // (its dust updated by it as well as by its own schedule, as the original)
    if (dust) dust->update(dt);
    cutThisFrame = false;
}

int Transplant::toolFor(const CCPoint& touch) {
    // (vf27) the tongs on a piece cut free; the stage's tool on its cuts
    if (!hasStage()) return kNoTool;
    for (size_t i : piecesOf(stage().state)) {
        const Piece& f = flags[i];
        if (f.inBody && !f.attached && f.interactable && pieceContains(pieces[i], touch)) return kTongs;
    }
    for (HitAreaState& a : areas)
        if (wear(a, touch, 0, true, true)) return stage().tools.empty() ? kNoTool : stage().tools[0];
    return kNoTool;
}

HitResult Transplant::hitTest(const CCPoint& touch, ToolState* tool) {
    HitResult r;
    if (!takesTool(tool->type)) return r;
    if (tool->type == kTongs) {
        // a piece cut free, taken
        if (holdingPiece) {
            r.hit = true;
            r.injury = this;
            return r;
        }
        for (size_t i : piecesOf(stage().state)) {
            Piece& f = flags[i];
            if (!f.inBody || f.attached || !f.interactable) continue;
            cutThisFrame = pieceContains(pieces[i], touch);
            if (!cutThisFrame) continue;
            r.piece = &pieces[i];
            r.hit = true;
            if (!holdingPiece) {
                holdingPiece = true;
                if (!f.comboGiven) r.comboAction = true;
                else r.comboBonus = true;
            }
            f.comboGiven = true;
            r.injury = this;
            break;
        }
        return r;
    }
    // the chainsaw along its cuts
    bool allDone = true;
    for (HitAreaState& a : areas) {
        HitPointState* hp = wear(a, touch, tool->power, false, tool->type != kStapler);
        if (hp) {
            cutThisFrame = true;
            r.hit = true;
            r.injury = this;
            if (dust) {
                kit::addOnce(topNode, dust);
                if (!dust->isActive()) {
                    dust->resetSystem();
                    dust->setEmissionRate(800.0f);
                    dust->setDuration(0.7f);
                }
                r.useful = true;
                r.b1d = true;
                dust->setPosition(touch);
            }
        }
        const bool done = areaDone(a);
        if (done && hp) {
            // the pieces this cut frees: every cut they hang by done
            for (size_t i : piecesOf(stage().state)) {
                Piece& f = flags[i];
                if (!f.inBody || !f.interactable) continue;
                bool free = true;
                for (const HitArea& dep : pieces[i].def->dependencies) {
                    HitAreaState* da = areaNamed(dep.name);
                    if (!da || !da->done) free = false;
                }
                if (!free) continue;
                f.attached = false;
                // it slides off its place a little, away from the middle
                PieceState& p = pieces[i];
                const CCPoint off = ccp(position.x - (p.rect.origin.x + p.rect.size.width * 0.5f),
                                        position.y - (p.rect.origin.y + p.rect.size.height * 0.5f));
                CCPoint at = position;
                const int ox = static_cast<int>(off.x), oy = static_cast<int>(off.y);
                if (std::abs(ox) > slideBeyond) at.x += ox > 0 ? -static_cast<float>(slideBy) : static_cast<float>(slideBy);
                if (std::abs(oy) > slideBeyond) at.y += oy > 0 ? -static_cast<float>(slideBy) : static_cast<float>(slideBy);
                if (p.sprite) p.sprite->setPosition(at);
                kit::comboFor(*this, r);
                // (traced for test scripts: where to take it, its rect's
                // middle turned with the transplant, cocos points)
                const double a = -rotation * 0.017453292519943295;
                const float cx = -off.x, cy = -off.y;
                log("injury #%d piece %d free at %.0f,%.0f", soundChannel, static_cast<int>(i),
                    position.x + static_cast<float>(cx * std::cos(a) - cy * std::sin(a)),
                    position.y + static_cast<float>(cx * std::sin(a) + cy * std::cos(a)));
            }
        } else {
            allDone = allDone && done;
        }
    }
    if (stage().state == kFree) {
        if (cutThisFrame && allDone) allCutFree = true;
    } else if (cutThisFrame && allDone) {
        r.stageDone = true;
        kit::comboFor(*this, r);
        finishStage();
    }
    return r;
}

void Transplant::onFinished() {
    // (vf20, 1.45 Transplant::onHealed 0xb0aa4) a limb (its definition's
    // is_limb) or an organ replaced: stat 12 or 13 (StatsMgr.h)
    StatsMgr::sharedInstance()->incrementStat(def->isLimb ? StatTypes::kReplaceLimb : StatTypes::kReplaceOrgan);
}

void Transplant::completeStage() {
    // (vf18, the tutorial's and partners' doing) the old organ's stages go
    // all at once: cut round, cut free and off (its pieces hidden), the new
    // one in place; the rest as the base
    if (!hasStage()) return;
    const int s = stage().state;
    if (s == kFixed || s == kFree) {
        kit::markAreasDone(*this);
        if (s == kFree)
            for (size_t i : piecesOf(s))
                if (flags[i].interactable && pieces[i].sprite) pieces[i].sprite->setVisible(false);
        finishStage();
        if (!finished) completeStage();
        return;
    }
    if (s == kReplacing) {
        for (size_t i = 0; i < pieces.size() && i < flags.size(); ++i) {
            if (!pieces[i].def->isReplacement) continue;
            SoundMgr::sharedSoundMgr()->playSound("organ_placement", false);
            CCSprite* sp = CCSprite::create();
            pieces[i].sprite = sp;
            spritesNode->addChild(sp, pieces[i].def->zOrder);
            if (CCSpriteFrame* f = frameNamed(pieces[i].def->sprite)) sp->setDisplayFrame(f);
            sp->setColor(pieces[i].def->tint);
            flags[i].shown = true;
            sp->setVisible(true);
            sp->setRotation(rotation);
            sp->setPosition(position);
        }
        finishStage();
        return;
    }
    kit::markAreasDone(*this);
    finishStage();
}

}  // namespace mt
