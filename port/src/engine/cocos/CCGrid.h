// cocos2d-x 2.1.1's grid effects (effects/CCGrid: CCGridBase, CCGrid3D),
// ported to Bleed Engine. A node with an active grid is drawn warped by the
// grid's vertices: the original drew it into a window-sized texture
// (beforeDraw) and the texture on the grid (afterDraw); the port draws its
// pieces on the grid directly (platform::beginWarp / endWarp), which looks the
// same. MIT: LICENSE-cocos2d-x.txt.
#pragma once

#include <vector>

#include "engine/cocos/CCGeometry.h"
#include "engine/cocos/CCObject.h"
#include "engine/cocos/ccTypes.h"

namespace cocos2d {

class CCNode;

class CCGridBase : public CCObject {
public:
    bool isActive() { return m_bActive; }
    virtual void setActive(bool active) { m_bActive = active; }
    // how many more actions reuse the grid as it is (CCReuseGrid)
    int getReuseGrid() { return m_nReuseGrid; }
    void setReuseGrid(int reuse) { m_nReuseGrid = reuse; }
    const CCSize& getGridSize() { return m_sGridSize; }
    const CCPoint& getStep() { return m_obStep; }
    // the grid over the window (in points): `gridSize` cells
    bool initWithSize(const CCSize& gridSize);
    virtual void beforeDraw() {}
    virtual void afterDraw(CCNode* target) {}
    virtual void blit() {}
    virtual void reuse() {}
    virtual void calculateVertexPoints() {}

protected:
    bool m_bActive = false;
    int m_nReuseGrid = 0;
    CCSize m_sGridSize;
    CCSize m_obWindowSize;
    CCPoint m_obStep;
};

// A grid whose every corner moves (CCLiquid, CCWaves3D...).
class CCGrid3D : public CCGridBase {
public:
    static CCGrid3D* create(const CCSize& gridSize);
    // the corner at (column, row): where it is now, where it started
    ccVertex3F vertex(const CCPoint& pos);
    ccVertex3F originalVertex(const CCPoint& pos);
    void setVertex(const CCPoint& pos, const ccVertex3F& vertex);
    void beforeDraw() override;
    void afterDraw(CCNode* target) override;
    void reuse() override;
    void calculateVertexPoints() override;

private:
    int index(const CCPoint& pos) const;

    std::vector<ccVertex3F> m_vertices, m_originalVertices;  // column by column
    std::vector<float> m_xy;  // the vertices' x, y for the draw
};

}  // namespace cocos2d
