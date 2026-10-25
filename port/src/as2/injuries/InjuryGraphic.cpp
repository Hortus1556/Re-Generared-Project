#include "as2/injuries/InjuryGraphic.h"

#include <cmath>
#include <cstdlib>
#include <map>

#include "as2/GameController.h"
#include "as2/FlashCreature.h"
#include "as2/GameState.h"
#include "as2/menus/FlashStage.h"
#include "as2/tools/Tongs.h"
#include "core/Json.h"
#include "platform/Assets.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

std::map<std::string, std::function<InjuryGraphic*(InjuryData*, Injury*, bool, bool, int)>>& classes() {
    static std::map<std::string, std::function<InjuryGraphic*(InjuryData*, Injury*, bool, bool, int)>> c;
    return c;
}

const std::map<std::string, std::string>& superclasses() {
    static const std::map<std::string, std::string> t = [] {
        std::map<std::string, std::string> out;
        std::string text;
        json::Value doc;
        if (!assets::readText("data/library.json", text) || !json::parse(text, doc)) return out;
        for (const auto& kv : doc["classes"].members) out[kv.first] = kv.second.str();
        return out;
    }();
    return t;
}

// The stage tables by class: the iPad's ("stages") or the Flash AS2's
// ("flashStages").
std::map<std::string, std::vector<StageInfo>> readTables(const char* key) {
    std::map<std::string, std::vector<StageInfo>> out;
    std::string text;
    json::Value doc;
    if (!assets::readText("data/library.json", text) || !json::parse(text, doc)) return out;
    {
        for (const auto& kv : doc[key].members) {
            std::vector<StageInfo>& list = out[kv.first];
            for (const json::Value& v : kv.second.items) {
                StageInfo s;
                s.tool = v["tool"].str();
                s.wrongTool = v["wrongTool"].str();
                s.i8 = v["i8"].integer();
                s.damagePeriod = v["damagePeriod"].integer();
                s.damage = static_cast<float>(v["damage"].num());
                s.specialPeriod = v["specialPeriod"].integer();
                s.effect = v["effect"].str();
                s.s1c = v["s1c"].str();
                s.c20 = v["c20"].integer() != 0;
                s.lineColour = v["lineColour"].str();
                s.c28 = v["c28"].integer() != 0;
                s.i2c = v["i2c"].integer();
                list.push_back(s);
            }
        }
    }
    return out;
}

std::map<std::string, std::vector<StageInfo>>& tables() {
    static std::map<std::string, std::vector<StageInfo>> t = readTables("stages");
    return t;
}

std::map<std::string, std::vector<StageInfo>>& flashTables() {
    static std::map<std::string, std::vector<StageInfo>> t = readTables("flashStages");
    return t;
}

// random() % 100 / 100, as the original's float maths had it.
float percent() { return static_cast<float>(std::rand() % 100) / 100.0f; }

}  // namespace

const std::vector<StageInfo>& stageTable(const std::string& className) {
    static const std::vector<StageInfo> none;
    auto it = tables().find(className);
    return it == tables().end() ? none : it->second;
}

const std::vector<StageInfo>& flashStageTable(const std::string& className) {
    auto it = flashTables().find(className);
    return it == flashTables().end() ? stageTable(className) : it->second;
}

void registerInjuryClass(const std::string& name,
                         std::function<InjuryGraphic*(InjuryData*, Injury*, bool, bool, int)> make) {
    classes()[name] = std::move(make);
}

InjuryGraphic* InjuryGraphic::create(InjuryData* data, Injury* instance, bool playerCreated, bool force,
                                     int velocity) {
    if (!data) return nullptr;
    auto it = classes().find(data->type);
    if (it == classes().end()) {
        platform::debugTrace("injury: class %s is not ported", data->type.c_str());
        return nullptr;
    }
    return it->second(data, instance, playerCreated, force, velocity);
}

InjuryGraphic::InjuryGraphic(InjuryData* data, Injury* instance, bool playerCreated, bool force) {
    // setData:Instance:PlayerCreated:Force:
    mInjuryData = data;
    mInjury = retainRef(instance);
    mPlayerCreated = playerCreated;
    mForce = force;
}

