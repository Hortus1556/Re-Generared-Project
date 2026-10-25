#include "as2/injuries/Graveyard.h"

#include <cstdlib>
#include <string>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/tools/ITool.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

Sprite* sprite(const std::string& image, int w, int h, int layer, Point offset) {
    Sprite* s = Sprite::initWithImageNamed(image, w, h);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    s->currentShape()->layer = layer;
    s->currentShape()->offset = offset;
    return s;
}

void removeSelf(InjuryGraphic* i) {
    retainRef(i);
    gameState->removeInjury(i);
    releaseRef(i);
}

}  // namespace

// ----------------------------------------------------------------------- Dirt

Dirt::Dirt(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 64;
    box_height = 64;
    make = "dirt";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Dirt"));
    Sprite* s = sprite("dirt.png", 128, 128, 36, {-64, -64});
    mSpriteArray.addObject(s);
    setup();
    // (Never put in the scene: the original made the picture but did not
    // show it. Shown, nothing could clear it off the coffin's line.)
    releaseRef(s);
}

void Dirt::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    if (currentStage <= maxStages) {
        setup();
        return;
    }
    alive = false;
    removeSelf(this);
}

// ------------------------------------------------------------- IncisionCoffin

IncisionCoffin::IncisionCoffin(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "incisionCoffin";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("IncisionCoffin"));
    // The cut in the lid, shown once sawn.
    Sprite* s = sprite("largecut.png", 64, 256, 38, {-32, 0});
    mSpriteArray.addObject(s);
    releaseRef(s);
    setup();
}

void IncisionCoffin::setup() {
    InjuryGraphic::setup();
    // Seven boxes down the lid, their guides shown.
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    if (!hitboxes) {
        hitBoxCount = 7;
        hitboxes = new HitBox[7];
    }
    for (int k = 0; k < hitBoxCount; ++k) {
        HitBox& b = hitboxes[k];
        b.rect = {{x - static_cast<float>(box_width / 2), y + static_cast<float>(box_height * k)},
                  {static_cast<float>(box_width), static_cast<float>(box_height)}};
        b.active = true;
        b.colour = 0x66ff33;
        b.health = 2;
        if (!b.sprite) {
            b.sprite = sprite("hitbox2_20x16.png", 20, 16, 44,
                              {static_cast<float>(-(box_width / 2)), static_cast<float>(box_height * k)});
        }
        Shape* g = b.sprite->currentShape();
        g->pos = {x, y};
        g->rotation = static_cast<float>(mInjury->rotation);
        scene().removeShape(g);
        scene().addShape(g);
    }
}

void IncisionCoffin::updateInjury() {
    ++currentStage;
    damageTimer = 0;
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    // Sawn: the cut shows.
    if (currentStage == 2 && stage(1).c28) {
        Shape* s = mSpriteArray[0]->currentShape();
        s->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
        s->rotation = static_cast<float>(mInjury->rotation);
        scene().addShape(s);
    }
    if (currentStage > maxStages) {
        alive = false;
        removeSelf(this);
        return;
    }
    for (int k = 0; k < hitBoxCount; ++k) scene().removeShape(hitboxes[k].sprite->currentShape());
    setup();
}

// ---------------------------------------------------------------------- Bleed

Bleed::Bleed(InjuryData* d, Injury* i, bool playerCreated, bool force) : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 138;
    box_height = 300;
    make = "bleed";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable("Bleed"));
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    // The body, the thumb (on its button, turned off it). (Its countdown is
    // the HUD's.)
    Sprite* s[] = {sprite("bleed1", 512, 512, 38, {-256, -256}), sprite("finger", 32, 32, 38, {-20, -25})};
    for (Sprite* p : s) mSpriteArray.addObject(p);
    setup();
    s[0]->currentShape()->pos = {x, y};
    s[1]->currentShape()->pos = {x - 170, y - 10};
    s[1]->currentShape()->rotation = -45;
    // (Last to first, as the original: on their shared layer the body goes
    // under the thumb.)
    for (int k = 1; k >= 0; --k) {
        scene().addShape(s[k]->currentShape());
        releaseRef(s[k]);
    }
}

void Bleed::setup() {
    aliveTimer = 0;
    injectionTimer = batteryTimer = syrupTimer = lighterTimer = tearsTimer = thumbTimer = 0;
    mouthHealth = 15;
    heartHealth = 15;
    bodyHealth = 25;
    // The body, its heart, its mouth, its arms.
    struct Box {
        float dx, dy;
        int w, h;
    };
    static const Box kBoxes[5] = {{0, 0, 138, 300}, {20, 92, 54, 54}, {0, 25, 100, 48}, {116, 28, 132, 92},
                                  {-116, 28, 132, 92}};
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    if (!hitboxes) {
        hitBoxCount = 5;
        hitboxes = new HitBox[5];
    }
    for (int k = 0; k < 5; ++k) {
        const Box& e = kBoxes[k];
        HitBox& b = hitboxes[k];
        float bx = x + e.dx, by = y + e.dy;
        b.rect = {{bx - static_cast<float>(e.w / 2), by - static_cast<float>(e.h / 2)},
                  {static_cast<float>(e.w), static_cast<float>(e.h)}};
        b.active = true;
        b.colour = 0x66ff33;
        b.health = 2;
        if (!b.sprite)
            b.sprite = sprite("hitbox_40x32.png", e.w, e.h, 31,
                              {static_cast<float>(-(e.w / 2)), static_cast<float>(-(e.h / 2))});
        b.sprite->currentShape()->pos = {bx, by};
        b.sprite->currentShape()->rotation = static_cast<float>(mInjury->rotation);
    }
    box_width = 132;
    box_height = 92;
}

