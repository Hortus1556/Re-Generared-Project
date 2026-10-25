#include "as3/ArtHit.h"

#include <cmath>

#include "engine/cocos/CCSprite.h"

namespace mt {

using namespace cocos2d;

namespace {

// how opaque a cell of the art must be to count (of 255)
constexpr int kOpaque = 32;

// a point of the parent's space in the sprite's own (cocos's node transform
// undone: its place, its turn, clockwise in degrees, its scale, its anchor)
CCPoint toSprite(CCSprite* sp, const CCPoint& p) {
    float dx = p.x - sp->getPositionX(), dy = p.y - sp->getPositionY();
    const float a = sp->getRotation() * 3.14159265f / 180.0f;
    const float c = std::cos(a), s = std::sin(a);
    float x = dx * c - dy * s, y = dx * s + dy * c;
    if (sp->getScaleX() != 0) x /= sp->getScaleX();
    if (sp->getScaleY() != 0) y /= sp->getScaleY();
    if (!sp->isIgnoreAnchorPointForPosition()) {
        x += sp->getAnchorPointInPoints().x;
        y += sp->getAnchorPointInPoints().y;
    }
    return ccp(x, y);
}

}  // namespace

bool artNear(CCSprite* sp, const CCPoint& at, float radius, const float* above) {
    if (!sp) return false;
    // the touch, and two rings about it (a finger's slack)
    auto tryAt = [&](float x, float y) {
        if (above && y < *above) return false;
        return sp->opaqueAt(toSprite(sp, ccp(x, y))) >= kOpaque;
    };
    if (tryAt(at.x, at.y)) return true;
    for (int ring = 1; ring <= 2; ++ring) {
        const float r = radius * static_cast<float>(ring) / 2.0f;
        const int n = ring * 8;
        for (int k = 0; k < n; ++k) {
            const float a = 6.2831853f * static_cast<float>(k) / static_cast<float>(n);
            if (tryAt(at.x + r * std::cos(a), at.y + r * std::sin(a))) return true;
        }
    }
    return false;
}

}  // namespace mt