InjuryGraphic::~InjuryGraphic() {
    freeHitBoxes();
    SceneManager& sm = SceneManager::sharedSceneManagerInstance();
    for (Sprite* s : mSpriteArray) sm.removeShape(s->currentShape());
    mSpriteArray.removeAllObjects();
    releaseRef(mInjury);
}

void InjuryGraphic::freeHitBoxes() {
    if (!hitboxes) return;
    for (int i = 0; i < hitBoxCount; ++i) {
        if (hitboxes[i].sprite) {
            SceneManager::sharedSceneManagerInstance().removeShape(hitboxes[i].sprite->currentShape());
            releaseRef(hitboxes[i].sprite);
        }
    }
    delete[] hitboxes;
    hitboxes = nullptr;
    hitBoxCount = 0;
}

void InjuryGraphic::oneHitBox(float x, float y, const std::string& guide, float health) {
    if (!hitboxes) {
        hitBoxCount = 1;
        hitboxes = new HitBox[1];
    }
    HitBox& b = hitboxes[0];
    float w = static_cast<float>(box_width), h = static_cast<float>(box_height);
    b.rect = {{x - static_cast<float>(box_width / 2), y - static_cast<float>(box_height / 2)}, {w, h}};
    b.active = true;
    b.colour = 0x66ff33;
    b.health = health;
    if (!b.sprite) {
        b.sprite = Sprite::initWithImageNamed(guide, box_width, box_height);
        b.sprite->calculateTextureCoordsAtOffsetX(0, 0);
    }
    Shape* g = b.sprite->currentShape();
    g->layer = 44;
    g->offset = {static_cast<float>(-(box_width / 2)), static_cast<float>(-(box_height / 2))};
    g->pos = {x, y};
    g->rotation = static_cast<float>(mInjury->rotation);
}

Point InjuryGraphic::flashPlace() const {
    if (mInjury->staged) return flashstage::stageToWorld(static_cast<float>(mInjury->stageX), static_cast<float>(mInjury->stageY));
    return {static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)};
}

void InjuryGraphic::flashHitBoxes(FlashCreature& flash, const std::string& label, Point at, float health) {
    freeHitBoxes();
    flashBoxOffsets.clear();
    std::vector<flash::Rect> areas = flash.hitAreas(label);
    if (areas.empty()) areas.push_back({-20, -16, 20, 16});  // (no art: a box about it)
    const float k = flashstage::worldPerStage();
    hitBoxCount = static_cast<int>(areas.size());
    hitboxes = new HitBox[areas.size()];
    for (size_t n = 0; n < areas.size(); ++n) {
        const flash::Rect& r = areas[n];
        float w = (r.xMax - r.xMin) * k, h = (r.yMax - r.yMin) * k;
        flashBoxOffsets.push_back({(r.xMin + r.xMax) / 2 * k, (r.yMin + r.yMax) / 2 * k});
        HitBox& b = hitboxes[n];
        b.rect = {{0, 0}, {w, h}};
        b.active = true;
        b.colour = 0x66ff33;
        b.health = health;
        b.sprite = Sprite::initWithImageNamed("hitbox_80x64.png", static_cast<int>(w), static_cast<int>(h));
        b.sprite->calculateTextureCoordsAtOffsetX(0, 0);
        Shape* g = b.sprite->currentShape();
        g->layer = 33;
        g->offset = {-w / 2, -h / 2};
    }
    if (!areas.empty()) {
        box_width = static_cast<int>(hitboxes[0].rect.size.width);
        box_height = static_cast<int>(hitboxes[0].rect.size.height);
    }
    moveFlashHitBoxes(at);
    trace("boxed");  // (fresh boxes, for the play-tests)
}

void InjuryGraphic::moveFlashHitBoxes(Point at) {
    for (int n = 0; n < hitBoxCount && n < static_cast<int>(flashBoxOffsets.size()); ++n) {
        HitBox& b = hitboxes[n];
        Point c = {at.x + flashBoxOffsets[n].x, at.y + flashBoxOffsets[n].y};
        b.rect.origin = {c.x - b.rect.size.width / 2, c.y - b.rect.size.height / 2};
        if (b.sprite) b.sprite->currentShape()->pos = c;
    }
}

