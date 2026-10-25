#include "as3/EffectList.h"

#include "as3/EventEffects.h"
#include "as3/HudEffects.h"
#include "as3/Log.h"
#include "as3/ToolEffects.h"
#include "as3/SurgeryData.h"

namespace mt {

using namespace cocos2d;

namespace {

// the z orders of types 4..28 (0x3f2b20)
const int kZOrders[25] = {400, 400, 275, 275, 300, 400, 275, 400, 275, 275, 275, 400, 275,
                          400, 400, 275, 400, 275, 275, 400, 400, 400, 275, 400, 400};

}  // namespace

EffectList::~EffectList() {
    for (Effect* e : m_effects) e->release();
    m_effects.clear();
}

int EffectList::zOrderOf(int type) {
    unsigned i = static_cast<unsigned>(type - 4);
    return i < 25 ? kZOrders[i] : 0x113;
}

int EffectList::add(Effect* effect, bool replace) {
    if (!effect) return -1;
    effect->retain();
    m_screen->addChild(effect, zOrderOf(effect->getType()));
    int index = listEffect(effect, replace);
    if (index < 0) {
        // Bug fixed: the original left an effect it could not list on the
        // screen for good, never updated nor removed
        m_screen->removeChild(effect, true);
        effect->release();
    }
    return index;
}

int EffectList::listEffect(Effect* effect, bool replace) {
    if (count() < kMaxEffects) {
        m_effects.push_back(effect);
        return count() - 1;
    }
    if (replace) {
        // the first goes (the second, if the first is the selected tool's)
        int victim = toolIndex == 0 ? 1 : 0;
        Effect* old = m_effects[victim];
        removeAt(victim);
        m_screen->removeChild(old, true);
        forget(old);
        old->release();
        // Bug fixed: the original then wrote the new effect over the one
        // removeAt had moved into the victim's place, which so left the
        // list (on the screen for good); the new one goes at the end
        m_effects.push_back(effect);
        return count() - 1;
    }
    logError("Trying to add effect but effect array is full. Consider incrementing DepthConstants::MaxEffectCount");
    return -1;
}

void EffectList::removeAt(int index) {
    if (index < 0 || index >= count()) return;
    m_effects[index]->onRemoved();
    // the last takes its place
    m_effects[index] = m_effects.back();
    m_effects.pop_back();
    int last = count();
    // an index kept on the moved one follows it; one kept on the removed one is cleared
    if (toolIndex == last) {
        toolIndex = index;
        return;
    }
    if (highFiveIndex == last) {
        highFiveIndex = index;
        return;
    }
    if (partnerIndex == last) {
        partnerIndex = index;
        return;
    }
    if (toolIndex == index) toolIndex = -1;
    else if (highFiveIndex == index) highFiveIndex = -1;
    else if (partnerIndex == index) partnerIndex = -1;
}

void EffectList::forget(Effect* effect) {
    if (effect == sparkle) sparkle = nullptr;  // (the original: by its type, 7)
    if (effect == static_cast<Effect*>(dyingFade)) dyingFade = nullptr;  // (not in the original: it kept a dangling pointer)
    if (effect == currentToolEffect) currentToolEffect = nullptr;
}

void EffectList::clear() {
    for (Effect* e : m_effects) {
        m_screen->removeChild(e, true);
        forget(e);
        e->release();
    }
    // (the original then released the selected tool's effect if it was still
    // alive and not listed, a case that cannot arise: the tool's effect is
    // never taken off the list)
    currentToolEffect = nullptr;
    toolIndex = -1;
    highFiveIndex = -1;
    partnerIndex = -1;
    m_effects.clear();
}

void EffectList::update(float dt, Listener* listener) {
    for (int i = 0; i < count(); ++i) {
        Effect* e = m_effects[i];
        e->update(dt);
        if (i == toolIndex || e->isAlive()) continue;
        if (listener) listener->effectFinished(e);
        removeAt(i);
        m_screen->removeChild(e, true);
        forget(e);
        e->release();
        --i;
    }
}

void EffectList::updateHighFive(float dt, Listener* listener) {
    Effect* e = highFive();
    if (!e) return;
    e->update(dt);
    if (e->isAlive()) return;
    m_screen->removeChild(e, true);
    removeAt(highFiveIndex);
    highFiveIndex = -1;
    forget(e);
    if (listener) listener->effectFinished(e);
    e->release();
}

void EffectList::removeType(int type) {
    for (int i = 0; i < count(); ++i) {
        Effect* e = m_effects[i];
        if (e->getType() != type) continue;
        removeAt(i);
        m_screen->removeChild(e, true);
        forget(e);
        e->release();
        --i;
    }
}

void EffectList::renewType(int type) {
    for (Effect* e : m_effects)
        if (e->getType() == type) e->renew();
}

std::vector<Effect*> EffectList::activeOfType(int type) {
    std::vector<Effect*> out;
    for (Effect* e : m_effects)
        if (e->getType() == type && !e->isEnding()) out.push_back(e);
    return out;
}

void EffectList::removeTutorialEffects() {
    for (int i = 0; i < count(); ++i) {
        Effect* e = m_effects[i];
        if (e->getType() != kEffectHighlight && e->getType() != kEffectTutorialArrow) continue;
        removeAt(i);
        m_screen->removeChild(e, true);
        forget(e);
        e->release();
        --i;
    }
}

// ---------------------------------------------------------------- the tool's effect

Effect* EffectList::makeToolEffect(int tool, const ToolContext& context) {
    if (currentToolEffect) currentToolEffect->touchEnded(context.touch);
    switch (tool) {
    case kLighter:
        return LighterEffect::create();
    case kCream:
        return CreamEffect::create();
    case kChainsaw:
        return ChainsawShakeEffect::create(context.stageBackground, context.bg, context.injuryLayer,
                                           context.bgExtras);
    case kScanner:
        return ScannerEffect::create(context.scannerInterval);
    default:
        return nullptr;
    }
}

Effect* EffectList::selectTool(int tool, const ToolContext& context) {
    Effect* e = makeToolEffect(tool, context);
    currentToolEffect = e;
    toolIndex = e ? add(e, true) : -1;
    return e;
}

void EffectList::partnerTouchBegan(const CCPoint& p) {
    if (Effect* e = partnerCursor()) e->touchBegan(p);
}

void EffectList::toolTouchBegan(const CCPoint& p) {
    if (Effect* e = toolEffect()) e->touchBegan(p);
}

void EffectList::partnerTouchMoved(const CCPoint& p) {
    if (Effect* e = partnerCursor()) e->touchMoved(p);
}

void EffectList::toolTouchMoved(const CCPoint& p) {
    if (Effect* e = toolEffect()) e->touchMoved(p);
}

void EffectList::partnerTouchEnded(const CCPoint& p) {
    if (Effect* e = partnerCursor()) e->touchEnded(p);
}

void EffectList::toolTouchEnded(const CCPoint& p) {
    if (Effect* e = toolEffect()) e->touchEnded(p);
}

void EffectList::toolStopped() {
    if (Effect* e = toolEffect()) e->toolStopped();
}

// ---------------------------------------------------------------- the events' effects

Effect* EffectList::eventEffect(int kind, const CCPoint& at, UIInstance* hud) {
    Effect* e = nullptr;
    switch (kind) {
    case 0:
        e = DamageEffect::create();
        break;
    case 1:
        e = ShockEffect::create();
        break;
    case 2:
        e = ElectricityEffect::create();
        e->setPosition(at);
        break;
    case 3:
        e = BloodSpurtEffect::create();
        e->setPosition(at);
        break;
    case 4:
        e = BluePoisonCloudEffect::create();
        e->setPosition(at);
        break;
    case 5:
        // one blue wash at a time
        if (activeOfType(kEffectBlueScreen).empty()) e = BlueScreenEffect::create();
        break;
    case 6:
        e = ExplosionEffect::create();
        e->setPosition(at);
        break;
    case 7: {
        FrozenToolsEffect* f = FrozenToolsEffect::create();
        if (hud) f->addToolAnimations(*hud);
        e = f;
        break;
    }
    case 8:
        e = WhiteScreenEffect::create();
        break;
    case 10: {
        PoisonedToolsEffect* p = PoisonedToolsEffect::create();
        if (hud) p->addToolAnimations(*hud);
        e = p;
        break;
    }
    case 11:
        e = GreenScreenEffect::create();
        break;
    default:
        break;
    }
    if (e) add(e, true);
    return e;
}

}  // namespace mt
