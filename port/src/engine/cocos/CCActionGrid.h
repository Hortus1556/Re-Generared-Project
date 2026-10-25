// cocos2d-x 2.1.1's grid actions (actions/CCActionGrid, CCActionGrid3D: the
// parts in use), ported to Bleed Engine. A grid action gives its target a
// grid (CCGrid.h) and moves the grid's corners as it runs; the grid stays on
// the target, as last moved, until CCStopGrid turns it off.
// MIT: LICENSE-cocos2d-x.txt.
#pragma once

#include "engine/cocos/CCAction.h"
#include "engine/cocos/CCGrid.h"

namespace cocos2d {

class CCGridAction : public CCActionInterval {
public:
    bool initWithDuration(float duration, const CCSize& gridSize);
    void startWithTarget(CCNode* target) override;
    // a new grid for the target
    virtual CCGridBase* getGrid() { return nullptr; }
    const CCSize& getGridSize() { return m_sGridSize; }

protected:
    CCSize m_sGridSize;
};

class CCGrid3DAction : public CCGridAction {
public:
    CCGridBase* getGrid() override;
    // the target's grid's corner at (column, row): now, at the start
    ccVertex3F vertex(const CCPoint& position);
    ccVertex3F originalVertex(const CCPoint& position);
    void setVertex(const CCPoint& position, const ccVertex3F& vertex);
};

// The target wobbles as a liquid: every inner corner sways along x and y,
// `waves` times over the action, by `amplitude` points.
class CCLiquid : public CCGrid3DAction {
public:
    static CCLiquid* create(float duration, const CCSize& gridSize, unsigned int waves, float amplitude);
    bool initWithDuration(float duration, const CCSize& gridSize, unsigned int waves, float amplitude);
    float getAmplitude() { return m_fAmplitude; }
    void setAmplitude(float amplitude) { m_fAmplitude = amplitude; }
    float getAmplitudeRate() { return m_fAmplitudeRate; }
    void setAmplitudeRate(float rate) { m_fAmplitudeRate = rate; }
    CCObject* copyWithZone(void*) override;
    void update(float time) override;

protected:
    unsigned int m_nWaves = 0;
    float m_fAmplitude = 0;
    float m_fAmplitudeRate = 1;
};

// Turns the target's grid off.
class CCStopGrid : public CCActionInstant {
public:
    static CCStopGrid* create();
    void startWithTarget(CCNode* target) override;
    CCObject* copyWithZone(void*) override;
};

}  // namespace cocos2d