bool InjuryGraphic::isKindOf(const std::string& className) const {
    if (!mInjuryData) return false;
    const auto& parents = superclasses();
    for (std::string c = mInjuryData->type; !c.empty();) {
        if (c == className) return true;
        auto it = parents.find(c);
        if (it == parents.end()) break;
        c = it->second;
    }
    return className == "InjuryGraphic";
}

void InjuryGraphic::columnHitBoxes(int count, const std::string& guide, bool shown, int layer, Point from) {
    freeHitBoxes();
    hitBoxCount = count;
    hitboxes = new HitBox[static_cast<size_t>(count)];
    float x = static_cast<float>(mInjury->x) + from.x, y = static_cast<float>(mInjury->y) + from.y;
    for (int i = 0; i < count; ++i) {
        HitBox& b = hitboxes[i];
        b.rect = {{x - static_cast<float>(box_width / 2), y + static_cast<float>(box_height * i)},
                  {static_cast<float>(box_width), static_cast<float>(box_height)}};
        // Every line colour (invisible ones too) made the box active; the
        // colour it was given was then always overwritten with green.
        b.active = true;
        b.colour = 0x66ff33;
        b.health = 2;
        b.sprite = Sprite::initWithImageNamed(guide, box_width, box_height);
        b.sprite->calculateTextureCoordsAtOffsetX(0, 0);
        Shape* s = b.sprite->currentShape();
        s->layer = layer;
        s->offset = {static_cast<float>(-(box_width / 2)), static_cast<float>(box_height * i)};
        s->pos = {x, y};
        s->rotation = static_cast<float>(mInjury->rotation);
        if (shown) SceneManager::sharedSceneManagerInstance().addShape(s);
    }
}

void InjuryGraphic::lineHitBoxes(HitBox* out, const float (*points)[3], int count, float x, float y) {
    for (int k = 0; k < count; ++k) {
        const float* t = points[k];
        HitBox& b = out[k];
        b.rect = {{static_cast<float>(-(box_width / 2)) + t[0] + x, t[1] + y},
                  {static_cast<float>(box_width), static_cast<float>(box_height)}};
        b.sprite = Sprite::initWithImageNamed("hitbox_20x16.png", 20, 16);
        b.sprite->calculateTextureCoordsAtOffsetX(0, 0);
        Shape* s = b.sprite->currentShape();
        s->layer = 33;
        s->offset = {static_cast<float>(-(box_width / 2)), 0};
        s->pos = {t[0] + x, t[1] + y};
        s->rotation = t[2];
        colourBox(b);
        SceneManager::sharedSceneManagerInstance().addShape(s);
    }
}

void InjuryGraphic::waitOnTray(Sprite* piece, HitBox* boxes, int count) {
    DraggableWrapper* w = new DraggableWrapper;
    w->injury = this;
    w->hitboxes = boxes;
    w->hitBoxCount = count;
    w->mSprite = piece;
    w->healthyReplacementSprite = piece;
    w->xOrigin = static_cast<float>(mInjury->x);
    w->yOrigin = static_cast<float>(mInjury->y);
    removable.removeAllObjects();
    removable.addObject(w);
    releaseRef(w);
    for (ITool* t : gameState->possible_tools)
        if (auto* tongs = dynamic_cast<Tongs*>(t)) tongs->setupInsertion(this);
}

void InjuryGraphic::colourBox(HitBox& b) {
    const std::string& line = current().lineColour;
    float* c = b.sprite->currentShape()->color;
    if (line == "invisible") {
        b.active = true;
    } else if (line == "yellow") {
        b.active = true;
        c[0] = 1, c[1] = 1, c[2] = 0, c[3] = 1;
        b.colour = 0xffff33;
    } else if (line == "black") {
        b.active = true;
        c[0] = 0, c[1] = 0, c[2] = 0, c[3] = 1;
        b.colour = 0;
    } else if (line == "green") {
        b.active = true;
        c[0] = 0, c[1] = 1, c[2] = 0, c[3] = 1;
        b.colour = 0x66ff33;
    }
    b.health = 2;
}

