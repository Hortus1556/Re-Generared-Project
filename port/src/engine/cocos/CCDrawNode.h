// cocos2d-x 2.1.1's CCDrawNode (draw_nodes/CCDrawNode), ported (MIT; see
// LICENSE-cocos2d-x.txt): a node that keeps a list of filled shapes (dots,
// segments, polygons) and draws them as untextured coloured triangles, here
// through the engine's recorded meshes. 2.1.1 drew them antialiased with a
// special shader (soft edges); the port draws them solid.
#pragma once

#include <vector>

#include "engine/cocos/CCNode.h"

namespace cocos2d {

class CCDrawNode : public CCNode {
public:
    static CCDrawNode* create();
    bool init() override;
    void draw() override;

    void drawDot(const CCPoint& pos, float radius, const ccColor4F& color);
    void drawSegment(const CCPoint& from, const CCPoint& to, float radius, const ccColor4F& color);
    void drawPolygon(CCPoint* verts, unsigned int count, const ccColor4F& fillColor, float borderWidth,
                     const ccColor4F& borderColor);
    void clear();

    // its blend function (2.1.1: CC_BLEND_SRC, CC_BLEND_DST)
    ccBlendFunc getBlendFunc() const { return m_sBlendFunc; }
    void setBlendFunc(const ccBlendFunc& f) { m_sBlendFunc = f; }

private:
    void addTriangle(const CCPoint& a, const CCPoint& b, const CCPoint& c, const ccColor4F& color);
    std::vector<float> m_xy;    // 2 floats a vertex
    std::vector<float> m_rgba;  // 4 floats a vertex
    ccBlendFunc m_sBlendFunc = {GL_ONE, GL_ONE_MINUS_SRC_ALPHA};
};

}  // namespace cocos2d
