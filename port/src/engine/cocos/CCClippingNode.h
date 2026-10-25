// cocos2d-x 2.1.1's CCClippingNode (misc_nodes/CCClippingNode), ported
// (MIT; see LICENSE-cocos2d-x.txt): its children are drawn only where its
// stencil node draws. 2.1.1 used the GL stencil buffer; the port uses the
// engine's masks (platform/DrawList.h: stencilMode, maskLevel), which take
// the stencil's texels over half opaque, as 2.1.1's default alpha threshold
// of 1 keeps every drawn pixel of a solid stencil. An inverted clipping node
// is not supported (the games don't use one).
#pragma once

#include "engine/cocos/CCNode.h"

namespace cocos2d {

class CCClippingNode : public CCNode {
public:
    static CCClippingNode* create();
    static CCClippingNode* create(CCNode* stencil);
    ~CCClippingNode() override;
    bool init() override;
    bool init(CCNode* stencil);
    void onEnter() override;
    void onEnterTransitionDidFinish() override;
    void onExitTransitionDidStart() override;
    void onExit() override;
    void visit() override;

    CCNode* getStencil() const { return m_pStencil; }
    void setStencil(CCNode* stencil);
    float getAlphaThreshold() const { return m_fAlphaThreshold; }
    void setAlphaThreshold(float t) { m_fAlphaThreshold = t; }
    bool isInverted() const { return m_bInverted; }
    void setInverted(bool inverted) { m_bInverted = inverted; }

private:
    CCNode* m_pStencil = nullptr;
    float m_fAlphaThreshold = 1;
    bool m_bInverted = false;
};

}  // namespace cocos2d
