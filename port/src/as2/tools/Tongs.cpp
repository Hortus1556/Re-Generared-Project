#include "as2/tools/Tongs.h"

#include <cmath>
#include <cstdlib>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/Extraction.h"
#include "as2/injuries/Eyelid.h"
#include "as2/injuries/Transplant.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
int missionId() { return Connector::sharedConnector().current_mission_id; }
Point finger() { return {gameState->mouseX, gameState->mouseY}; }

// The finger, in a box's frame (turned back by its guide's turn).
Point inFrame(const Shape* g) {
    float dx = gameState->mouseX - g->pos.x, dy = gameState->mouseY - g->pos.y;
    double a = static_cast<float>(-g->rotation * 3.141592653589793 / 180.0);
    double c = std::cos(a), s = std::sin(a);
    return {static_cast<float>(dx * c - dy * s) + g->pos.x, g->pos.y + static_cast<float>(c * dy + dx * s)};
}

}  // namespace

Tongs::Tongs(ToolData* d) : ITool(d) {
    getSingleInjury = true;
    setToolLevel();
    damage = 2;
    mTray = Sprite::initWithImageNamed("tray.png", 256, 128);
    mTray->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* t = mTray->currentShape();
    t->layer = 29;
    t->offset.x = -76;
    t->pos = {128, 240};
    t->rotation = 0;
    maxLowEffectTimer = 10;
}

Tongs::~Tongs() {
    clearHolding();
    removeTrayHolding();
    releaseRef(injury);
    releaseRef(mTray);
}

void Tongs::dropHeld() {
    releaseRef(injury);
    injury = nullptr;
    clearHolding();
    mouseArray.clear();
    doTurnOff(false);
}

void Tongs::clearHolding() {
    releaseRef(holdingInjury);
    holdingInjury = nullptr;
    holdingIndex = 0;
    holdingFromTray = false;
}

bool Tongs::takenWhole(const InjuryGraphic* i) {
    // A dead snake, a skull's piece: picked up whole (its stage moves on),
    // then dropped on the tray.
    return i->isKindOf("Snake") || i->isKindOf("SkullCut");
}

DraggableWrapper* Tongs::removableOf(InjuryGraphic* i, int index) {
    if (!i || index < 0) return nullptr;
    if (auto* t = dynamic_cast<Transplant*>(i))
        return index < static_cast<int>(t->parts.size()) ? t->parts[static_cast<size_t>(index)].removable : nullptr;
    return index < i->removable.count() ? i->removable[index] : nullptr;
}

void Tongs::setTrayHolding(InjuryGraphic* owner, DraggableWrapper* w) {
    retainRef(owner);
    retainRef(w);
    removeTrayHolding();
    if (!owner || !w || !w->healthyReplacementSprite) {
        releaseRef(owner);
        releaseRef(w);
        return;
    }
    // The healthy one on the tray, its box with it.
    Shape* h = w->healthyReplacementSprite->currentShape();
    Shape* tray = mTray->currentShape();
    bool pin = owner->isKindOf("Insertion");
    h->pos = {tray->pos.x, tray->pos.y + (pin ? 128.0f : 64.0f)};
    if (!pin && !owner->isKindOf("Transistor") && w->hitBoxCount > 0) {
        HitBox& b = w->hitboxes[0];
        b.rect.origin = h->pos;
        if (b.sprite) b.sprite->currentShape()->pos = h->pos;
    }
    w->mSprite = w->healthyReplacementSprite;
    scene().addShape(h);
    trayInjury = owner;
    trayDraggable = w;
    if (platform::traceEnabled()) {
        // "tray <class> at <x>,<y> to <x>,<y>": where it waits and goes
        // (screen points).
        float size = 0;
        Point a = gameState->worldToScreen(h->pos), b = gameState->worldToScreen(trayTarget(owner, &size));
        platform::debugTrace("tray %s at %.0f,%.0f to %.0f,%.0f", owner->mInjuryData->type.c_str(), a.x, a.y, b.x, b.y);
    }
}

