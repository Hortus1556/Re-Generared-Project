// One tracked touch (port of touchData): start point, current point and the
// points it moved through since the game loop last consumed it.
#pragma once

#include <vector>

#include "core/Object.h"

class TouchData : public Object {
public:
    float startX = 0, startY = 0;
    float curX = 0, curY = 0;
    bool isSetInCycle = false;  // moved since the last game-loop pass
    int touchIndex = 0;
    std::vector<Point> touchPoint;
    int pointCount = 0;

    static TouchData* initWithStartXStartYTouchIndex(float x, float y, int index) {
        TouchData* t = new TouchData;
        t->startX = t->curX = x;
        t->startY = t->curY = y;
        t->touchIndex = index;
        t->touchPoint.push_back({x, y});
        t->pointCount = 1;
        return t;
    }

    // -copyWithZone: — note the copy's pointCount is the number of points,
    // so locationInView: on a copy reports the current point.
    TouchData* copy() const {
        TouchData* t = new TouchData;
        t->startX = startX;
        t->startY = startY;
        t->curX = curX;
        t->curY = curY;
        t->touchIndex = touchIndex;
        t->touchPoint = touchPoint;
        t->pointCount = static_cast<int>(touchPoint.size());
        return t;
    }

    bool getIsSetInCycle() const { return isSetInCycle; }
    int getMoveCount() const { return static_cast<int>(touchPoint.size()); }

    void setMoveXY(float x, float y) {
        curX = x;
        curY = y;
        isSetInCycle = true;
        touchPoint.push_back({x, y});
    }
    void clearPosition() {
        isSetInCycle = false;
        touchPoint.clear();
        pointCount = 0;
    }

    // Replays recorded points one per call, then the current point.
    Point locationInView() {
        if (pointCount < static_cast<int>(touchPoint.size())) return touchPoint.at(pointCount++);
        return {curX, curY};
    }
    // The point before the one locationInView() returned last (the current
    // point when there is none).
    Point previousLocationInView() const {
        int i = pointCount - 2;
        if (i >= 0 && i < static_cast<int>(touchPoint.size())) return touchPoint[i];
        return {curX, curY};
    }
};
