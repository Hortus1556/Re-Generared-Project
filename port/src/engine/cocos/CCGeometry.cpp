// cocos2d-x 2.1.1's CCAffineTransform.cpp and CCPointExtension.cpp, ported.
// MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCGeometry.h"

namespace cocos2d {

CCRect CCRectApplyAffineTransform(const CCRect& rect, const CCAffineTransform& t) {
    float top = rect.getMinY(), left = rect.getMinX(), right = rect.getMaxX(), bottom = rect.getMaxY();
    CCPoint topLeft = CCPointApplyAffineTransform(CCPointMake(left, top), t);
    CCPoint topRight = CCPointApplyAffineTransform(CCPointMake(right, top), t);
    CCPoint bottomLeft = CCPointApplyAffineTransform(CCPointMake(left, bottom), t);
    CCPoint bottomRight = CCPointApplyAffineTransform(CCPointMake(right, bottom), t);
    float minX = std::min(std::min(topLeft.x, topRight.x), std::min(bottomLeft.x, bottomRight.x));
    float maxX = std::max(std::max(topLeft.x, topRight.x), std::max(bottomLeft.x, bottomRight.x));
    float minY = std::min(std::min(topLeft.y, topRight.y), std::min(bottomLeft.y, bottomRight.y));
    float maxY = std::max(std::max(topLeft.y, topRight.y), std::max(bottomLeft.y, bottomRight.y));
    return CCRectMake(minX, minY, maxX - minX, maxY - minY);
}

bool ccpLineIntersect(const CCPoint& A, const CCPoint& B, const CCPoint& C, const CCPoint& D, float* S, float* T) {
    if ((A.x == B.x && A.y == B.y) || (C.x == D.x && C.y == D.y)) return false;  // a line undefined
    const float BAx = B.x - A.x, BAy = B.y - A.y;
    const float DCx = D.x - C.x, DCy = D.y - C.y;
    const float ACx = A.x - C.x, ACy = A.y - C.y;
    const float denom = DCy * BAx - DCx * BAy;
    *S = DCx * ACy - DCy * ACx;
    *T = BAx * ACy - BAy * ACx;
    if (denom == 0) return *S == 0 || *T == 0;  // incident, or parallel
    *S = *S / denom;
    *T = *T / denom;
    return true;
}

bool ccpSegmentIntersect(const CCPoint& A, const CCPoint& B, const CCPoint& C, const CCPoint& D) {
    float S, T;
    return ccpLineIntersect(A, B, C, D, &S, &T) && S >= 0.0f && S <= 1.0f && T >= 0.0f && T <= 1.0f;
}

CCPoint ccpIntersectPoint(const CCPoint& A, const CCPoint& B, const CCPoint& C, const CCPoint& D) {
    float S, T;
    if (ccpLineIntersect(A, B, C, D, &S, &T)) return CCPoint(A.x + S * (B.x - A.x), A.y + S * (B.y - A.y));
    return CCPointZero;
}

}  // namespace cocos2d