Point Tongs::trayTarget(InjuryGraphic* owner, float* size) {
    // Its place, and how near it must be let go.
    float tx = static_cast<float>(owner->mInjury->x), ty = static_cast<float>(owner->mInjury->y);
    *size = 16;
    if (owner->isKindOf("Insertion") || owner->isKindOf("Transistor")) {
        // 73 points down the cut.
        double r = static_cast<float>(owner->mInjury->rotation * 3.141592653589793 / 180.0);
        tx = static_cast<float>(tx + (std::cos(r) * 0.0 - std::sin(r) * 73.0));
        ty = static_cast<float>(ty + std::cos(r) * 73.0 + std::sin(r) * 0.0);
        *size = 32;
    } else if (owner->isKindOf("KidneyTransplantLeft") || owner->isKindOf("PoisonKidneyTransplant")) {
        ty -= 30;
        *size = 48;
    } else if (owner->isKindOf("TypeAndTell")) {
        *size = 64;
    }
    return {tx, ty};
}

void Tongs::setupInsertion(InjuryGraphic* insertion) {
    setTrayHolding(insertion, removableOf(insertion, 0));
    addTray();
}

void Tongs::removeTrayHolding() {
    if (trayInjury) platform::debugTrace("tray empty");
    releaseRef(trayInjury);
    releaseRef(trayDraggable);
    trayInjury = nullptr;
    trayDraggable = nullptr;
}

void Tongs::setHoldingFromTray() {
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("DROP");
    clearHolding();
    holdingInjury = retainRef(trayInjury);
    holdingFromTray = true;
    draggable = trayDraggable;
    if (draggable && draggable->mSprite) draggable->mSprite->currentShape()->pos = finger();
}

void Tongs::setToolLevel() {
    level = SaveDetails::GetToolLevel("Tongs");
    if (level == 1) {
        angleBuffer = 0;
        removeBuffer = 0;
        grip = 0;
    } else if (level == 2) {
        angleBuffer = 15;
        removeBuffer = 10;
        grip = 2;
    } else if (level == 3) {
        angleBuffer = 30;
        removeBuffer = 20;
        grip = 4;
    }
}

void Tongs::wrongUsage() {
    ITool::wrongUsage();
    turnOff(false);
    if (!gameState->inShock) gameState->health = gameState->health - damage;
}

void Tongs::droppedPopup() {
    gameState->createPopup("dropped", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), nullptr);
}

bool Tongs::hitTest(const HitBox* boxes, int count) const {
    for (int i = 0; i < count; ++i) {
        const HitBox& b = boxes[i];
        if (!b.sprite) continue;
        Point p = inFrame(b.sprite->currentShape());
        if (b.rect.origin.x <= p.x && p.x <= b.rect.origin.x + b.rect.size.width && b.rect.origin.y <= p.y &&
            p.y <= b.rect.origin.y + b.rect.size.height)
            return true;
    }
    return false;
}

void Tongs::turnOn() {
    if (turnedOn || mDoTurnOn) return;
    Array<InjuryGraphic>& under = gameState->currentInjuries();
    if (under.count() != 0 && under[0]->anything) {
        under[0]->interact();
        return;
    }
    if (missionId() == 18)
        for (InjuryGraphic* i : gameState->_injuries)
            if (i->isAttacking()) return;
    // Not in the graveyard (its hands dig instead).
    if (missionId() != 19) {
        mDoTurnOn = true;
        ITool::turnOn();
    }
}

void Tongs::turnOff(bool release) {
    mRelease = release;
    mDoTurnOff = true;
}

void Tongs::addTray() {
    mTray->currentShape()->layer = 29;
    scene().addShape(mTray->currentShape());
}

void Tongs::removeTray() { scene().removeShape(mTray->currentShape()); }

void Tongs::doTurnOn() {
    mDoTurnOn = false;
    movedDist = 0;
    releaseRef(injury);
    injury = nullptr;
    draggable = nullptr;
    clearHolding();
    mouseArray.assign(4, finger());
    // Pressed on the tray with a replacement on it: taken from there.
    float ux = gameState->unscaledMouseX, uy = gameState->unscaledMouseY;
    // (An insertion's pin: anywhere on the tray.)
    bool pin = trayInjury && (trayInjury->isKindOf("Insertion") || trayInjury->isKindOf("Transistor"));
    if (trayInjury && 64 < ux && ux < 192 && uy > (pin ? 103 : 220)) setHoldingFromTray();
}

void Tongs::setHolding(int index) {
    clearHolding();
    holdingInjury = retainRef(injury);
    holdingIndex = index;
    addTray();
    draggable = removableOf(injury, index);
    if (draggable && draggable->mSprite) draggable->mSprite->currentShape()->pos = finger();
    if (injury->isKindOf("Canister")) {
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("Gas_removed");
    } else {
        int n = static_cast<int>(static_cast<float>(std::rand() % 100) / 100.0f * 3.0f + 1.0f);
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXTRACTION" + std::to_string(n));
    }
}