void InjuryGraphic::hideGuides() {
    for (int i = 0; i < hitBoxCount; ++i)
        if (hitboxes[i].sprite) SceneManager::sharedSceneManagerInstance().removeShape(hitboxes[i].sprite->currentShape());
}

bool InjuryGraphic::fadeOut(Sprite* s, bool fadeIn) {
    if (--removeCounter < 1) {
        gameState->removeInjury(this);
        return true;
    }
    float v = fadeIn ? 1.0f / static_cast<float>(removeCounter) : static_cast<float>(removeCounter) / 10.0f;
    float* c = s->currentShape()->color;
    c[0] = c[1] = c[2] = c[3] = v;
    return false;
}

void InjuryGraphic::followFinger(Sprite* pupil, float x, float y, float scale, float max) {
    float dx = gameState->mouseX - x, dy = gameState->mouseY - y;
    float d = std::sqrt(dx * dx + dy * dy) * scale;
    if (d > max) d = max;
    double a = static_cast<float>(std::atan2(static_cast<double>(dy), static_cast<double>(dx)));
    Shape* s = pupil->currentShape();
    s->pos.x = static_cast<float>(x + std::cos(a) * d * 1.2);
    s->pos.y = static_cast<float>(y + d * std::sin(a));
}

void InjuryGraphic::trace(const char* event) const {
    if (!platform::traceEnabled()) return;
    Point at = gameState->worldToScreen({static_cast<float>(mInjury->x), static_cast<float>(mInjury->y)});
    platform::debugTrace("injury %s %s id %d stage %d/%d tool %s alive %d at %.0f,%.0f rot %d hidden %d #%llu", event,
                         mInjuryData ? mInjuryData->type.c_str() : "?", mInjury->mID, currentStage, maxStages,
                         toolNow().c_str(), alive ? 1 : 0, at.x, at.y, mInjury->rotation, hidden ? 1 : 0,
                         serial());
    traceBoxes();
}

void InjuryGraphic::traceBoxes() const { traceBoxes(hitboxes, hitBoxCount, ""); }

void InjuryGraphic::traceBoxes(const HitBox* boxes, int count, const char* note) {
    for (int i = 0; i < count; ++i) {
        const HitBox& b = boxes[i];
        // Every box (healed ones too: the tongs take an object by a box the
        // cutter has used up).
        if (!b.sprite) continue;
        // The box's middle, turned with its guide about the guide's position.
        const Shape* s = const_cast<Sprite*>(b.sprite)->currentShape();
        float cx = b.rect.origin.x + b.rect.size.width * 0.5f - s->pos.x;
        float cy = b.rect.origin.y + b.rect.size.height * 0.5f - s->pos.y;
        double r = s->rotation * 3.141592653589793 / 180.0;
        Point w{static_cast<float>(cx * std::cos(r) - cy * std::sin(r)) + s->pos.x,
                static_cast<float>(cx * std::sin(r) + cy * std::cos(r)) + s->pos.y};
        Point p = gameState->worldToScreen(w);
        platform::debugTrace("  box %.0f,%.0f %.1f%s", p.x, p.y, b.health, note);
    }
}

void InjuryGraphic::traceWorked() {
    for (InjuryGraphic* i : gameState->_injuries) {
        if (!i->worked) continue;
        i->worked = false;
        i->trace("worked");
    }
}

void InjuryGraphic::traceMoved() {
    if (!platform::traceEnabled() || !hitboxes || !hitboxes[0].sprite) return;
    const HitBox& b = hitboxes[0];
    Point p = gameState->worldToScreen({b.rect.origin.x + b.rect.size.width * 0.5f, b.rect.origin.y + b.rect.size.height * 0.5f});
    bool open = !invincible;
    if (std::fabs(p.x - tracedAt.x) < 12 && std::fabs(p.y - tracedAt.y) < 12 && open == tracedOpen) return;
    tracedAt = p;
    tracedOpen = open;
    platform::debugTrace("injury moved #%llu at %.0f,%.0f open %d", serial(), p.x, p.y, open ? 1 : 0);
}

