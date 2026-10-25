#include "as2/injuries/Remains.h"

#include <algorithm>
#include <cmath>

#include "as2/GameController.h"
#include "as2/GameState.h"
#include "as2/menus/FlashStage.h"

namespace as2 {

namespace {
SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }
}  // namespace

Remains::Remains(InjuryData* d, Injury* i, bool playerCreated, bool force, const Kind& kind)
    : Extraction(d, i, playerCreated, force), label(kind.label), hitGroup(kind.hitGroup) {
    make = "remains";
    maxStages = 1;
    alive = true;
    initStageArray(stageTable(d->type));
    Sprite* s = Sprite::initWithImageNamed(kind.picture, kind.size, kind.size);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 32;
    sh->offset = {-kind.size / 2.0f, -kind.size / 2.0f};
    sh->pos = {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
    mSpriteArray.addObject(s);
    scene().addShape(sh);
    flash = std::make_unique<FlashCreature>(kind.symbol);
    flash->attach(sh);
    flash->turn = static_cast<float>(mInjury->rotation);
    flash->look.hidden = kind.hidden;
    flash->still = true;
    for (const auto& [clip, inner] : kind.innerLabels) flash->look.frames[clip] = flash->innerFrameOf(clip, inner);
    if (!label.empty()) flash->show(label);
    setup();
    // What the tongs carry to the tray.
    DraggableWrapper* w = new DraggableWrapper;
    w->injury = this;
    w->mSprite = s;
    w->hitboxes = hitboxes;
    w->hitBoxCount = hitBoxCount;
    w->xOrigin = sh->pos.x;
    w->yOrigin = sh->pos.y;
    removable.addObject(w);
    releaseRef(w);
    releaseRef(s);
}

void Remains::setup() {
    InjuryGraphic::setup();
    // (Its art turns it: the sprite the set-up turned stays straight.)
    mSpriteArray[0]->currentShape()->rotation = 0;
    invincible = false;
    damageTimer = 0;
    if (hitboxes) return;
    // One box about its hit areas (their bounds), turned with it.
    flash::Rect r{-20, -16, 20, 16};  // (no art: a box about it)
    std::vector<flash::Rect> areas;
    if (!hitGroup.empty()) areas = flash->hitAreas(label, hitGroup);
    else if (flash->ready()) areas.push_back(flash->boundsOf(label, ""));
    if (!areas.empty()) {
        r = areas[0];
        for (const flash::Rect& a : areas)
            r = {std::min(r.xMin, a.xMin), std::min(r.yMin, a.yMin), std::max(r.xMax, a.xMax), std::max(r.yMax, a.yMax)};
    }
    const float k = flashstage::worldPerStage();
    box_width = static_cast<int>((r.xMax - r.xMin) * k);
    box_height = static_cast<int>((r.yMax - r.yMin) * k);
    const double a = mInjury->rotation * 3.141592653589793 / 180;
    const float cx = (r.xMin + r.xMax) / 2 * k, cy = (r.yMin + r.yMax) / 2 * k;
    const float x = static_cast<float>(mInjury->x + cx * std::cos(a) - cy * std::sin(a));
    const float y = static_cast<float>(mInjury->y + cx * std::sin(a) + cy * std::cos(a));
    oneHitBox(x, y, "hitbox_80x64.png");
    trace("boxed");
}

}  // namespace as2