void Tongs::render(int) {
    if (mDoTurnOn) {
        doTurnOn();
        return;
    }
    if (mDoTurnOff) {
        doTurnOff(mRelease);
        return;
    }
    // A replacement waiting on the tray: nothing else is taken until it is
    // (the original's; without it a second rib could go to the tray too).
    if (trayInjury && !holding()) return;
    if (holding()) {
        if (!draggable || !draggable->mSprite) return;
        Shape* s = draggable->mSprite->currentShape();
        if (!holdingInjury->slippery) {
            // Held: it follows the finger (drawn under the cursor where the
            // pointer is at each display frame).
            s->pos = finger();
            scene().onPointer = s;
            return;
        }
        // Slippery: it slides after the finger by no more than the grip,
        // and slips out when the finger leaves it.
        float dx = gameState->mouseX - s->pos.x, dy = gameState->mouseY - s->pos.y;
        // (atan2 of x over y: the step's sine is its x part.)
        double a = std::atan2(static_cast<double>(dx), static_cast<double>(dy));
        float d = std::sqrt(dx * dx + dy * dy);
        if (d <= 3) d = 0;
        int step = grip + 3;
        if (d < static_cast<float>(step)) step = static_cast<int>(d);
        if (d == 0) step = 0;
        a = static_cast<float>(a * 180.0 / 3.141592653589793) * 3.141592653589793 / 180.0;
        float mx = static_cast<float>(std::sin(a) * step), my = static_cast<float>(step * std::cos(a));
        s->pos.x += mx;
        s->pos.y += my;
        for (int i = 0; i < draggable->hitBoxCount; ++i) {
            HitBox& b = draggable->hitboxes[i];
            if (b.sprite) {
                b.sprite->currentShape()->pos.x += mx;
                b.sprite->currentShape()->pos.y += my;
            }
            b.rect.origin.x += mx;
            b.rect.origin.y += my;
        }
        if (!hitTest(draggable->hitboxes, draggable->hitBoxCount)) turnOff(false);
        return;
    }
    game->findCorrectInjury(getSingleInjury);
    InjuryGraphic* stick = nullptr;
    if (injury) {
        // Taking one: the finger must stay on it.
        if (injury->anything) return;
        if (dynamic_cast<Extraction*>(injury)) {
            // A thing to lift away: taken at once.
            if (!hitTest(injury->hitboxes, injury->hitBoxCount)) {
                wrongUsage();
                if (holding()) droppedPopup();
                return;
            }
            injury->beingHeld = true;
            setHolding(0);
            gameState->createPopup("correct", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY),
                                   injury);
            injury->trace("taken");
            return;
        }
        if (auto* t = dynamic_cast<Transplant*>(injury)) {
            // A transplant's piece cut loose under it: taken.
            for (size_t p = 0; p < t->parts.size(); ++p) {
                DraggableWrapper* w = t->parts[p].removable;
                if (!w || !hitTest(w->hitboxes, w->hitBoxCount)) continue;
                // (The old brain out, however quickly it was taken: the
                // heart runs down.)
                if (injury->isKindOf("BrainTransplant") && injury->currentStage == 1) gameState->loseBrain();
                setHolding(static_cast<int>(p));
                InjuryGraphic* taken = retainRef(injury);
                taken->updateInjury();
                taken->trace("taken");
                releaseRef(taken);
                return;
            }
            wrongUsage();
            if (holding()) droppedPopup();
            return;
        }
        if (takenWhole(injury)) {
            DraggableWrapper* w = removableOf(injury, 0);
            if (!w || !hitTest(w->hitboxes, w->hitBoxCount)) {
                wrongUsage();
                if (holding()) droppedPopup();
                return;
            }
            setHolding(0);
            InjuryGraphic* taken = retainRef(injury);
            taken->updateInjury();
            taken->trace("taken");
            releaseRef(taken);
            return;
        }
        // An eyelid is held by its own box; the rest by their object's.
        bool lid = dynamic_cast<Eyelid*>(injury) != nullptr;
        DraggableWrapper* w = injury->removable.count() ? injury->removable[0] : nullptr;
        bool on = lid ? hitTest(injury->hitboxes, injury->hitBoxCount) : w && hitTest(w->hitboxes, w->hitBoxCount);
        if (!on) {
            wrongUsage();
            if (holding()) droppedPopup();
            return;
        }
        stick = injury;
    }
    Array<InjuryGraphic>& under = gameState->currentInjuries();
    for (int n = 0; n < under.count(); ++n) {
        InjuryGraphic* found = under[n];
        if (found->anything) {
            found->interact();
            return;
        }
        if (stick && found != stick) found = stick;
        if (injury != found) {
            retainRef(found);
            releaseRef(injury);
            injury = found;
        }
        if (mouseArray.empty()) mouseArray.assign(4, finger());
        if (dynamic_cast<Extraction*>(injury)) {
            // Pressed on it: lifted over the rest (taken next frame).
            Shape* sh = injury->mSpriteArray[0]->currentShape();
            if (sh->layer == 28) return;
            scene().removeShape(sh);
            sh->layer = 28;
            scene().addShape(sh);
            return;
        }
        if (auto* t = dynamic_cast<Transplant*>(injury)) {
            // Pressed on a transplant's loose piece: it is lifted (taken
            // next frame).
            for (Transplant::Part& p : t->parts) {
                DraggableWrapper* w = p.removable;
                if (!w || !w->mSprite || !hitTest(w->hitboxes, w->hitBoxCount)) continue;
                Shape* sh = w->mSprite->currentShape();
                scene().removeShape(sh);
                sh->layer = 28;
                scene().addShape(sh);
                // The old brain lifted out: the heart runs down (the new
                // one, in, restarts it with the battery).
                if (injury->isKindOf("BrainTransplant")) gameState->loseBrain();
                return;
            }
            continue;
        }
        if (takenWhole(injury)) {
            // Pressed on it: lifted over the rest (taken next frame).
            DraggableWrapper* w = removableOf(injury, 0);
            if (!w || !w->mSprite || !hitTest(w->hitboxes, w->hitBoxCount)) continue;
            Shape* sh = w->mSprite->currentShape();
            scene().removeShape(sh);
            sh->layer = 28;
            scene().addShape(sh);
            return;
        }
        if (auto* lid = dynamic_cast<Eyelid*>(injury)) {
            // An eyelid: drawn open away from the eye, along its turn.
            if (!injury->beingHeld) injury->extendHitBox();
            injury->beingHeld = true;
            Point prev = mouseArray.back(), cur = finger();
            if (prev.x == cur.x && prev.y == cur.y) continue;
            mouseArray.push_back(cur);
            float dist = std::sqrt((cur.x - prev.x) * (cur.x - prev.x) + (cur.y - prev.y) * (cur.y - prev.y));
            float angle = static_cast<float>(std::atan2(static_cast<double>(cur.y - prev.y),
                                                        static_cast<double>(cur.x - prev.x)) *
                                             180.0 / 3.141592653589793) + 90;
            float buffer = lid->angleBuffer + angleBuffer;
            float lo = static_cast<float>(injury->mInjury->rotation) - buffer;
            float hi = static_cast<float>(injury->mInjury->rotation) + buffer;
            bool along = (lo < angle && angle < hi) || (lo < angle + 360 && angle + 360 < hi);
            if (!along) {
                if (dist < 6) continue;
                wrongUsage();
                if (holding()) droppedPopup();
                return;
            }
            if (!lid->pull(dist)) continue;
            // Open: treated, and let go.
            InjuryGraphic* taken = injury;
            injury = nullptr;
            taken->updateInjury();
            taken->trace("treated");
            releaseRef(taken);
            doTurnOff(false);
            MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXTRACTION4");
            return;
        }
        auto* se = dynamic_cast<SurfaceExtraction*>(injury);
        DraggableWrapper* w = injury->removable.count() ? injury->removable[0] : nullptr;
        if (!se || !w || !hitTest(w->hitboxes, w->hitBoxCount)) {
            // (Other kinds the tongs take are not ported yet.)
            wrongUsage();
            if (holding()) droppedPopup();
            return;
        }
        if (!injury->beingHeld) injury->extendHitBox();
        injury->beingHeld = true;
        // The clown's scarf: pulled on from where it has slipped back to.
        bool clown = injury->isKindOf("UnderSurfaceExtractionClown");
        if (clown) movedDist = se->moved;
        Point prev = mouseArray.back(), cur = finger();
        if (prev.x == cur.x && prev.y == cur.y) continue;
        mouseArray.push_back(cur);
        float dist = std::sqrt((cur.x - prev.x) * (cur.x - prev.x) + (cur.y - prev.y) * (cur.y - prev.y));
        // The pull's direction over the last few moves, against the
        // object's own.
        Point old = mouseArray[mouseArray.size() - 5];
        float angle = static_cast<float>(std::atan2(static_cast<double>(cur.y - old.y), static_cast<double>(cur.x - old.x)) *
                                         180.0 / 3.141592653589793) + 90;
        float buffer = static_cast<float>(se->angleBuffer) + angleBuffer;
        float lo = static_cast<float>(static_cast<int>(static_cast<float>(injury->mInjury->rotation + 90) - buffer));
        float hi = static_cast<float>(static_cast<int>(static_cast<float>(injury->mInjury->rotation + 90) + buffer));
        bool along = (lo < angle && angle < hi) || (lo < angle + 360 && angle + 360 < hi);
        if (!along) {
            if (dist < 6) continue;
            wrongUsage();
            if (holding()) droppedPopup();
            return;
        }
        movedDist += dist;
        if (clown) se->moved += dist;
        if (static_cast<float>(removeBuffer) + movedDist >= static_cast<float>(se->removableLength) && clown) {
            // All of it out: nothing to hold.
            scene().removeShape(w->mSprite->currentShape());
            turnOff(true);
            InjuryGraphic* taken = retainRef(injury);
            taken->updateInjury();
            taken->trace("treated");
            releaseRef(taken);
        } else if (static_cast<float>(removeBuffer) + movedDist >= static_cast<float>(se->removableLength)) {
            // Out: held under the finger, over everything but the HUD.
            setHolding(0);
            Shape* s = w->mSprite->currentShape();
            scene().removeShape(s);
            grabOrigin = cur;
            se->setExtractionLength(static_cast<float>(se->spriteLength - 56));
            s->rotation = 0;
            s->offset = {-64, -32};
            s->layer = 28;
            s->pos = cur;
            scene().addShape(s);
            InjuryGraphic* taken = retainRef(injury);
            taken->updateInjury();
            taken->trace("treated");
            releaseRef(taken);
        } else {
            se->setExtractionLength(movedDist);
        }
    }
}

