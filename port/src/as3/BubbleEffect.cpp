#include "as3/BubbleEffect.h"

namespace mt {

BubbleEffect* BubbleEffect::create() {
    // (1.45 0x4d48bc; its constructor 0x4dcd28 sets its type 0x20) its
    // bubbles added where their plist puts them
    BubbleEffect* e = new BubbleEffect;
    e->m_type = kEffectBubble;
    e->m_bubbles = retainedParticles("particle_bubbleFX");
    if (e->m_bubbles) e->addChild(e->m_bubbles);
    e->autorelease();
    return e;
}

BubbleEffect::~BubbleEffect() {
    releaseParticles(m_bubbles);
    removeAllChildrenWithCleanup(true);
}

bool BubbleEffect::isAlive() {
    // (1.45 0x4d49a0)
    return emissionRate(m_bubbles) > 0;
}

void BubbleEffect::setToEnd() {
    // (1.45 0x4d49cc)
    setEmissionRate(m_bubbles, 0);
    if (m_bubbles) m_bubbles->setVisible(false);
}

Effect* stageEffect(const std::string& name) {
    // (1.45 0x7b0b4: only 0x20; the original then added what it got, so an
    // effect name of another kind would have crashed it)
    if (name == "BubbleEffect") return BubbleEffect::create();
    return nullptr;
}

}  // namespace mt
