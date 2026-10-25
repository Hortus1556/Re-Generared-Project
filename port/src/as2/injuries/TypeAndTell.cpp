#include "as2/injuries/TypeAndTell.h"

#include <cstdlib>

#include "as2/GameController.h"
#include "as2/GameData.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
float percent() { return static_cast<float>(std::rand() % 100) / 100.0f; }

void addAt(int id, int x, int y) {
    Injury* n = new Injury;
    n->mID = id;
    n->x = x;
    n->y = y;
    n->rotation = 0;
    n->scale = 1;
    gameState->addInjury(n, false, false);
    releaseRef(n);
}

// The keys: three rows of eight and "+ - y z = #" under them, 25 points
// apart, the first ("a") 87.5 left of and 8 below the device's middle.
Point keyAt(int k) {
    int row = k / 8, col = k % 8;
    if (row == 3) col += 1;
    return {-87.5f + 25.0f * static_cast<float>(col), 8.0f + 25.0f * static_cast<float>(row)};
}

}  // namespace

TypeAndTell::TypeAndTell(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : BoneTransplant(d, i, playerCreated, force) {
    // Six stages: the transplant's four, the battery, the words.
    maxStages = 6;
    initStageArray(stageTable("TypeAndTell"));
}

TypeAndTell::~TypeAndTell() {
    clearWord();
    if (ownHits) freeHitBoxes();
    ownHits = false;
}

std::string TypeAndTell::toolNow() const {
    // (The keys take any tool.)
    return BoneTransplant::toolNow();
}

void TypeAndTell::traceBoxes() const {
    if (ownHits) InjuryGraphic::traceBoxes(hitboxes, hitBoxCount, "");
    else BoneTransplant::traceBoxes();
}

void TypeAndTell::ownBoxes(int count) {
    // The cords' own lines and pieces go; the device's boxes are its own.
    for (Part& p : parts) freeHits(p);
    if (ownHits) freeHitBoxes();
    hitBoxCount = count;
    hitboxes = new HitBox[static_cast<size_t>(count)];
    ownHits = true;
}

void TypeAndTell::setup() {
    if (currentStage < 5) {
        BoneTransplant::setup();
        return;
    }
    InjuryGraphic::setup();
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    if (currentStage == 5) {
        // The whole device, for the battery.
        gameState->checkTutorial(8, 0);
        ownBoxes(1);
        HitBox& b = hitboxes[0];
        b.rect = {{x - 128, y - 128}, {256, 256}};
        b.active = true;
        b.colour = 0x66ff33;
        b.health = 2;
        b.sprite = Sprite::initWithImageNamed("hitbox_80x64.png", 256, 256);
        b.sprite->calculateTextureCoordsAtOffsetX(0, 0);
        Shape* g = b.sprite->currentShape();
        g->layer = 33;
        g->offset = {-128, -128};
        g->pos = {x, y};
        g->rotation = static_cast<float>(mInjury->rotation);
        return;
    }
    if (currentStage != 6) return;
    // A word, scrolling in from the right; the keys.
    otx = 136;
    chooseWord();
    if (ownHits && hitBoxCount == 30) return;
    ownBoxes(30);
    for (int k = 0; k < 30; ++k) {
        Point at = keyAt(k);
        HitBox& b = hitboxes[k];
        b.rect = {{x + at.x - 12.5f, y + at.y - 12.5f}, {25, 25}};
        b.active = true;
        b.colour = 0x66ff33;
        b.health = 2;
        b.i20 = k;
        b.sprite = Sprite::initWithImageNamed("hitbox_80x64.png", 25, 25);
        b.sprite->calculateTextureCoordsAtOffsetX(0, 0);
        Shape* g = b.sprite->currentShape();
        g->layer = 33;
        g->offset = {-12.5f, -12.5f};
        g->pos = {x + at.x, y + at.y};
        g->rotation = static_cast<float>(mInjury->rotation);
    }
}

void TypeAndTell::clearWord() {
    for (Sprite* s : letters) {
        scene().removeShape(s->currentShape());
        releaseRef(s);
    }
    letters.clear();
}

void TypeAndTell::chooseWord() {
    clearWord();
    int n = static_cast<int>(percent() * 13.0f);
    if (n == 13) n = 12;
    word = GameData::oWords[n];
    typed.clear();
    platform::debugTrace("word %s", word.c_str());
    // Its letters from the 32-point font (eight to a row, from "a").
    for (char c : word) {
        int k = c - 'a';
        Sprite* s = Sprite::initWithImageNamed("font32", 32, 32);
        s->calculateTextureCoordsAtOffsetX(k % 8, k / 8);
        Shape* sh = s->currentShape();
        sh->offset = {-16, -16};
        sh->layer = 42;
        sh->scale = 0.7f;
        letters.push_back(s);
    }
}

void TypeAndTell::render() {
    if (!alive) return;
    if (++specialTimer >= current().specialPeriod) special();
    if (currentStage != 6) return;
    // The word moves left a quarter point a frame; the letters show only on
    // the device's screen (whole: the original showed one from 70 points
    // left of the middle to 110 right, half of it over the frame).
    otx -= 0.25f;
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    float lx = x + otx;
    for (Sprite* s : letters) {
        Shape* sh = s->currentShape();
        sh->pos = {lx, y - 40};
        bool shown = x - 94 <= lx && lx <= x + 92;
        scene().removeShape(sh);
        if (shown) scene().addShape(sh);
        lx += 20;
    }
    if (otx < -68) burnt();
}

void TypeAndTell::interact() {
    if (currentStage == 5) {
        // Woken: the bomb shows.
        retainRef(this);
        updateInjury();
        scene().addShape(mSpriteArray[3]->currentShape());
        trace("treated");
        releaseRef(this);
        return;
    }
    if (currentStage != 6 || !gameState->newTouch) return;
    gameState->newTouch = false;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("BUTTON_SELECT");
    float mx = gameState->mouseX, my = gameState->mouseY;
    for (int k = 0; k < hitBoxCount; ++k) {
        const Rect& r = hitboxes[k].rect;
        if (r.origin.x <= mx && mx <= r.origin.x + r.size.width && r.origin.y <= my &&
            my <= r.origin.y + r.size.height) {
            checkWord(static_cast<char>(GameData::keyMap[k]));
            return;
        }
    }
}

void TypeAndTell::typeKey(char c) {
    if (!typing()) return;
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    checkWord(c);
}

void TypeAndTell::checkWord(char c) {
    if (typed.size() >= word.size() || word[typed.size()] != c) {
        platform::debugTrace("type %c wrong", c);
        burnt();
        return;
    }
    typed += c;
    platform::debugTrace("type %c ok", c);
    // Typed: its letter green.
    float* col = letters[typed.size() - 1]->currentShape()->color;
    col[0] = 0;
    col[2] = 0;
    if (typed != word) return;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("BUZZER_RIGHT");
    gameState->createPopup("correct", mInjury->x, mInjury->y, this);
    if (++correctWords == 5) {
        clearWord();
        updateInjury();
        trace("treated");
        return;
    }
    setup();
}

void TypeAndTell::burnt() {
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("BUZZER_WRONG");
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXPLOSION");
    gameState->createPopup("wrong", mInjury->x, mInjury->y, nullptr);
    // A fire somewhere on the body, and burning splatter from it.
    int fx = static_cast<int>(percent() * 350.0f) + 75, fy = static_cast<int>(percent() * 210.0f) + 80;
    addAt(29, fx, fy);
    addAt(27, fx, fy);
    setup();
    gameState->health = gameState->health + current().damage * -2.0f;
}

}  // namespace as2
