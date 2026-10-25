// cocos2d-x 2.1.1's geometry (cocoa/CCGeometry, cocoa/CCAffineTransform,
// support/CCPointExtension), ported to Bleed Engine. MIT: LICENSE-cocos2d-x.txt.
// Points are cocos's: y up, origin at the bottom left of the screen.
#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace cocos2d {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define CC_DEGREES_TO_RADIANS(__ANGLE__) ((__ANGLE__) * 0.01745329252f)  // PI / 180
#define CC_RADIANS_TO_DEGREES(__ANGLE__) ((__ANGLE__) * 57.29577951f)    // PI * 180
#define kCCPointEpsilon FLT_EPSILON

class CCPoint {
public:
    float x = 0, y = 0;
    CCPoint() = default;
    CCPoint(float x_, float y_) : x(x_), y(y_) {}
    void setPoint(float x_, float y_) { x = x_; y = y_; }
    bool equals(const CCPoint& t) const { return x == t.x && y == t.y; }
};

class CCSize {
public:
    float width = 0, height = 0;
    CCSize() = default;
    CCSize(float w, float h) : width(w), height(h) {}
    void setSize(float w, float h) { width = w; height = h; }
    bool equals(const CCSize& t) const { return width == t.width && height == t.height; }
};

class CCRect {
public:
    CCPoint origin;
    CCSize size;
    CCRect() = default;
    CCRect(float x, float y, float w, float h) : origin(x, y), size(w, h) {}
    void setRect(float x, float y, float w, float h) { origin.setPoint(x, y); size.setSize(w, h); }
    float getMinX() const { return origin.x; }
    float getMidX() const { return origin.x + size.width / 2.0f; }
    float getMaxX() const { return origin.x + size.width; }
    float getMinY() const { return origin.y; }
    float getMidY() const { return origin.y + size.height / 2.0f; }
    float getMaxY() const { return origin.y + size.height; }
    bool equals(const CCRect& r) const { return origin.equals(r.origin) && size.equals(r.size); }
    bool containsPoint(const CCPoint& p) const {
        return p.x >= getMinX() && p.x <= getMaxX() && p.y >= getMinY() && p.y <= getMaxY();
    }
    bool intersectsRect(const CCRect& r) const {
        return !(getMaxX() < r.getMinX() || r.getMaxX() < getMinX() || getMaxY() < r.getMinY() ||
                 r.getMaxY() < getMinY());
    }
};

#define CCPointMake(x, y) cocos2d::CCPoint((float)(x), (float)(y))
#define CCSizeMake(w, h) cocos2d::CCSize((float)(w), (float)(h))
#define CCRectMake(x, y, w, h) cocos2d::CCRect((float)(x), (float)(y), (float)(w), (float)(h))
const CCPoint CCPointZero(0, 0);
const CCSize CCSizeZero(0, 0);
const CCRect CCRectZero(0, 0, 0, 0);

// ---- CCAffineTransform: x' = a x + c y + tx, y' = b x + d y + ty
struct CCAffineTransform {
    float a, b, c, d;
    float tx, ty;
};

