#include "as3/HudEffects.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

// the feedback words (0x3f2e80, 0x14 bytes each) and the medals (0x3f2e1c)
const char* const kWords[19] = {"GOOD!",   "NICE!",   "NEAT!",    "GREAT!",    "SWEET!",      "YEAH!",   "AMAZING!",
                                "FANTASTIC!", "DELICIOUS!", "YEEAAAH!!", "WOOOOO!!", "AWESOME!!!", "NOOO!", "FAIL!",
                                "MISS!",   "TOO FAST!", "SLOW DOWN!", "WRONG TOOL", "NAILED IT"};
const char* const kMedals[5] = {"bronze", "silver", "gold", "diamond", "rainbow"};

void setOpacityOf(CCNode* n, GLubyte o) {
    if (CCRGBAProtocol* p = dynamic_cast<CCRGBAProtocol*>(n)) p->setOpacity(o);
}

// in at once, held half a second, out over one (the popup's parts)
CCAction* popupFade() {
    return CCSequence::create(CCFadeTo::create(0.05f, 0xff), CCDelayTime::create(0.5f), CCFadeTo::create(1, 0),
                              nullptr);
}

}  // namespace

// ---------------------------------------------------------------- DyingFadeEffect

DyingFadeEffect* DyingFadeEffect::create() {
    DyingFadeEffect* e = new DyingFadeEffect;
    e->m_type = kEffectDyingFade;
    e->initFade();
    // (the original did not autorelease it: it was never freed)
    e->autorelease();
    return e;
}

bool DyingFadeEffect::initFade() {
    CCSize win = CCDirector::sharedDirector()->getWinSize();
    // white, transparent until the health falls: each half of the screen,
    // the edge one way, the middle the other
    m_left = CCLayerGradient::create(ccc4(255, 255, 255, 0), ccc4(255, 255, 255, 0), ccp(-1, 0));
    m_left->setContentSize(CCSizeMake(win.width * 0.5f, win.height));
    m_left->retain();
    addChild(m_left);
    m_right = CCLayerGradient::create(ccc4(255, 255, 255, 0), ccc4(255, 255, 255, 0), ccp(1, 0));
    m_right->setContentSize(CCSizeMake(win.width * 0.5f, win.height));
    m_right->setPosition(ccp(win.width * 0.5f, 0));
    m_right->retain();
    addChild(m_right);
    return true;
}

DyingFadeEffect::~DyingFadeEffect() {
    CC_SAFE_RELEASE(m_left);
    CC_SAFE_RELEASE(m_right);
    removeAllChildrenWithCleanup(true);
}

void DyingFadeEffect::setHealth(float health) {
    int middle = 0, edge = 0;
    if (health < 15) {
        float t = health / 15.0f;
        edge = static_cast<int>(180.0 - std::floor(t * 180.0f));
        middle = static_cast<int>(100.0 - std::floor(t * 100.0f));
    }
    m_left->setStartOpacity(static_cast<GLubyte>(middle));
    m_right->setStartOpacity(static_cast<GLubyte>(middle));
    m_left->setEndOpacity(static_cast<GLubyte>(edge));
    m_right->setEndOpacity(static_cast<GLubyte>(edge));
}

// ---------------------------------------------------------------- Success, OutOfTime

bool EndPopupEffect::initPopup(const char* frame) {
    m_type = kEffectEndPopup;
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    CCSprite* s = CCSprite::createWithSpriteFrameName(frame);
    if (!s) {
        logError("EndPopupEffect: %s not found", frame);
        return true;
    }
    s->setAnchorPoint(ccp(0.5f, 0.5f));
    s->setPosition(ccp(r->width * 0.5f, r->height * 0.5f));
    s->setScale(0.1f);
    s->runAction(CCEaseBounceInOut::create(CCScaleTo::create(0.5f, 1)));
    addChild(s);
    return true;
}

SuccessEffect* SuccessEffect::create() {
    SuccessEffect* e = new SuccessEffect;
    e->initPopup("ingame_popup-success.png");
    e->autorelease();
    return e;
}