const StageInfo& InjuryGraphic::stage(int n) const {
    static const StageInfo none;
    return n >= 1 && n <= static_cast<int>(stageArray.size()) ? stageArray[n - 1] : none;
}

void InjuryGraphic::initStageArray(const std::vector<StageInfo>& stages) {
    stageArray.assign(stages.begin(), stages.begin() + std::min<size_t>(stages.size(), maxStages));
    stageArray.resize(maxStages);
}

void InjuryGraphic::initOwnStages(const std::string& fallback) {
    const std::vector<StageInfo>& own = mInjuryData ? stageTable(mInjuryData->type) : stageTable(fallback);
    const std::vector<StageInfo>& t = own.empty() ? stageTable(fallback) : own;
    maxStages = static_cast<int>(t.size());
    initStageArray(t);
}

void InjuryGraphic::render() {
    if (!alive) return;
    if (++damageTimer >= current().damagePeriod) {
        damageTimer = 0;
        gameState->health = gameState->health + current().damage * -2.0f;
    }
    if (++specialTimer >= current().specialPeriod) special();
}

void InjuryGraphic::createEffect(int count, int velocity) {
    Injury* e = new Injury;
    e->mID = 0;
    e->x = static_cast<int>(gameState->mouseX);
    e->y = static_cast<int>(gameState->mouseY);
    e->rotation = 0;
    e->scale = 1;
    for (int i = 0; i < count; ++i) gameState->addBlood(e, velocity);
    releaseRef(e);
}

void InjuryGraphic::setup() {
    invincible = false;
    invincibleTimer = 45;
    damageTimer = 0;
    specialTimer = static_cast<int>(static_cast<float>(current().specialPeriod) * percent());
    int n = mSpriteArray.count();
    if (n == 1 || (forcePosition && n > 0)) {
        Shape* s = mSpriteArray[0]->currentShape();
        s->pos = picturePlace();
        s->rotation = static_cast<float>(mInjury->rotation);
    }
}

void InjuryGraphic::special() {
    float x, y;
    if (mSpriteArray.count() == 0) {
        x = static_cast<float>(mInjury->x);
        y = static_cast<float>(mInjury->y);
    } else {
        Shape* s = mSpriteArray[0]->currentShape();
        x = s->pos.x;
        y = s->pos.y;
    }
    specialTimer = static_cast<int>(static_cast<float>(current().specialPeriod) * percent() * 0.5f);
    const std::string& effect = current().effect;
    if (effect == "blood") {
        // A drop of blood 32 points along the injury's turn.
        Injury* b = new Injury;
        b->mID = 0;
        double a = static_cast<float>(mInjury->rotation + 180) * 3.141592653589793 / 180.0;
        b->y = static_cast<int>(static_cast<double>(static_cast<float>(static_cast<int>(y))) + std::cos(a) * -32.0);
        b->x = static_cast<int>(static_cast<double>(static_cast<float>(static_cast<int>(x))) + std::sin(a) * 32.0);
        b->rotation = 0;
        b->scale = 1;
        gameState->addBlood(b, -1);
        releaseRef(b);
    } else if (effect == "smallCut") {
        // A new small cut where it is, at a random turn, kept on the body.
        Injury* c = new Injury;
        c->mID = 1;
        c->x = static_cast<int>(x);
        c->y = static_cast<int>(y);
        c->rotation = static_cast<int>(percent() * 360.0f);
        c->scale = 1;
        if (c->y < 80) c->y = 80;
        if (c->x < 100) c->x = 100;
        if (c->x > 350) c->x = 350;
        if (c->y < 20) c->y = 20;
        if (c->y > 280) c->y = 280;
        gameState->addInjury(c, false, false);
        releaseRef(c);
        Injury* b = new Injury;
        b->mID = 0;
        b->x = static_cast<int>(x);
        b->y = static_cast<int>(y);
        b->rotation = 0;
        b->scale = 1;
        gameState->addBlood(b, -1);
        releaseRef(b);
    }
}

}  // namespace as2