inline CCAffineTransform __CCAffineTransformMake(float a, float b, float c, float d, float tx, float ty) {
    return {a, b, c, d, tx, ty};
}
#define CCAffineTransformMake __CCAffineTransformMake
inline CCPoint __CCPointApplyAffineTransform(const CCPoint& p, const CCAffineTransform& t) {
    return CCPoint(t.a * p.x + t.c * p.y + t.tx, t.b * p.x + t.d * p.y + t.ty);
}
#define CCPointApplyAffineTransform __CCPointApplyAffineTransform
inline CCSize __CCSizeApplyAffineTransform(const CCSize& s, const CCAffineTransform& t) {
    return CCSize(t.a * s.width + t.c * s.height, t.b * s.width + t.d * s.height);
}
#define CCSizeApplyAffineTransform __CCSizeApplyAffineTransform
inline CCAffineTransform CCAffineTransformMakeIdentity() { return {1, 0, 0, 1, 0, 0}; }
const CCAffineTransform CCAffineTransformIdentity = {1, 0, 0, 1, 0, 0};
CCRect CCRectApplyAffineTransform(const CCRect& rect, const CCAffineTransform& t);
inline CCAffineTransform CCAffineTransformTranslate(const CCAffineTransform& t, float tx, float ty) {
    return {t.a, t.b, t.c, t.d, t.tx + t.a * tx + t.c * ty, t.ty + t.b * tx + t.d * ty};
}
inline CCAffineTransform CCAffineTransformRotate(const CCAffineTransform& t, float angle) {
    float s = sinf(angle), c = cosf(angle);
    return {t.a * c + t.c * s, t.b * c + t.d * s, t.c * c - t.a * s, t.d * c - t.b * s, t.tx, t.ty};
}
inline CCAffineTransform CCAffineTransformScale(const CCAffineTransform& t, float sx, float sy) {
    return {t.a * sx, t.b * sx, t.c * sy, t.d * sy, t.tx, t.ty};
}
// t1, then t2
inline CCAffineTransform CCAffineTransformConcat(const CCAffineTransform& t1, const CCAffineTransform& t2) {
    return {t1.a * t2.a + t1.b * t2.c, t1.a * t2.b + t1.b * t2.d, t1.c * t2.a + t1.d * t2.c,
            t1.c * t2.b + t1.d * t2.d, t1.tx * t2.a + t1.ty * t2.c + t2.tx, t1.tx * t2.b + t1.ty * t2.d + t2.ty};
}
inline bool CCAffineTransformEqualToTransform(const CCAffineTransform& t1, const CCAffineTransform& t2) {
    return t1.a == t2.a && t1.b == t2.b && t1.c == t2.c && t1.d == t2.d && t1.tx == t2.tx && t1.ty == t2.ty;
}
inline CCAffineTransform CCAffineTransformInvert(const CCAffineTransform& t) {
    float determinant = 1 / (t.a * t.d - t.b * t.c);
    return {determinant * t.d, -determinant * t.b, -determinant * t.c, determinant * t.a,
            determinant * (t.c * t.ty - t.d * t.tx), determinant * (t.b * t.tx - t.a * t.ty)};
}