OutOfTimeEffect* OutOfTimeEffect::create() {
    OutOfTimeEffect* e = new OutOfTimeEffect;
    e->initPopup("ingame_popup-outoftime.png");
    e->autorelease();
    return e;
}

// ---------------------------------------------------------------- HighlightEffect

HighlightEffect* HighlightEffect::create(InjuryState* injury) {
    HighlightEffect* e = new HighlightEffect;
    e->m_type = kEffectHighlight;  // (0x1d9f14)
    e->m_injury = injury;
    e->m_particles = retainedParticles("particle_highlightFX");
    place(e->m_particles, CCPointZero);
    if (e->m_particles) e->addChild(e->m_particles);
    e->autorelease();
    return e;
}

HighlightEffect::~HighlightEffect() {
    releaseParticles(m_particles);
    removeAllChildrenWithCleanup(true);
}

void HighlightEffect::update(float) { setPosition(m_injury->position); }

// ---------------------------------------------------------------- SparkleEffect

SparkleEffect* SparkleEffect::create() {
    SparkleEffect* e = new SparkleEffect;
    e->m_type = kEffectSparkle;
    if (ParticleSystem* p = ParticleSystem::create("particle_tagFX")) {  // (0x111e54)
        p->setDuration(-1);  // for ever
        e->addChild(p);
        e->m_systems.push_back(p);
        p->retain();
    }
    e->autorelease();
    return e;
}

SparkleEffect::~SparkleEffect() {
    for (ParticleSystem* p : m_systems) p->release();
    removeAllChildrenWithCleanup(true);
}

bool SparkleEffect::isAlive() {
    for (ParticleSystem* p : m_systems)
        if (p->isActive() || p->getParticleCount() != 0) return true;
    return false;
}

void SparkleEffect::restart() {
    for (ParticleSystem* p : m_systems) p->resetSystem();
}

void SparkleEffect::stop() {
    for (ParticleSystem* p : m_systems) p->stopSystem();
}

void SparkleEffect::setPosition(const CCPoint& p) {
    for (ParticleSystem* s : m_systems) s->setPosition(p);
}

// ---------------------------------------------------------------- TagEffect

const char* TagEffect::word(int kind, int level) {
    // Bug fixed: under 10 points (level 0) the original read before its
    // table; the level is taken as at least 1.
    level = std::max(level, 1);
    int i = 18;
    switch (kind) {
    case kGood:
    case kPartner:
        if (level < 4) i = randomInt(0, 2) + level * 3 - 3;
        else if (level == 4) i = randomInt(0, 1) + 9;
        else i = 11;
        break;
    case kCombo:
        break;  // NAILED IT
    case kMiss:
        i = randomInt(0, 2) + 12;
        break;
    case kTooFast:
        i = randomInt(0, 1) + 15;
        break;
    case kWrongTool:
        i = 17;
        break;
    default:
        i = 0;
    }
    return kWords[i];
}

TagEffect* TagEffect::create(int kind, int points, bool halve) {
    TagEffect* e = new TagEffect;
    e->m_type = kEffectTag;  // (its constructor, 0xa9dc0)
    e->initTag(kind, points, halve);
    e->autorelease();
    return e;
}

