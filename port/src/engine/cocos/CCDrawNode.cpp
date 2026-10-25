#include "engine/cocos/CCDrawNode.h"

#include <cmath>

#include "platform/DrawList.h"

namespace cocos2d {

CCDrawNode* CCDrawNode::create() {
    CCDrawNode* n = new CCDrawNode;
    if (n->init()) {
        n->autorelease();
        return n;
    }
    delete n;
    return nullptr;
}

bool CCDrawNode::init() { return true; }

void CCDrawNode::addTriangle(const CCPoint& a, const CCPoint& b, const CCPoint& c, const ccColor4F& color) {
    for (const CCPoint* p : {&a, &b, &c}) {
        m_xy.push_back(p->x);
        m_xy.push_back(p->y);
        m_rgba.push_back(color.r);
        m_rgba.push_back(color.g);
        m_rgba.push_back(color.b);
        m_rgba.push_back(color.a);
    }
}

void CCDrawNode::drawDot(const CCPoint& pos, float radius, const ccColor4F& color) {
    // (2.1.1: one quad with a round fade; here a 16-sided disc)
    const int n = 16;
    for (int i = 0; i < n; ++i) {
        float a0 = 2.0f * 3.14159265f * i / n, a1 = 2.0f * 3.14159265f * (i + 1) / n;
        addTriangle(pos, ccp(pos.x + radius * std::cos(a0), pos.y + radius * std::sin(a0)),
                    ccp(pos.x + radius * std::cos(a1), pos.y + radius * std::sin(a1)), color);
    }
}

void CCDrawNode::drawSegment(const CCPoint& from, const CCPoint& to, float radius, const ccColor4F& color) {
    // a band `radius` wide each side of the line, with round ends
    CCPoint d = ccpSub(to, from);
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len > 0) {
        CCPoint n = ccp(-d.y / len * radius, d.x / len * radius);
        CCPoint a = ccpAdd(from, n), b = ccpSub(from, n), c = ccpAdd(to, n), e = ccpSub(to, n);
        addTriangle(a, b, c, color);
        addTriangle(b, e, c, color);
    }
    drawDot(from, radius, color);
    drawDot(to, radius, color);
}

void CCDrawNode::drawPolygon(CCPoint* verts, unsigned int count, const ccColor4F& fillColor, float borderWidth,
                             const ccColor4F& borderColor) {
    // 2.1.1's shape (fixed: the port drew round-ended segments half the
    // border wide, so a stencil polygon came out 0.5 point smaller all round
    // and rounded): each corner pushed along its mitred normal, (n1 + n2) /
    // (n1·n2 + 1); a fan of the (convex) polygon, then a band along each
    // edge, from -borderWidth to +borderWidth with an outline (one only when
    // the fill shows), else 2.1.1's antialiased fringe, ±0.5, here solid in
    // the fill colour (with the fill inset by 0.5 under it, as 2.1.1)
    if (count < 3) return;
    std::vector<CCPoint> offset(count);
    auto normalized = [](CCPoint v) {
        float l = std::sqrt(v.x * v.x + v.y * v.y);
        return l > 0 ? ccp(v.x / l, v.y / l) : CCPointZero;
    };
    for (unsigned int i = 0; i < count; ++i) {
        CCPoint v0 = verts[(i + count - 1) % count], v1 = verts[i], v2 = verts[(i + 1) % count];
        CCPoint n1 = normalized(ccp(-(v1.y - v0.y), v1.x - v0.x));  // v2fperp
        CCPoint n2 = normalized(ccp(-(v2.y - v1.y), v2.x - v1.x));
        float k = 1.0f / (n1.x * n2.x + n1.y * n2.y + 1.0f);
        offset[i] = ccp((n1.x + n2.x) * k, (n1.y + n2.y) * k);
    }
    bool outline = fillColor.a > 0 && borderWidth > 0;
    float inset = outline ? 0.0f : 0.5f;
    auto moved = [&](unsigned int i, float by) { return ccp(verts[i].x + offset[i].x * by, verts[i].y + offset[i].y * by); };
    for (unsigned int i = 0; i + 2 < count; ++i) addTriangle(moved(0, -inset), moved(i + 1, -inset), moved(i + 2, -inset), fillColor);
    float band = outline ? borderWidth : 0.5f;
    const ccColor4F& bandColor = outline ? borderColor : fillColor;
    for (unsigned int i = 0; i < count; ++i) {
        unsigned int j = (i + 1) % count;
        CCPoint inner0 = moved(i, -band), inner1 = moved(j, -band), outer0 = moved(i, band), outer1 = moved(j, band);
        addTriangle(inner0, inner1, outer1, bandColor);
        addTriangle(inner0, outer0, outer1, bandColor);
    }
}

void CCDrawNode::clear() {
    m_xy.clear();
    m_rgba.clear();
}

void CCDrawNode::draw() {
    if (m_xy.empty()) return;
    std::vector<unsigned short> idx(m_xy.size() / 2);
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<unsigned short>(i);
    platform::drawMesh(serial(), m_xy.data(), 2, static_cast<int>(idx.size()), m_rgba.data(), idx.data(),
                       static_cast<int>(idx.size()));
}

}  // namespace cocos2d