void Tongs::doTurnOff(bool release) {
    InjuryGraphic* hatching = nullptr;  // a dropped egg, to hatch once let go
    mDoTurnOff = false;
    if (injury) MyOpenAL::sharedMyOpenAL().playSFXWithKey("SPLAT1");
    ITool::turnOff(release);
    if (injury && injury->anything) return;
    bool clown = injury && injury->isKindOf("UnderSurfaceExtractionClown");
    if (clown && release) {
        injury->beingHeld = false;
        return;
    }
    if (!clown) {
        if (injury && injury->alive && !holding()) {
            // Let go before it came out: it goes back.
            ITool::wrongUsage();
            if (!gameState->inShock) gameState->health = gameState->health - damage;
            droppedPopup();
            injury->setup();
        }
    } else {
        injury->beingHeld = false;
    }
    if (holding() && holdingFromTray) {
        // The replacement from the tray: let go on its place (within 16
        // points) it goes in; anywhere else it is dropped back on the tray.
        InjuryGraphic* owner = retainRef(trayInjury);
        DraggableWrapper* w = retainRef(trayDraggable);
        Shape* sh = w && w->mSprite ? w->mSprite->currentShape() : nullptr;
        if (owner && sh) {
            scene().removeShape(sh);
            sh->pos = finger();
            float size = 0;
            Point target = trayTarget(owner, &size);
            float tx = target.x, ty = target.y;
            if (tx - size < sh->pos.x && sh->pos.x < tx + size && ty - size < sh->pos.y && sh->pos.y < ty + size) {
                if (!owner->isKindOf("BrainPill")) sh->pos = {tx, ty};
                if (owner->isKindOf("Insertion")) sh->layer = 37;
                else if (owner->isKindOf("AmpPlug")) sh->layer = 39;
                else if (owner->isKindOf("TypeAndTell")) sh->layer = 43;
                else if (auto* t = dynamic_cast<Transplant*>(owner)) sh->layer = t->inLayer;
                scene().addShape(sh);
                if (owner->isKindOf("Canister")) {
                    MyOpenAL::sharedMyOpenAL().playSFXWithKey("Gas_fitted");
                } else {
                    int n = static_cast<int>(static_cast<float>(std::rand() % 100) / 100.0f * 3.0f + 1.0f);
                    MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXTRACTION" + std::to_string(n));
                }
                removeTrayHolding();
                owner->updateInjury();
                owner->trace("treated");
            } else {
                // (A gas canister let go anywhere else blows up.)
                if (owner->isKindOf("Canister")) owner->interact();
                ITool::wrongUsage();
                if (!gameState->inShock) gameState->health = gameState->health - damage;
                droppedPopup();
                setTrayHolding(owner, w);
            }
        }
        releaseRef(w);
        releaseRef(owner);
    } else if (holding()) {
        InjuryGraphic* held = holdingInjury;
        retainRef(held);
        float ux = gameState->unscaledMouseX, uy = gameState->unscaledMouseY;
        if (64 < ux && ux < 192 && uy > 220) {
            // Over the tray: done with.
            if (draggable && draggable->mSprite) scene().removeShape(draggable->mSprite->currentShape());
            MyOpenAL::sharedMyOpenAL().playSFXWithKey("DROP");
            auto* t = dynamic_cast<Transplant*>(held);
            if (t) t->removeRemovable(holdingIndex);
            held->updateInjury();
            // A transplant's healthy piece takes its place on the tray.
            if (t && held->currentStage == 2 && !held->isKindOf("SkullCut"))
                setTrayHolding(held, removableOf(held, holdingIndex));
            held->trace("treated");
        } else {
            // Dropped elsewhere: a wrong use; it goes back in.
            ITool::wrongUsage();
            if (!gameState->inShock) gameState->health = gameState->health - damage;
            droppedPopup();
            if (held->isKindOf("UnderSurfaceExtraction") || held->isKindOf("UnderSurfaceExtractionSilver")) {
                if (held->currentStage == 3) held->currentStage = 2;
            } else if (held->isKindOf("SurfaceExtraction")) {
                if (held->currentStage == 2) held->currentStage = 1;
            } else if (takenWhole(held) && held->currentStage == 3) {
                // Put back where it was, to be taken again.
                held->currentStage = 2;
                if (draggable && draggable->mSprite) {
                    Shape* sh = draggable->mSprite->currentShape();
                    sh->pos = {draggable->xOrigin, draggable->yOrigin};
                    scene().removeShape(sh);
                    sh->layer = 32;
                    scene().addShape(sh);
                }
            }
            if (held->isKindOf("FireFlyEgg")) {
                // An egg let fall breaks: it hatches where it falls (its
                // firefly, shell and fire), once the tongs have let go.
                held->beingHeld = false;
                held->mInjury->x = static_cast<int>(gameState->mouseX);
                held->mInjury->y = static_cast<int>(gameState->mouseY);
                if (draggable && draggable->mSprite) scene().removeShape(draggable->mSprite->currentShape());
                hatching = retainRef(held);
            }
            InjuryGraphic* owner = hatching ? nullptr : draggable ? draggable->injury : held;
            if (owner) owner->setup();
            if (dynamic_cast<Extraction*>(held) && !hatching) {
                // Put back down (setup took it back to its place).
                Shape* sh = held->mSpriteArray[0]->currentShape();
                if (sh->layer == 28) {
                    scene().removeShape(sh);
                    sh->layer = 38;
                    scene().addShape(sh);
                }
            }
            if (dynamic_cast<Transplant*>(held) && draggable && draggable->mSprite) {
                // A transplant's piece goes back where it was, a little off.
                float rx = static_cast<float>(std::rand() % 10 + 5), ry = static_cast<float>(std::rand() % 10 + 5);
                if (held->isKindOf("LeftThumbTransplant")) rx = -rx;
                if (held->isKindOf("LeftThumbTransplant") || held->isKindOf("RightThumbTransplant")) ry = -ry;
                Shape* sh = draggable->mSprite->currentShape();
                sh->pos = {draggable->xOrigin + rx, draggable->yOrigin + ry};
                scene().removeShape(sh);
                sh->layer = held->isKindOf("PoisonLiverTransplant") ? 40 : 28;
                scene().addShape(sh);
            }
            held->trace("dropped");
        }
        releaseRef(held);
    }
    draggable = nullptr;
    releaseRef(injury);
    injury = nullptr;
    clearHolding();
    mouseArray.clear();
    // The tray stays while a replacement waits on it.
    if (!trayInjury) removeTray();
    movedDist = 0;
    if (hatching) {
        hatching->special();
        releaseRef(hatching);
    }
}

}  // namespace as2