void Bleed::checkThumbClick() {
    float dx = static_cast<float>(mInjury->x - 164) - gameState->mouseX;
    float dy = static_cast<float>(mInjury->y - 10) - gameState->mouseY;
    if (!(std::abs(static_cast<int>(dx)) < 20 && std::abs(static_cast<int>(dy)) < 20)) return;
    Shape* thumb = mSpriteArray[1]->currentShape();
    if (thumb->rotation != -45) return;
    // The thumb pressed down on its button.
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("BUTTON_SELECT");
    thumbTimer = maxThumbTimer;
    thumb->rotation = 0;
    gameState->createPopup("correct", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), this);
    trace("treated");
}

void Bleed::updateInjury() { treat(true); }

void Bleed::treat(bool popup) {
    if (popup)
        gameState->createPopup("correct", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), this);
    ITool* tool = gameState->currentTool;
    if (!tool) return;
    const std::string& type = tool->toolData->type;
    if (type == "Injection") {
        injectionTimer = maxInjectionTimer;
    } else if (type == "Battery") {
        batteryTimer = maxBatteryTimer;
    } else if (type == "Lighter") {
        lighterTimer = maxLighterTimer;
        bodyHealth = 25;
    } else if (type == "Cream") {
        syrupTimer = maxSyrupTimer;
        mouthHealth = 15;
    } else if (type == "Stitch") {
        tearsTimer = maxTearsTimer;
        heartHealth = 15;
    }
    trace("treated");
}

void Bleed::held(Part part) {
    int& health = part == kBody ? bodyHealth : part == kMouth ? mouthHealth : heartHealth;
    int box = part == kMouth ? 2 : part == kHeart ? 1 : -1;
    if (box >= 0 && !game->performHitTestOnHitBoxes(&hitboxes[box], 1)) return;
    // (Not held on it the frame before: a new hold.)
    if (heldFrame[part] < frame - 1) heldShown[part] = false;
    heldFrame[part] = frame;
    if (--health < 1) {
        treat(!heldShown[part]);
        heldShown[part] = true;
    }
}

void Bleed::render() {
    ++frame;
    // Each treatment wears off.
    auto wear = [](float& t) {
        if (t > 0) t -= 1;
        return t <= 0;
    };
    if (injectionTimer > 0) wear(injectionTimer);
    if (batteryTimer > 0) wear(batteryTimer);
    if (syrupTimer > 0 && wear(syrupTimer)) mouthHealth = 15;
    if (tearsTimer > 0 && wear(tearsTimer)) heartHealth = 15;
    if (lighterTimer > 0 && wear(lighterTimer)) bodyHealth = 25;
    if (thumbTimer > 0 && wear(thumbTimer)) mSpriteArray[1]->currentShape()->rotation = -45;
    // The heart at 30 or more, the counter runs down; at 0 it lives.
    if (gameState->health < 30) {
        aliveTimer = 0;
    } else {
        if (aliveTimer == 0) aliveTimer = maxAliveTimer;
        if (--aliveTimer < 1) {
            alive = false;
            invincible = true;
            ++currentStage;
            removeSelf(this);
            return;
        }
    }
    // The heart: what each treatment still gives (up to 10 each).
    float h = injectionTimer / maxInjectionTimer * 100.0f / 10.0f + lighterTimer / maxLighterTimer * 100.0f / 10.0f +
              syrupTimer / maxSyrupTimer * 100.0f / 10.0f + batteryTimer / maxBatteryTimer * 100.0f / 10.0f +
              (thumbTimer > 0 ? 10.0f : 0.0f) + tearsTimer / maxTearsTimer * 100.0f / 10.0f;
    gameState->health = static_cast<float>(static_cast<int>(h));
    int bpm = static_cast<int>(gameState->health);
    const char* look = bpm < 7 ? "bleed1" : bpm < 14 ? "bleed2" : bpm < 22 ? "bleed3" : bpm < 31 && bpm != 30 ? "bleed4" : "bleed5";
    mSpriteArray[0]->currentShape()->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(look);
}

std::string Bleed::toolNow() const {
    // The treatment worn off most.
    struct T {
        const char* tool;
        float left;
    } t[] = {{"injection", injectionTimer / maxInjectionTimer}, {"battery", batteryTimer / maxBatteryTimer},
             {"lighter", lighterTimer / maxLighterTimer},         {"cream", syrupTimer / maxSyrupTimer},
             {"stitch", tearsTimer / maxTearsTimer},              {"tap", thumbTimer > 0 ? 1.0f : 0.0f}};
    const T* best = &t[0];
    for (const T& e : t)
        if (e.left < best->left) best = &e;
    return best->tool;
}

void Bleed::traceBoxes() const {
    std::string tool = toolNow();
    if (tool == "tap") {
        // The thumb's button.
        Point p = gameState->worldToScreen({static_cast<float>(mInjury->x - 164), static_cast<float>(mInjury->y - 10)});
        platform::debugTrace("  box %.0f,%.0f 2.0", p.x, p.y);
        return;
    }
    // Where its treatment goes: the cream on its mouth, the tears on its
    // heart, the rest anywhere on it (its body's box).
    const HitBox& b = hitboxes[tool == "cream" ? 2 : tool == "stitch" ? 1 : 0];
    Point p = gameState->worldToScreen({b.rect.origin.x + b.rect.size.width * 0.5f, b.rect.origin.y + b.rect.size.height * 0.5f});
    platform::debugTrace("  box %.0f,%.0f 2.0", p.x, p.y);
}

}  // namespace as2
