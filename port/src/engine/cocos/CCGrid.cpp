#include "engine/cocos/CCGrid.h"

#include "engine/cocos/CCDirector.h"
#include "platform/DrawList.h"

namespace cocos2d {

bool CCGridBase::initWithSize(const CCSize& gridSize) {
    // (the original's texture was the window's size; its step, the window in
    // points over the grid)
    m_sGridSize = gridSize;
    m_obWindowSize = CCDirector::sharedDirector()->getWinSize();
    m_obStep = ccp(m_obWindowSize.width / gridSize.width, m_obWindowSize.height / gridSize.height);
    m_bActive = false;
    m_nReuseGrid = 0;
    calculateVertexPoints();
    return true;
}

CCGrid3D* CCGrid3D::create(const CCSize& gridSize) {
    CCGrid3D* g = new CCGrid3D;
    if (g->initWithSize(gridSize)) {
        g->autorelease();
        return g;
    }
    delete g;
    return nullptr;
}

int CCGrid3D::index(const CCPoint& pos) const {
    return static_cast<int>(pos.x) * (static_cast<int>(m_sGridSize.height) + 1) + static_cast<int>(pos.y);
}

ccVertex3F CCGrid3D::vertex(const CCPoint& pos) { return m_vertices[index(pos)]; }

ccVertex3F CCGrid3D::originalVertex(const CCPoint& pos) { return m_originalVertices[index(pos)]; }

void CCGrid3D::setVertex(const CCPoint& pos, const ccVertex3F& vertex) { m_vertices[index(pos)] = vertex; }

void CCGrid3D::calculateVertexPoints() {
    const int cols = static_cast<int>(m_sGridSize.width), rows = static_cast<int>(m_sGridSize.height);
    m_vertices.assign((cols + 1) * (rows + 1), ccVertex3F{0, 0, 0});
    for (int x = 0; x <= cols; ++x)
        for (int y = 0; y <= rows; ++y) m_vertices[x * (rows + 1) + y] = vertex3(x * m_obStep.x, y * m_obStep.y, 0);
    m_originalVertices = m_vertices;
}

void CCGrid3D::reuse() {
    if (m_nReuseGrid > 0) {
        m_originalVertices = m_vertices;
        --m_nReuseGrid;
    }
}

void CCGrid3D::beforeDraw() {
    // (the depth of the corners is not shown: the grid effects in use are flat)
    m_xy.resize(m_vertices.size() * 2);
    for (size_t i = 0; i < m_vertices.size(); ++i) {
        m_xy[i * 2] = m_vertices[i].x;
        m_xy[i * 2 + 1] = m_vertices[i].y;
    }
    platform::beginWarp(static_cast<int>(m_sGridSize.width), static_cast<int>(m_sGridSize.height),
                        m_obWindowSize.width, m_obWindowSize.height, m_xy.data());
}

void CCGrid3D::afterDraw(CCNode*) { platform::endWarp(); }

}  // namespace cocos2d
