#include "engine/cocos/CCActionGrid.h"

#include <cmath>

#include "engine/cocos/CCNode.h"

namespace cocos2d {

// ---- CCGridAction

bool CCGridAction::initWithDuration(float duration, const CCSize& gridSize) {
    if (!CCActionInterval::initWithDuration(duration)) return false;
    m_sGridSize = gridSize;
    return true;
}

void CCGridAction::startWithTarget(CCNode* target) {
    CCActionInterval::startWithTarget(target);
    CCGridBase* newGrid = getGrid();
    CCGridBase* targetGrid = target->getGrid();
    if (targetGrid && targetGrid->getReuseGrid() > 0) {
        // the grid kept as it is (CCReuseGrid), when it is of the same size
        if (targetGrid->isActive() && targetGrid->getGridSize().width == m_sGridSize.width &&
            targetGrid->getGridSize().height == m_sGridSize.height)
            targetGrid->reuse();
    } else {
        if (targetGrid && targetGrid->isActive()) targetGrid->setActive(false);
        target->setGrid(newGrid);
        target->getGrid()->setActive(true);
    }
}

// ---- CCGrid3DAction

CCGridBase* CCGrid3DAction::getGrid() { return CCGrid3D::create(m_sGridSize); }

ccVertex3F CCGrid3DAction::vertex(const CCPoint& position) {
    return static_cast<CCGrid3D*>(m_pTarget->getGrid())->vertex(position);
}

ccVertex3F CCGrid3DAction::originalVertex(const CCPoint& position) {
    return static_cast<CCGrid3D*>(m_pTarget->getGrid())->originalVertex(position);
}

void CCGrid3DAction::setVertex(const CCPoint& position, const ccVertex3F& vertex) {
    static_cast<CCGrid3D*>(m_pTarget->getGrid())->setVertex(position, vertex);
}

// ---- CCLiquid

CCLiquid* CCLiquid::create(float duration, const CCSize& gridSize, unsigned int waves, float amplitude) {
    CCLiquid* a = new CCLiquid;
    if (a->initWithDuration(duration, gridSize, waves, amplitude)) {
        a->autorelease();
        return a;
    }
    delete a;
    return nullptr;
}

bool CCLiquid::initWithDuration(float duration, const CCSize& gridSize, unsigned int waves, float amplitude) {
    if (!CCGrid3DAction::initWithDuration(duration, gridSize)) return false;
    m_nWaves = waves;
    m_fAmplitude = amplitude;
    m_fAmplitudeRate = 1.0f;
    return true;
}

CCObject* CCLiquid::copyWithZone(void*) {
    CCLiquid* c = new CCLiquid;
    c->initWithDuration(m_fDuration, m_sGridSize, m_nWaves, m_fAmplitude);
    return c;
}

void CCLiquid::update(float time) {
    // the inner corners only: the edges stay
    for (int i = 1; i < static_cast<int>(m_sGridSize.width); ++i)
        for (int j = 1; j < static_cast<int>(m_sGridSize.height); ++j) {
            const CCPoint at = ccp(static_cast<float>(i), static_cast<float>(j));
            ccVertex3F v = originalVertex(at);
            v.x = v.x + std::sin(time * 3.14159265f * m_nWaves * 2 + v.x * .01f) * m_fAmplitude * m_fAmplitudeRate;
            v.y = v.y + std::sin(time * 3.14159265f * m_nWaves * 2 + v.y * .01f) * m_fAmplitude * m_fAmplitudeRate;
            setVertex(at, v);
        }
}

// ---- CCStopGrid

CCStopGrid* CCStopGrid::create() {
    CCStopGrid* a = new CCStopGrid;
    a->autorelease();
    return a;
}

void CCStopGrid::startWithTarget(CCNode* target) {
    CCActionInstant::startWithTarget(target);
    if (CCGridBase* grid = m_pTarget->getGrid())
        if (grid->isActive()) grid->setActive(false);
}

CCObject* CCStopGrid::copyWithZone(void*) { return new CCStopGrid; }

}  // namespace cocos2d
