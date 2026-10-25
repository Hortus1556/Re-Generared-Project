#include "engine/cocos/CCClippingNode.h"

#include "platform/DrawList.h"

namespace cocos2d {

CCClippingNode* CCClippingNode::create() { return create(nullptr); }

CCClippingNode* CCClippingNode::create(CCNode* stencil) {
    CCClippingNode* n = new CCClippingNode;
    if (n->init(stencil)) {
        n->autorelease();
        return n;
    }
    delete n;
    return nullptr;
}

CCClippingNode::~CCClippingNode() { CC_SAFE_RELEASE(m_pStencil); }

bool CCClippingNode::init() { return init(nullptr); }

bool CCClippingNode::init(CCNode* stencil) {
    setStencil(stencil);
    m_fAlphaThreshold = 1;
    m_bInverted = false;
    return true;
}

void CCClippingNode::setStencil(CCNode* stencil) {
    CC_SAFE_RETAIN(stencil);
    CC_SAFE_RELEASE(m_pStencil);
    m_pStencil = stencil;
}

// the stencil lives outside the scene graph: it is told of the node's comings and goings
void CCClippingNode::onEnter() {
    CCNode::onEnter();
    if (m_pStencil) m_pStencil->onEnter();
}

void CCClippingNode::onEnterTransitionDidFinish() {
    CCNode::onEnterTransitionDidFinish();
    if (m_pStencil) m_pStencil->onEnterTransitionDidFinish();
}

void CCClippingNode::onExitTransitionDidStart() {
    if (m_pStencil) m_pStencil->onExitTransitionDidStart();
    CCNode::onExitTransitionDidStart();
}

void CCClippingNode::onExit() {
    if (m_pStencil) m_pStencil->onExit();
    CCNode::onExit();
}

void CCClippingNode::visit() {
    // without a stencil, a plain node
    if (!m_pStencil || !m_pStencil->isVisible() || m_bInverted) {
        CCNode::visit();
        return;
    }
    if (!isVisible()) return;
    // the stencil drawn into the mask under this node's transform
    glPushMatrix();
    transform();
    platform::stencilMode(1);
    m_pStencil->visit();
    platform::stencilMode(0);
    glPopMatrix();
    platform::maskLevel(1);
    CCNode::visit();
    // and taken away again
    glPushMatrix();
    transform();
    platform::stencilMode(-1);
    m_pStencil->visit();
    platform::stencilMode(0);
    glPopMatrix();
    platform::maskLevel(-1);
}

}  // namespace cocos2d
