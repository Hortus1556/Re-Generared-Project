#include "as2/tools/Stitch.h"

#include <cmath>
#include <cstdlib>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/Graveyard.h"
#include "as2/injuries/InjuryGraphic.h"

namespace as2 {

Stitch::Stitch(ToolData* d) : ITool(d) {
    getSingleInjury = false;
    damage = 2;
    setToolLevel();
    mStitches = new Point[2048]();
    mStitchCount = 0;
}

Stitch::~Stitch() {
    clearSewn();
    delete[] mStitches;
}

void Stitch::clearSewn() {
    for (Sewn& s : mInjuries) releaseRef(s.injury);
    mInjuries.clear();
}

void Stitch::setToolLevel() {
    level = SaveDetails::GetToolLevel("Stitch");
    if (level == 1) mNeeded = 4;
    else if (level == 2) mNeeded = 3;
    else if (level == 3) mNeeded = 2;
}

void Stitch::turnOn() {
    if (turnedOn) return;
    if (Connector::sharedConnector().current_mission_id == 18)
        for (InjuryGraphic* i : gameState->_injuries)
            if (i->isAttacking()) return;
    ITool::turnOn();
    Array<InjuryGraphic>& under = gameState->currentInjuries();
    if (under.count() != 0 && under[0]->anything) under[0]->interact();
}

void Stitch::turnOff(bool release) {
    ITool::turnOff(release);
    clearSewn();
    mStitchCount = 0;
}

void Stitch::wrongUsage() {
    ITool::wrongUsage();
    turnOff(false);
    if (!gameState->inShock) gameState->health = gameState->health - damage;
}

bool Stitch::finishedStitching(const HitBox* boxes, int count, const std::vector<Point>& stitches,
                               const InjuryGraphic* injury, int needed) {
    int done = 0;
    for (int b = 0; b < count; ++b) {
        const Rect& r = boxes[b].rect;
        for (const Point& p : stitches)
            if (r.origin.x <= p.x && p.x <= r.origin.x + r.size.width && r.origin.y <= p.y &&
                p.y <= r.origin.y + r.size.height) {
                ++done;
                if (!injury->isKindOf("Snake")) break;
            }
    }
    return needed <= done;
}

void Stitch::render(int) {
    if (Connector::sharedConnector().current_mission_id == 19) {
        // The graveyard's phial: its tears poured, held on the risen
        // patient's heart.
        MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXTRACTION1");
        int x = static_cast<int>(gameState->mouseX), y = static_cast<int>(gameState->mouseY);
        // (Along the stroke too: a drop every 5 points, a spray every third.)
        int made = trail(static_cast<float>(x), static_cast<float>(y), 5, [this](float tx, float ty, float at) {
            game->addTrailEffect("CreamEffect", static_cast<int>(tx), static_cast<int>(ty), 0.375f, 0.9f, 0.98f, at);
            if (++trailSprays % 3 == 0)
                game->addTrailEffect("CreamSpray", static_cast<int>(tx), static_cast<int>(ty), 0.375f, 0.9f, 0.98f, at);
        });
        if (!made) game->addEffect("CreamEffect", x, y, 0.375f, 0.9f, 0.98f);
        if (maxEffectTimer <= ++effectTimer) {
            effectTimer = 0;
            if (!made) game->addEffect("CreamSpray", x, y, 0.375f, 0.9f, 0.98f);
        }
        for (InjuryGraphic* i : game->findCorrectInjury(getSingleInjury))
            if (auto* b = dynamic_cast<Bleed*>(i)) b->held(Bleed::kHeart);
        return;
    }
    game->findCorrectInjury(getSingleInjury);
    Array<InjuryGraphic>& under = gameState->currentInjuries();
    // Each injury newly under the needle is followed from now on.
    for (InjuryGraphic* i : under) {
        if (i->invincible) continue;
        bool known = false;
        for (const Sewn& s : mInjuries) known = known || s.injury == i;
        if (!known) mInjuries.push_back({retainRef(i), {}, 0, 0, 0});
    }
    for (size_t n = 0; n < mInjuries.size(); ++n) {
        Sewn& s = mInjuries[n];
        InjuryGraphic* inj = s.injury;
        if (!inj->hitboxes || inj->hitBoxCount == 0) continue;
        const HitBox& first = inj->hitboxes[0];
        // The finger in the frame of the cut (its first box's guide).
        const Shape* g = first.sprite->currentShape();
        float dx = gameState->mouseX - g->pos.x, dy = gameState->mouseY - g->pos.y;
        double a = static_cast<float>(-g->rotation * 3.141592653589793 / 180.0);
        double c = std::cos(a), sn = std::sin(a);
        float px = static_cast<float>(dx * c - dy * sn) + g->pos.x;
        float py = static_cast<float>(c * dy + dx * sn) + g->pos.y;
        float x0 = static_cast<float>(inj->mInjury->x), y0 = static_cast<float>(inj->mInjury->y);
        // Along the cut (down from its start), the finger crossing its
        // middle line makes a stitch there. A snake's hole is sewn across
        // its body instead: along from its middle to the right, crossing
        // the level line.
        bool across = inj->isKindOf("Snake");
        float al = across ? px : py, ac = across ? py : px;        // along, across
        float pal = across ? s.x : s.y, pac = across ? s.y : s.x;  // the last frame's
        float al0 = across ? x0 : y0, ac0 = across ? y0 : x0;
        float length = (across ? first.rect.size.width : first.rect.size.height) * static_cast<float>(inj->hitBoxCount);
        if (al0 <= al && al < al0 + length && ac != ac0) {
            int side = ac < ac0 ? -1 : 1;
            bool crossed = s.side == -side;
            s.side = side;
            if (crossed) {
                float cal = al;
                float dac = ac - pac;
                if (dac != 0) {
                    float slope = (al - pal) / dac;
                    if (slope != 0) cal = static_cast<float>(std::abs(static_cast<int>(ac - pac))) * slope + al;
                }
                s.stitches.push_back(across ? Point{cal, ac0} : Point{ac0, cal});
                if (finishedStitching(inj->hitboxes, inj->hitBoxCount, s.stitches, inj, mNeeded)) {
                    MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXTRACTION3");
                    retainRef(inj);
                    inj->updateInjury();
                    inj->trace("treated");
                    releaseRef(inj);
                    turnOff(false);
                    return;
                }
            }
        }
        s.x = px;
        s.y = py;
    }
}

}  // namespace as2