bool TagEffect::initTag(int kind, int points, bool halve) {
    CCSpriteFrame* bronze =
        CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName("ingame_popup_box-bronze.png");
    CCSize boxSize = bronze ? bronze->getOriginalSize() : CCSizeZero;
    setContentSize(boxSize);
    // the layout's box is about canvas (80, 32): its root is moved so that
    // that point is the effect's origin, wherever the screen shows the canvas
    m_root = CCNode::create();
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    float s = r->canvasScale;
    float x = 80 - (640 - r->width / s) * 0.5f, y = 32 - (384 - r->height / s) * 0.5f;
    m_root->setPosition(ccp(-(s * x), -(s * y)));
    m_root->setAnchorPoint(CCPointZero);
    addChild(m_root);
    m_def = loadUIDefinition("common/ui_feedback_popups.json");
    buildUI(m_ui, m_root, m_def, nullptr, nullptr);

    int value = halve ? points / 2 : points;
    int level = value / 10;
    const char* text = word(kind, level);
    // Bug fixed as in word(): the medal of level 0 was read before the table
    const char* medal = kMedals[std::min(std::max(level, 1), 5) - 1];
    char numbers[16];
    std::snprintf(numbers, sizeof numbers, "+%d", points);
    switch (kind) {
    case kGood:
    case kCombo:
        m_ui.setItemImage(m_def, "ingame_popup_box", medal);
        m_ui.setText("popup_numbers", numbers);
        m_ui.setText("popup_text", text);
        m_ui.setText("popup_fail_numbers", "");
        m_ui.setText("popup_smallfail", "");
        break;
    case kMiss:
    case kWrongTool:
        m_ui.setItemImage(m_def, "ingame_popup_box", "fail");
        m_ui.setText("popup_numbers", "");
        m_ui.setText("popup_text", "");
        m_ui.setText("popup_fail_numbers", text);
        m_ui.setText("popup_smallfail", "");
        break;
    case kTooFast:
        m_ui.setItemImage(m_def, "ingame_popup_box", "fail");
        m_ui.setText("popup_numbers", "");
        m_ui.setText("popup_text", "");
        m_ui.setText("popup_fail_numbers", "");
        m_ui.setText("popup_smallfail", text);
        break;
    case kPartner:
        m_ui.setItemImage(m_def, "ingame_popup_box", medal);
        m_ui.setText("popup_numbers", "");
        // the words alone: a third of the box higher
        if (CCNode* label = m_ui.label("popup_text")) label->setPositionY(label->getPositionY() + boxSize.height / 3.0f);
        m_ui.setText("popup_text", text);
        m_ui.setText("popup_fail_numbers", "");
        m_ui.setText("popup_smallfail", "");
        break;
    default:
        break;
    }
    m_box = m_ui.node("ingame_popup_box");
    m_numbers = m_ui.node("popup_numbers");
    m_text = m_ui.node("popup_text");
    m_failNumbers = m_ui.node("popup_fail_numbers");

    // spun in upside down (a little off), grown at once, faded out
    setRotation(static_cast<float>(randomInt(-17, 17) - 180));
    runAction(CCEaseIn::create(CCRotateBy::create(0.2f, 180), 2));
    setScale(0.1f);
    runAction(CCScaleTo::create(0.05f, 1));
    for (CCNode* n : {m_box, m_numbers, m_text, m_failNumbers}) {
        if (!n) continue;
        setOpacityOf(n, 0);
        n->runAction(popupFade());
    }
    return true;
}

TagEffect::~TagEffect() {}

bool TagEffect::isAlive() { return runsActions(m_box) || runsActions(m_numbers) || runsActions(m_failNumbers); }

// ---------------------------------------------------------------- CoinTagEffect

CoinTagEffect* CoinTagEffect::create() {
    CoinTagEffect* e = new CoinTagEffect;
    e->m_type = kEffectCoinTag;
    SoundMgr::sharedSoundMgr()->playSound("Nowe_bleep", false);  // (0x2d4e58)
    e->m_time = 0;
    e->m_particles = retainedParticles("particle_tagfilthyFX");
    if (e->m_particles) e->addChild(e->m_particles);
    e->autorelease();
    return e;
}

CoinTagEffect::~CoinTagEffect() {
    releaseParticles(m_particles);
    removeAllChildrenWithCleanup(true);
}

bool CoinTagEffect::isAlive() { return hasParticles(m_particles) || m_time < 3; }

void CoinTagEffect::setPosition(const CCPoint& p) { place(m_particles, ccp(p.x, p.y * 1.25f)); }

// ---------------------------------------------------------------- TutorialArrowEffect

TutorialArrowEffect* TutorialArrowEffect::create(CCNode* item, int mode) {
    TutorialArrowEffect* e = new TutorialArrowEffect;
    e->initArrow(item, mode);
    e->autorelease();
    return e;
}