// ---- point extension
inline CCPoint ccp(float x, float y) { return CCPoint(x, y); }
inline CCPoint ccpNeg(const CCPoint& v) { return ccp(-v.x, -v.y); }
inline CCPoint ccpAdd(const CCPoint& v1, const CCPoint& v2) { return ccp(v1.x + v2.x, v1.y + v2.y); }
inline CCPoint ccpSub(const CCPoint& v1, const CCPoint& v2) { return ccp(v1.x - v2.x, v1.y - v2.y); }
inline CCPoint ccpMult(const CCPoint& v, float s) { return ccp(v.x * s, v.y * s); }
inline CCPoint ccpMidpoint(const CCPoint& v1, const CCPoint& v2) { return ccpMult(ccpAdd(v1, v2), 0.5f); }
inline float ccpDot(const CCPoint& v1, const CCPoint& v2) { return v1.x * v2.x + v1.y * v2.y; }
inline float ccpCross(const CCPoint& v1, const CCPoint& v2) { return v1.x * v2.y - v1.y * v2.x; }
inline CCPoint ccpPerp(const CCPoint& v) { return ccp(-v.y, v.x); }
inline CCPoint ccpRPerp(const CCPoint& v) { return ccp(v.y, -v.x); }
inline CCPoint ccpProject(const CCPoint& v1, const CCPoint& v2) { return ccpMult(v2, ccpDot(v1, v2) / ccpDot(v2, v2)); }
inline CCPoint ccpRotate(const CCPoint& v1, const CCPoint& v2) {
    return ccp(v1.x * v2.x - v1.y * v2.y, v1.x * v2.y + v1.y * v2.x);
}
inline CCPoint ccpUnrotate(const CCPoint& v1, const CCPoint& v2) {
    return ccp(v1.x * v2.x + v1.y * v2.y, v1.y * v2.x - v1.x * v2.y);
}
inline float ccpLengthSQ(const CCPoint& v) { return ccpDot(v, v); }
inline float ccpDistanceSQ(const CCPoint p1, const CCPoint p2) { return ccpLengthSQ(ccpSub(p1, p2)); }
inline float ccpLength(const CCPoint& v) { return sqrtf(ccpLengthSQ(v)); }
inline float ccpDistance(const CCPoint& v1, const CCPoint& v2) { return ccpLength(ccpSub(v1, v2)); }
inline CCPoint ccpNormalize(const CCPoint& v) { return ccpMult(v, 1.0f / ccpLength(v)); }
inline CCPoint ccpForAngle(const float a) { return ccp(cosf(a), sinf(a)); }
inline float ccpToAngle(const CCPoint& v) { return atan2f(v.y, v.x); }
inline float clampf(float value, float min_inclusive, float max_inclusive) {
    if (min_inclusive > max_inclusive) std::swap(min_inclusive, max_inclusive);
    return value < min_inclusive ? min_inclusive : value < max_inclusive ? value : max_inclusive;
}
inline CCPoint ccpClamp(const CCPoint& p, const CCPoint& from, const CCPoint& to) {
    return ccp(clampf(p.x, from.x, to.x), clampf(p.y, from.y, to.y));
}
inline CCPoint ccpFromSize(const CCSize& s) { return ccp(s.width, s.height); }
inline CCPoint ccpLerp(const CCPoint& a, const CCPoint& b, float alpha) {
    return ccpAdd(ccpMult(a, 1.f - alpha), ccpMult(b, alpha));
}
inline bool ccpFuzzyEqual(const CCPoint& a, const CCPoint& b, float var) {
    return a.x - var <= b.x && b.x <= a.x + var && a.y - var <= b.y && b.y <= a.y + var;
}
inline CCPoint ccpCompMult(const CCPoint& a, const CCPoint& b) { return ccp(a.x * b.x, a.y * b.y); }
inline float ccpAngleSigned(const CCPoint& a, const CCPoint& b) {
    CCPoint a2 = ccpNormalize(a), b2 = ccpNormalize(b);
    float angle = atan2f(a2.x * b2.y - a2.y * b2.x, ccpDot(a2, b2));
    if (fabs(angle) < kCCPointEpsilon) return 0.f;
    return angle;
}
inline float ccpAngle(const CCPoint& a, const CCPoint& b) {
    float angle = acosf(clampf(ccpDot(ccpNormalize(a), ccpNormalize(b)), -1.0f, 1.0f));
    if (fabs(angle) < kCCPointEpsilon) return 0.f;
    return angle;
}
inline CCPoint ccpRotateByAngle(const CCPoint& v, const CCPoint& pivot, float angle) {
    CCPoint r = ccpSub(v, pivot);
    float cosa = cosf(angle), sina = sinf(angle);
    float t = r.x;
    r.x = t * cosa - r.y * sina + pivot.x;
    r.y = t * sina + r.y * cosa + pivot.y;
    return r;
}
// segments A-B and C-D: S, T where the lines meet (false if parallel)
bool ccpLineIntersect(const CCPoint& A, const CCPoint& B, const CCPoint& C, const CCPoint& D, float* S, float* T);
bool ccpSegmentIntersect(const CCPoint& A, const CCPoint& B, const CCPoint& C, const CCPoint& D);
CCPoint ccpIntersectPoint(const CCPoint& A, const CCPoint& B, const CCPoint& C, const CCPoint& D);

}  // namespace cocos2d
