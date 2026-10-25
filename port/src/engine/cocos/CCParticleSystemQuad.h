// cocos2d-x 2.1.1's CCParticleSystemQuad, ported to Bleed Engine.
// MIT: LICENSE-cocos2d-x.txt.
//
// A particle system whose particles are textured quads, turned by their
// spin: each update writes a particle's quad (its corners, colour, the
// texture's rectangle), and draw() draws the living ones. In 2.1.1 the
// quads went into a vertex buffer drawn at once with the system's blend
// function; here each quad is drawn through the engine's recorded drawing
// (platform::drawQuad under BlendDst for the destination factor), its owner
// its particle (tCCParticle::drawId), so each particle moves smoothly
// between passes as the recording interpolates it.
// Left out: the vertex buffers and indices (the engine draws), the shader,
// the batch node and the "come back to foreground" notification.
#pragma once

#include <vector>

#include "engine/cocos/CCParticleSystem.h"

namespace cocos2d {

class CCSpriteFrame;

class CCParticleSystemQuad : public CCParticleSystem {
public:
    CCParticleSystemQuad() = default;
    ~CCParticleSystemQuad() override = default;

    // a system read from a property list
    static CCParticleSystemQuad* create(const char* plistFile);
    static CCParticleSystemQuad* create();
    static CCParticleSystemQuad* createWithTotalParticles(unsigned int numberOfParticles);

    // the particles' texture coordinates (rect in points)
    void initTexCoordsWithRect(const CCRect& rect);
    // the texture: a sprite frame's (it must have no offset)
    void setDisplayFrame(CCSpriteFrame* spriteFrame);
    // a texture and the rectangle of it the particles show (points)
    void setTextureWithRect(CCTexture2D* texture, const CCRect& rect);

    bool initWithTotalParticles(unsigned int numberOfParticles) override;  // 0x32c204
    void setTexture(CCTexture2D* texture) override;                         // 0x32c788
    void updateQuadWithParticle(tCCParticle* particle, const CCPoint& newPosition) override;  // 0x32c7cc
    void postStep() override;                                               // 0x32ca1c
    void draw() override;                                                   // 0x32ca54
    void setBatchNode(CCParticleBatchNode* batchNode) override;            // 0x32cbb8
    void setTotalParticles(unsigned int tp) override;                       // 0x32cac8

private:
    void setupIndices() {}  // (the engine draws each quad itself)
    bool allocMemory();

    std::vector<ccV3F_C4B_T2F_Quad> m_pQuads;  // one per particle slot
    CCRect m_texRect;  // the rectangle last given to initTexCoordsWithRect (points)
};

}  // namespace cocos2d