bool TutorialArrowEffect::initArrow(CCNode* item, int mode) {
    m_type = kEffectTutorialArrow;
    // (by its bounding box: the item's place and size as scaled, as the
    // port's bar across the top scales the tools' buttons; the same as its
    // position and content size for the original's unscaled items)
    CCPoint at = item ? item->boundingBox().origin : CCPointZero;
    CCSize size = item ? item->boundingBox().size : CCSizeZero;
    CCSprite* arrow = CCSprite::createWithSpriteFrameName("ingame_tutorial_arrow-normal.png");
    if (!arrow) return true;
    arrow->setAnchorPoint(ccp(1, 0.5f));  // by its tip
    float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    CCPoint start, away;
    if (mode == kUIRight) {
        // left of the item, pointing right
        start = ccp(at.x, at.y + size.height * 0.5f);
        away = ccp(at.x - 20 * s, start.y);
    } else if (mode == kUILeft) {
        // right of it, pointing left
        arrow->setRotation(180);
        start = ccp(at.x + size.width, at.y + size.height * 0.5f);
        away = ccp(start.x + 20 * s, start.y);
    } else if (mode >= kUITopLeft && mode <= kUITopRight) {
        // under it, pointing up
        arrow->setRotation(270);
        start = ccp(at.x + size.width * 0.5f, at.y);
        away = ccp(start.x, at.y - 20 * s);
    } else {
        // (the original went on with no action and crashed)
        logError("UIPosition mode %d not supported", mode);
        return true;
    }
    arrow->setPosition(start);
    CCActionInterval* out = CCEaseInOut::create(CCMoveTo::create(0.4f, away), 2);
    CCActionInterval* back = CCEaseInOut::create(CCMoveTo::create(0.4f, start), 2);
    arrow->runAction(CCRepeatForever::create(static_cast<CCActionInterval*>(CCSequence::create(out, back, nullptr))));
    addChild(arrow);
    return true;
}

// ---------------------------------------------------------------- TutorialGradientEffect

TutorialGradientEffect* TutorialGradientEffect::create() {
    TutorialGradientEffect* e = new TutorialGradientEffect;
    e->m_type = kEffectTutorialGradient;
    e->initGradient();
    e->autorelease();
    return e;
}

bool TutorialGradientEffect::initGradient() {
    CCSize win = CCDirector::sharedDirector()->getWinSize();
    // dark at the bottom and top edges, a quarter of the screen high
    m_bottom = CCLayerGradient::create(ccc4(0, 0, 0, 0), ccc4(0, 0, 0, 0x96), ccp(0, -1));
    m_bottom->setContentSize(CCSizeMake(win.width, win.height * 0.25f));
    m_top = CCLayerGradient::create(ccc4(0, 0, 0, 0), ccc4(0, 0, 0, 0x96), ccp(0, 1));
    m_top->setContentSize(CCSizeMake(win.width, win.height * 0.25f));
    m_top->setPosition(ccp(0, win.height * 0.75f));
    m_bottom->retain();
    m_top->retain();
    m_bottom->setOpacity(0);
    m_top->setOpacity(0);
    m_bottom->runAction(CCFadeIn::create(0.5f));
    m_top->runAction(CCFadeIn::create(0.5f));
    addChild(m_bottom);
    addChild(m_top);
    m_fading = false;
    return true;
}

TutorialGradientEffect::~TutorialGradientEffect() {
    CC_SAFE_RELEASE(m_bottom);
    CC_SAFE_RELEASE(m_top);
    removeAllChildrenWithCleanup(true);
}

void TutorialGradientEffect::fadeOut() {
    m_bottom->runAction(CCSequence::create(CCDelayTime::create(0.25f), CCFadeOut::create(0.5f), nullptr));
    m_top->runAction(CCSequence::create(CCDelayTime::create(0.25f), CCFadeOut::create(0.5f), nullptr));
    m_fading = true;
}

bool TutorialGradientEffect::isAlive() {
    if (!m_fading) return true;
    return runsActions(m_bottom) || runsActions(m_top);
}

}  // namespace mt
