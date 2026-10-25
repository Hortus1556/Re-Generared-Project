#include "as2/injuries/Transplant.h"

#include <cstdlib>
#include <cstring>
#include <utility>

#include "as2/Game.h"
#include "as2/GameController.h"
#include "as2/GameData.h"
#include "as2/GameState.h"
#include "as2/MyOpenAL.h"
#include "as2/tools/Tongs.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

Sprite* sprite(const char* image, int w, int h, int layer, Point offset) {
    Sprite* s = Sprite::initWithImageNamed(image, w, h);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = layer;
    sh->offset = offset;
    return s;
}

}  // namespace

// ----------------------------------------------------------------- Transplant

Transplant::Transplant(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : InjuryGraphic(d, i, playerCreated, force) {
    box_width = 20;
    box_height = 16;
    make = "Transplant";
    maxStages = 4;
    alive = true;
    inLayer = 40;
    isTransplant = true;
    initStageArray(stageTable("Transplant"));
}

Transplant::~Transplant() {
    for (Part& p : parts) {
        freeHits(p);
        releaseRef(p.removable);
    }
    // hitboxes is the last part's, freed above.
    hitboxes = nullptr;
    hitBoxCount = 0;
}

void Transplant::freeHits(Part& p) {
    if (!p.hits) return;
    for (int k = 0; k < p.count; ++k) releaseRef(p.hits[k].sprite);
    if (hitboxes == p.hits) {
        hitboxes = nullptr;
        hitBoxCount = 0;
    }
    delete[] p.hits;
    p.hits = nullptr;
}

void Transplant::traceBoxes() const {
    if (parts.empty()) {
        InjuryGraphic::traceBoxes();
        return;
    }
    // (From the third stage the line burnt and creamed, on the first part.)
    bool healing = currentStage > 2;
    for (const Part& p : parts)
        if (p.hits && (!p.removed || healing))
            InjuryGraphic::traceBoxes(p.hits, p.count, p.complete && !healing ? " piece" : "");
}

void Transplant::removeRemovable(int i) {
    if (i < 0 || i >= static_cast<int>(parts.size())) return;
    releaseRef(parts[static_cast<size_t>(i)].removable);
    parts[static_cast<size_t>(i)].removable = nullptr;
    parts[static_cast<size_t>(i)].removed = true;
}

void Transplant::updateRemovables() {
    for (Part& p : parts) {
        if (p.complete) continue;
        bool done = true;
        for (int k = 0; k < p.count; ++k)
            if (p.hits[k].active) done = false;
        if (!done) continue;
        p.complete = true;
        freeHits(p);
    }
}

void Transplant::removeHit(int i) {
    Part& p = parts[static_cast<size_t>(i)];
    p.complete = true;
    freeHits(p);
    updateRemovables();
}

void Transplant::updateInjury() {
    bool all = true;
    for (const Part& p : parts)
        if (!p.removed) all = false;
    if (all) ++currentStage;
    gameState->createPopup("correct", static_cast<int>(gameState->mouseX), static_cast<int>(gameState->mouseY), this);
    if (maxStages < currentStage) {
        alive = false;
        gameState->removeInjury(this);
    } else {
        setup();
    }
}

// ------------------------------------------------------------- BoneTransplant

// Each kind's pictures: the diseased part and the healthy one (image, size,
// the corner's offset, layer), the body's pieces drawn over them (also where,
// from the injury), the cut line's table and where it is drawn from, and
// the box the cut-out piece is taken by (its size; the left thumb's piece is
// nudged the other way).
struct BoneTransplant::Kind {
    const char* cls;
    struct Pic {
        const char* image;
        int w, h;
        Point offset;
        int layer;
        int dx, dy;
        float rotation = 0;  // its turn (the thigh's upper bone, the clean canister)
    } diseased, healthy, bg1, bg2;
    const float (*points)[3];
    int count;
    int lineDx, lineDy;
    int boxW, boxH;
    bool nudgeLeft;
    // The line the last stages burn and cream along, when not the cut's
    // own (the buttocks' fix).
    const float (*fix)[3] = nullptr;
    int fixCount = 0;
    // The background tinted while it is there (the amp's brain, pink).
    const float* bgTint = nullptr;
};

namespace {

using Pic = BoneTransplant::Kind::Pic;
constexpr Pic kNone{nullptr, 0, 0, {0, 0}, 0, 0, 0};
// A w x h picture about its middle, (dx, dy) from the injury.
constexpr Pic mid(const char* image, int w, int h, int layer, int dx = 0, int dy = 0) {
    return {image, w, h, {static_cast<float>(-w / 2), static_cast<float>(-h / 2)}, layer, dx, dy};
}

constexpr float kPinkBrain[4] = {0.71875f, 0.3125f, 0.71875f, 1};

const BoneTransplant::Kind kBones[] = {
    {"PelvisTransplant", mid("diseasedPelvis", 256, 256, 40), mid("healthyPelvis", 256, 256, 28),
     {"pelvisPiece", 512, 512, {-256, 0}, 40, 116, -65}, kNone, GameData::pelvisHitPoints, 19, 0, 0, 80, 64, false},
    {"ShoulderTransplant", mid("diseasedShoulder", 512, 256, 40), mid("healthyShoulder", 512, 256, 28),
     mid("shoulderPiece1", 128, 128, 40, -219, 92), mid("shoulderPiece2", 128, 64, 40, 201, -82),
     GameData::shoulderHitPoints, 41, 0, 0, 80, 64, false},
    {"SpineTransplant", mid("diseasedSpine", 128, 128, 40), mid("healthySpine", 128, 128, 28),
     mid("spinePiece1", 128, 128, 40, 0, -112), mid("spinePiece2", 128, 128, 40, 0, 111), GameData::spineHitPoints, 20,
     0, 0, 80, 64, false},
    {"KneeTransplant", mid("diseasedKnee", 256, 512, 40), mid("healthyKnee", 256, 512, 28),
     mid("kneePiece1", 256, 256, 41, 8, -152), mid("kneePiece2", 256, 256, 41, 6, 167), GameData::kneeHitPoints, 44, 0,
     0, 256, 140, false},
    {"ThighTransplant", mid("diseasedThigh", 128, 256, 40), mid("healthyThigh", 128, 256, 28),
     mid("thighPiece1", 128, 128, 41, -42, -69), mid("thighPiece2", 256, 256, 41, 75, 105), GameData::thighHitPoints,
     20, 0, 0, 256, 140, false},
    {"ThighTransplant2", mid("kneeDamage2", 128, 128, 41), mid("kneeHealthy2", 128, 128, 28),
     {"thighbone2", 128, 512, {-64, -256}, 40, 168, -92, 60}, kNone, GameData::thighHitPoints2, 6, 0, 0, 256, 140, false},
    {"LeftThumbTransplant", mid("trotterPiece", 256, 256, 40), mid("trotterThumb", 256, 256, 28),
     {"napoleonTrotter", 256, 512, {-256, 0}, 40, 224, -143}, kNone, GameData::trotterHitPoints, 7, 16, 20, 80, 128,
     true},
    {"RightThumbTransplant", mid("trotterPieceRight", 256, 256, 40), mid("trotterThumbRight", 256, 256, 28),
     {"napoleonTrotterRight", 256, 512, {-256, 0}, 40, 32, -143}, kNone, GameData::trotterRightHitPoints, 7, -16, 20,
     80, 128, false},
    {"NoseTransplant", mid("noseBoneBroken", 128, 128, 40), mid("noseBoneFixed", 128, 128, 28),
     {"noseBone", 256, 256, {-128, -36}, 40, 5, 0}, kNone, GameData::noseHitPoints, 6, 0, 0, 80, 64, false},
    {"EyeTransplant", mid("cortexExtraction", 128, 128, 40), mid("cortwxReplace", 128, 128, 28),
     mid("cortex", 512, 256, 40, -21, -30), kNone, GameData::eyeHitPoints, 12, 0, 0, 80, 64, false},
    {"RibTransplant", mid("rib1-damage", 128, 128, 40), mid("rib1-healthy", 128, 128, 28),
     {"rib-stationary", 512, 512, {-179, -169}, 41, 0, 0}, kNone, GameData::ribHitPoints, 4, 0, 0, 80, 64, false},
    {"RibTransplant2", mid("rib2-damage", 256, 128, 40), mid("rib2-healthy", 256, 128, 28), kNone, kNone,
     GameData::ribHitPoints2, 4, 0, 0, 80, 64, false},
    {"RibTransplant3", mid("rib3-damage", 256, 128, 40), mid("rib3-healthy", 256, 128, 28), kNone, kNone,
     GameData::ribHitPoints3, 4, 0, 0, 80, 64, false},
    {"Canister", mid("canisterDIRTY", 128, 256, 41), {"canisterCLEAN", 128, 256, {-64, -128}, 28, 0, 0, -10}, kNone, kNone,
     GameData::canisterHitPoints, 3, 0, 0, 256, 140, false},
    {"LighterLeftTransplant", mid("oldburnerLeft", 128, 256, 41), mid("newburnerLeft", 128, 256, 28), kNone, kNone,
     GameData::LighterLeftHitPoints, 2, 0, 0, 256, 140, false},
    {"LighterRightTransplant", mid("oldburnerRight", 128, 256, 41), mid("newburnerRight", 128, 256, 28), kNone, kNone,
     GameData::LighterRightHitPoints, 2, 0, 0, 256, 140, false},
    {"LeftBicepTransplant", mid("Left_bicep_sick", 256, 256, 41), mid("Left_bicep_healthy", 256, 256, 28), kNone,
     kNone, GameData::LeftBicepHitPoints, 10, 0, 0, 256, 140, false},
    {"LeftForearmTransplant", mid("Left_forearm_sick", 256, 256, 41), mid("Left_forearm_healthy", 256, 256, 28), kNone,
     kNone, GameData::LeftForearmHitPoints, 14, 0, 0, 256, 140, false},
    {"LeftButtTransplant", mid("butt_left_small", 256, 256, 41), {"butt_left_big", 256, 256, {-150, -132}, 28, 0, 0},
     kNone, kNone, GameData::LeftButtHitPoints, 18, 0, 0, 256, 140, false, GameData::LeftButtFixHitPoints, 11},
    {"RightButtTransplant", mid("butt_right_small", 256, 256, 41), {"butt_right_big", 256, 256, {-102, -132}, 28, 0, 0},
     kNone, kNone, GameData::RightButtHitPoints, 18, 0, 0, 256, 140, false, GameData::RightButtFixHitPoints, 11},
    {"RightTricepTransplant", mid("Right_bicep_sick1", 256, 256, 41), mid("Right_bicep_healthy1", 256, 256, 28), kNone,
     kNone, GameData::RightTricepHitPoints, 8, 0, 0, 256, 140, false},
    {"LeftLungTransplant", mid("tricksleftlungpoison", 256, 512, 41), mid("tricksleftlunghealthy", 256, 512, 28), kNone,
     kNone, GameData::leftLungHitPoints, 33, 0, 0, 256, 140, false},
    {"RightLungTransplant", mid("tricksrightlungpoison", 256, 512, 41), mid("tricksrightlunghealthy", 256, 512, 28),
     kNone, kNone, GameData::rightLungHitPoints, 33, 0, 0, 256, 140, false},
    // (The original put the brain's hole at (262, 203), its opening 26
    // points left of the flap and the cut line: moved over them.)
    {"Amp", mid("S9_BrainFlap", 128, 128, 40), mid("amp", 128, 128, 28),
     {"S9_BrainHole", 512, 512, {-256, -256}, 41, -10, -28}, kNone, GameData::ampHitPoints, 20, 0, 0, 80, 64, false,
     nullptr, 0, kPinkBrain},
    {"BrainTransplant", {"S10_Brain1", 256, 512, {-128, -209}, 41, 0, 0}, {"S10_Brain2", 256, 512, {-128, -209}, 28, 0, 0},
     kNone, kNone, GameData::brainHitPoints, 28, 0, 0, 256, 256, false},
    // (The skull's cap goes to the tray for good: its "healthy" sprite is
    // the original's, an eye mask, never used.)
    {"SkullCut", {"cap", 512, 128, {-256, -64}, 41, 0, 0}, mid("right_eye_mask.png", 64, 64, 40), kNone, kNone,
     GameData::skullHitPoints, 11, 0, 0, 200, 80, false},
};

// The organs of three parts (kidneys, liver, beard): each part's piece (at
// its origin from the injury) and cut line, the one healthy organ, pieces of
// the body drawn over them, and the line round the whole the last stages
// burn and cream along.
}  // namespace

struct BoneTransplant::Organ {
    const char* cls;
    Pic pieces[3];
    Pic healthy;
    Pic extra[2];
    const float (*points[3])[3];
    int counts[3];
    const float (*heal)[3];
    int healCount;
    const float* originX;
    const float* originY;
    Pic between = kNone;  // before the healthy one (the type-n-tell's bomb)
};

namespace {

const BoneTransplant::Organ kOrgans[] = {
    {"KidneyTransplantLeft",
     {mid("kidneyPiece1", 256, 256, 40), mid("kidneyPiece2", 256, 256, 40), mid("kidneyPiece3", 256, 256, 40)},
     {"healthyKidney", 256, 512, {-128, -256}, 28, 0, 0}, {kNone, kNone},
     {GameData::KidneyTransplantLeftHitPoints1, GameData::KidneyTransplantLeftHitPoints2,
      GameData::KidneyTransplantLeftHitPoints3},
     {21, 25, 21}, GameData::KidneyTransplantLeftHitPoints0, 32, GameData::originX, GameData::originY},
    {"PoisonKidneyTransplant",
     {{"poisonkidneypiece1", 256, 128, {-128, -64}, 40, 0, 0}, mid("poisonkidneypiece2", 256, 256, 40),
      mid("poisonkidneypiece3", 256, 256, 40)},
     {"healthyKidney", 256, 512, {-128, -256}, 28, 0, 0}, {kNone, kNone},
     {GameData::PoisonKidneyTransplantLeftHitPoints1, GameData::PoisonKidneyTransplantLeftHitPoints2,
      GameData::PoisonKidneyTransplantLeftHitPoints3},
     {21, 25, 21}, GameData::PoisonKidneyTransplantLeftHitPoints0, 32, GameData::poisonKidneyOriginX,
     GameData::poisonKidneyOriginY},
    {"PoisonLiverTransplant",
     {mid("poisonliverpiece1", 256, 256, 40), mid("poisonliverpiece2", 256, 256, 40),
      mid("poisonliverpiece3", 256, 256, 40)},
     mid("healthyliver", 512, 512, 28), {mid("arterypiece", 64, 64, 40, 38, -111), mid("arterypiece2", 128, 128, 40, 60, 148)},
     {GameData::LiverTransplantLeftHitPoints1, GameData::LiverTransplantLeftHitPoints2,
      GameData::LiverTransplantLeftHitPoints3},
     {10, 17, 7}, GameData::LiverTransplantLeftHitPoints0, 6, GameData::liverOriginX, GameData::liverOriginY},
    {"BeardTransplant",
     {mid("beardpiece1", 256, 256, 40), mid("beardpiece2", 256, 256, 40), mid("beardpiece3", 256, 256, 40)},
     mid("newbeard", 512, 512, 28), {kNone, kNone},
     {GameData::BeardHitPoints1, GameData::BeardHitPoints2, GameData::BeardHitPoints3}, {5, 10, 5},
     GameData::BeardHitPoints0, 12, GameData::beardOriginX, GameData::beardOriginY},
    {"TypeAndTell",
     {{"vocalchordPiece1", 64, 256, {-32, -128}, 40, 0, 0}, {"vocalchordPiece2", 64, 256, {-32, -128}, 40, 0, 0},
      {"vocalchordPiece3", 64, 256, {-32, -128}, 40, 0, 0}},
     mid("typentell", 256, 256, 28), {mid("vocalchordsTop", 256, 128, 40, 1, -172), mid("vocalchordsBottom", 256, 64, 40, 2, 135)},
     {GameData::TypeAndTellTransplantHitPoints1, GameData::TypeAndTellTransplantHitPoints2,
      GameData::TypeAndTellTransplantHitPoints3},
     {26, 26, 26}, GameData::TypeAndTellTransplantHitPoints0, 46, GameData::TypeAndTellOriginX,
     GameData::TypeAndTellOriginY, mid("typeBomb", 32, 32, 28, -88, -40)},
};

}  // namespace

const char* const* BoneTransplant::kinds() {
    static std::vector<const char*> names = [] {
        std::vector<const char*> v;
        for (const Kind& k : kBones) v.push_back(k.cls);
        for (const Organ& k : kOrgans) v.push_back(k.cls);
        v.push_back(nullptr);
        return v;
    }();
    return names.data();
}

BoneTransplant::BoneTransplant(InjuryData* d, Injury* i, bool playerCreated, bool force)
    : Transplant(d, i, playerCreated, force) {
    for (const Organ& o : kOrgans)
        if (isKindOf(o.cls)) {
            organ(o);
            return;
        }
    for (const Kind& k : kBones)
        if (isKindOf(k.cls)) kind = &k;
    if (!kind) return;
    // Its own stages where it has them (the brain's battery, the skull's
    // one), else the transplant's four.
    initOwnStages("Transplant");
    parts.resize(1);
    specs = {{kind->points, kind->count, 0, 0}};
    heal = kind->fix ? kind->fix : kind->points;
    healCount = kind->fix ? kind->fixCount : kind->count;
    boxW = kind->boxW;
    boxH = kind->boxH;
    lineDx = kind->lineDx;
    lineDy = kind->lineDy;
    nudgeLeft = kind->nudgeLeft;
    const Pic& sick = kind->diseased;
    Sprite* diseased = sprite(sick.image, sick.w, sick.h, sick.layer, sick.offset);
    mSpriteArray.addObject(diseased);
    scene().addShape(diseased->currentShape());
    releaseRef(diseased);
    // The healthy one waits (the tongs put it on the tray, then in place).
    const Pic& well = kind->healthy;
    Sprite* healthy = sprite(well.image, well.w, well.h, well.layer, well.offset);
    healthy->currentShape()->rotation = well.rotation;
    mSpriteArray.addObject(healthy);
    releaseRef(healthy);
    forcePosition = true;
    setup();
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    for (auto [piece, out] : {std::pair{&kind->bg1, &mSpriteBG1}, std::pair{&kind->bg2, &mSpriteBG2}}) {
        if (!piece->image) continue;
        *out = sprite(piece->image, piece->w, piece->h, piece->layer, piece->offset);
        (*out)->currentShape()->pos = {x + static_cast<float>(piece->dx), y + static_cast<float>(piece->dy)};
        (*out)->currentShape()->rotation = piece->rotation;
        scene().addShape((*out)->currentShape());
    }
    if (kind->bgTint && gameState->spriteBg)
        for (int c = 0; c < 4; ++c) gameState->spriteBg->currentShape()->color[c] = kind->bgTint[c];
}

void BoneTransplant::organ(const Organ& o) {
    float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
    parts.resize(3);
    for (int p = 0; p < 3; ++p) specs.push_back({o.points[p], o.counts[p], o.originX[p], o.originY[p]});
    heal = o.heal;
    healCount = o.healCount;
    // The pieces, the healthy organ (waiting for the tray), the body's
    // pieces over them.
    for (const Pic& pc : o.pieces) {
        Sprite* s = sprite(pc.image, pc.w, pc.h, pc.layer, pc.offset);
        mSpriteArray.addObject(s);
        releaseRef(s);
    }
    if (o.between.image) {
        Sprite* b = sprite(o.between.image, o.between.w, o.between.h, o.between.layer, o.between.offset);
        b->currentShape()->pos = {x + static_cast<float>(o.between.dx), y + static_cast<float>(o.between.dy)};
        mSpriteArray.addObject(b);
        releaseRef(b);
    }
    Sprite* healthy = sprite(o.healthy.image, o.healthy.w, o.healthy.h, o.healthy.layer, o.healthy.offset);
    healthy->currentShape()->pos = {x, y};
    healthyIndex = mSpriteArray.count();
    mSpriteArray.addObject(healthy);
    releaseRef(healthy);
    setup();
    for (int p = 0; p < 3; ++p) {
        Shape* sh = mSpriteArray[p]->currentShape();
        sh->pos = {x + o.originX[p], y + o.originY[p]};
        scene().addShape(sh);
    }
    for (auto [piece, out] : {std::pair{&o.extra[0], &mSpriteBG1}, std::pair{&o.extra[1], &mSpriteBG2}}) {
        if (!piece->image) continue;
        *out = sprite(piece->image, piece->w, piece->h, piece->layer, piece->offset);
        (*out)->currentShape()->pos = {x + static_cast<float>(piece->dx), y + static_cast<float>(piece->dy)};
        (*out)->currentShape()->rotation = piece->rotation;
        scene().addShape((*out)->currentShape());
    }
}

BoneTransplant::~BoneTransplant() {
    releaseRef(mSpriteBG1);
    releaseRef(mSpriteBG2);
}

void BoneTransplant::createEffect(int count, int velocity) {
    // Flesh bleeds; bone sparks and chips (the thumbs' hoof only sparks;
    // the canister shows nothing).
    for (const char* flesh : {"EyeTransplant", "LeftBicepTransplant", "LeftForearmTransplant", "RightTricepTransplant",
                              "LeftLungTransplant", "RightLungTransplant", "KidneyTransplantLeft",
                              "PoisonKidneyTransplant", "PoisonLiverTransplant", "BeardTransplant",
                              "LeftButtTransplant", "RightButtTransplant", "Amp", "BrainTransplant", "SkullCut"})
        if (isKindOf(flesh)) {
            InjuryGraphic::createEffect(count, velocity);
            return;
        }
    if (isKindOf("Canister")) return;
    int x = static_cast<int>(gameState->mouseX), y = static_cast<int>(gameState->mouseY);
    for (int n = 0; n < count; ++n) game->addEffect("sparkParticle", x, y);
    if (!isKindOf("LeftThumbTransplant") && !isKindOf("RightThumbTransplant")) game->addEffect("boneParticle", x, y);
}

void BoneTransplant::lineBoxes(Part& p) {
    const PartSpec& spec = specs[static_cast<size_t>(&p - parts.data())];
    const float(*points)[3] = currentStage > 2 ? heal : spec.points;
    p.count = currentStage > 2 ? healCount : spec.count;
    p.hits = new HitBox[static_cast<size_t>(p.count)];
    // (The thumbs' lines are drawn a little off the injury's place.)
    lineHitBoxes(p.hits, points, p.count, static_cast<float>(mInjury->x + lineDx),
                 static_cast<float>(mInjury->y + lineDy));
}

void BoneTransplant::pieceBox(Part& p) {
    size_t i = static_cast<size_t>(&p - parts.data());
    float bx = static_cast<float>(mInjury->x) + specs[i].ox, by = static_cast<float>(mInjury->y) + specs[i].oy;
    if (currentStage == 2) {
        // (So the original; the tongs then move the box onto the tray.)
        bx = static_cast<float>(128 - mInjury->x);
        by = static_cast<float>(220 - mInjury->y);
    }
    // A little off, loose.
    float rx = static_cast<float>(std::rand() % 10 + 5), ry = static_cast<float>(std::rand() % 10 + 5);
    if (nudgeLeft) rx = -rx;
    p.count = 1;
    p.hits = new HitBox[1];
    HitBox& b = p.hits[0];
    float hw = static_cast<float>(boxW / 2), hh = static_cast<float>(boxH / 2);
    b.rect = {{bx - hw + rx, by - hh + ry}, {static_cast<float>(boxW), static_cast<float>(boxH)}};
    b.sprite = sprite("hitbox_80x64.png", boxW, boxH, 33, {-hw, -hh});
    Shape* s = b.sprite->currentShape();
    s->pos = {bx + rx, by + ry};
    s->rotation = static_cast<float>(mInjury->rotation);
    colourBox(b);
    if (!p.removable) {
        p.removable = new DraggableWrapper;
        p.removable->injury = this;
    }
    DraggableWrapper* w = p.removable;
    w->hitboxes = p.hits;
    w->hitBoxCount = p.count;
    w->mSprite = mSpriteArray[static_cast<int>(i)];
    Shape* piece = w->mSprite->currentShape();
    piece->pos.x += rx;
    piece->pos.y -= ry;
    w->healthyReplacementSprite = mSpriteArray[healthyIndex];
    w->xOrigin = bx;
    w->yOrigin = by;
}

void BoneTransplant::setup() {
    InjuryGraphic::setup();
    if (specs.empty()) return;
    if (current().tool == "battery") {
        // A battery stage (the new brain's restart): the whole piece is the
        // target, with no line. (The original drew the cream's green line
        // again, so the stage looked like the cream's still.)
        Part& p = parts[0];
        freeHits(p);
        p.count = 1;
        p.hits = new HitBox[1];
        HitBox& b = p.hits[0];
        float x = static_cast<float>(mInjury->x), y = static_cast<float>(mInjury->y);
        float hw = static_cast<float>(boxW / 2), hh = static_cast<float>(boxH / 2);
        b.rect = {{x - hw, y - hh}, {static_cast<float>(boxW), static_cast<float>(boxH)}};
        b.sprite = sprite("hitbox_80x64.png", boxW, boxH, 33, {-hw, -hh});
        b.sprite->currentShape()->pos = {x, y};
        b.active = true;
        b.health = 2;
        hitboxes = p.hits;
        hitBoxCount = 1;
        return;
    }
    for (Part& p : parts) {
        // An organ's last stages: one line round the whole (on its first part).
        if (currentStage > 2 && &p != parts.data() && parts.size() > 1) {
            freeHits(p);
            continue;
        }
        if (currentStage >= 2 && p.hits && p.complete) freeHits(p);  // the last stage's
        if (p.hits) continue;
        if (!p.complete || currentStage > 2) lineBoxes(p);
        else pieceBox(p);
        hitboxes = p.hits;
        hitBoxCount = p.count;
    }
}

void BoneTransplant::interact() {
    if (!isKindOf("Canister")) return;
    auto* tongs = dynamic_cast<Tongs*>(gameState->currentTool);
    if (!tongs || tongs->holdingNow() != this) return;
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("small_explosion");
    gameState->health = gameState->health - 2;
    int x = static_cast<int>(gameState->mouseX), y = static_cast<int>(gameState->mouseY);
    game->addEffect("ExplodeEffect", x, y);
    MyOpenAL::sharedMyOpenAL().playSFXWithKey("EXPLOSION");
    for (int dy : {-30, 30}) {
        Injury* fire = new Injury;
        fire->mID = 0x1d;
        fire->x = x;
        fire->y = y + dy;
        fire->rotation = 0;
        fire->scale = 1;
        gameState->addInjury(fire, true, false);
        releaseRef(fire);
    }
}

}  // namespace as2
