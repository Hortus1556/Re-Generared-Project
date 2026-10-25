// cocos2d-x 2.1.1's CCParticleSystemQuad.cpp, ported. MIT: LICENSE-cocos2d-x.txt.
#include "engine/cocos/CCParticleSystemQuad.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCSpriteFrameCache.h"
#include "engine/cocos/CCTexture2D.h"
#include "platform/DrawList.h"

namespace cocos2d {

// overriding the init method
bool CCParticleSystemQuad::initWithTotalParticles(unsigned int numberOfParticles) {
    // base initialization
    if (!CCParticleSystem::initWithTotalParticles(numberOfParticles)) return false;
    // allocating data space
    if (!allocMemory()) return false;
    setupIndices();
    // (2.1.1 then made its vertex buffers, took the position-texture-colour
    // shader and listened for the app coming back to the foreground to
    // remake them: the engine draws)
    return true;
}

CCParticleSystemQuad* CCParticleSystemQuad::create(const char* plistFile) {
    CCParticleSystemQuad* p = new CCParticleSystemQuad;
    if (p->initWithFile(plistFile)) {
        p->autorelease();
        return p;
    }
    delete p;
    return nullptr;
}

CCParticleSystemQuad* CCParticleSystemQuad::createWithTotalParticles(unsigned int numberOfParticles) {
    CCParticleSystemQuad* p = new CCParticleSystemQuad;
    if (p->initWithTotalParticles(numberOfParticles)) {
        p->autorelease();
        return p;
    }
    delete p;
    return nullptr;
}

CCParticleSystemQuad* CCParticleSystemQuad::create() {
    CCParticleSystemQuad* p = new CCParticleSystemQuad;
    if (p->init()) {
        p->autorelease();
        return p;
    }
    delete p;
    return nullptr;
}

// pointRect is in points, not in pixels
void CCParticleSystemQuad::initTexCoordsWithRect(const CCRect& pointRect) {
    m_texRect = pointRect;
    // in pixels (by the texture's own scale: the port's, CCTexture2D::getContentScale)
    float s = m_pTexture ? m_pTexture->getContentScale() : CCDirector::sharedDirector()->getContentScaleFactor();
    CCRect rect = CCRectMake(pointRect.origin.x * s, pointRect.origin.y * s, pointRect.size.width * s,
                             pointRect.size.height * s);
    GLfloat wide = static_cast<GLfloat>(pointRect.size.width);
    GLfloat high = static_cast<GLfloat>(pointRect.size.height);
    if (m_pTexture) {
        wide = static_cast<GLfloat>(m_pTexture->getPixelsWide());
        high = static_cast<GLfloat>(m_pTexture->getPixelsHigh());
    }
    // (CC_FIX_ARTIFACTS_BY_STRECHING_TEXEL is off in 2.1.1)
    GLfloat left = rect.origin.x / wide;
    GLfloat bottom = rect.origin.y / high;
    GLfloat right = left + rect.size.width / wide;
    GLfloat top = bottom + rect.size.height / high;
    // cocos2d's textures are upside down: the y is turned over
    std::swap(top, bottom);
    for (unsigned int i = 0; i < m_uTotalParticles && i < m_pQuads.size(); i++) {
        m_pQuads[i].bl.texCoords = {left, bottom};
        m_pQuads[i].br.texCoords = {right, bottom};
        m_pQuads[i].tl.texCoords = {left, top};
        m_pQuads[i].tr.texCoords = {right, top};
    }
}

void CCParticleSystemQuad::setTextureWithRect(CCTexture2D* texture, const CCRect& rect) {
    // only a different texture is set
    if (!m_pTexture || texture->getName() != m_pTexture->getName()) CCParticleSystem::setTexture(texture);
    initTexCoordsWithRect(rect);
}

void CCParticleSystemQuad::setTexture(CCTexture2D* texture) {
    // (2.1.1 takes the size of a null texture: a crash)
    if (!texture) return;
    CCSize s = texture->getContentSize();
    setTextureWithRect(texture, CCRectMake(0, 0, s.width, s.height));
}

void CCParticleSystemQuad::setDisplayFrame(CCSpriteFrame* spriteFrame) {
    // (2.1.1 asserts the frame has no offset: quad particles take none)
    // the texture before the texture's rectangle
    if (!m_pTexture || spriteFrame->getTexture()->getName() != m_pTexture->getName())
        setTexture(spriteFrame->getTexture());
}

void CCParticleSystemQuad::updateQuadWithParticle(tCCParticle* particle, const CCPoint& newPosition) {
    // (a batch node's quad would be its atlas's: there is none)
    ccV3F_C4B_T2F_Quad* quad = &m_pQuads[m_uParticleIdx];
    ccColor4B color = m_bOpacityModifyRGB
                          ? ccc4(static_cast<GLubyte>(particle->color.r * particle->color.a * 255),
                                 static_cast<GLubyte>(particle->color.g * particle->color.a * 255),
                                 static_cast<GLubyte>(particle->color.b * particle->color.a * 255),
                                 static_cast<GLubyte>(particle->color.a * 255))
                          : ccc4(static_cast<GLubyte>(particle->color.r * 255),
                                 static_cast<GLubyte>(particle->color.g * 255),
                                 static_cast<GLubyte>(particle->color.b * 255),
                                 static_cast<GLubyte>(particle->color.a * 255));
    quad->bl.colors = color;
    quad->br.colors = color;
    quad->tl.colors = color;
    quad->tr.colors = color;

    // vertices
    GLfloat size_2 = particle->size / 2;
    if (particle->rotation) {
        GLfloat x1 = -size_2;
        GLfloat y1 = -size_2;
        GLfloat x2 = size_2;
        GLfloat y2 = size_2;
        GLfloat x = newPosition.x;
        GLfloat y = newPosition.y;
        GLfloat r = static_cast<GLfloat>(-CC_DEGREES_TO_RADIANS(particle->rotation));
        GLfloat cr = cosf(r);
        GLfloat sr = sinf(r);
        GLfloat ax = x1 * cr - y1 * sr + x;
        GLfloat ay = x1 * sr + y1 * cr + y;
        GLfloat bx = x2 * cr - y1 * sr + x;
        GLfloat by = x2 * sr + y1 * cr + y;
        GLfloat cx = x2 * cr - y2 * sr + x;
        GLfloat cy = x2 * sr + y2 * cr + y;
        GLfloat dx = x1 * cr - y2 * sr + x;
        GLfloat dy = x1 * sr + y2 * cr + y;
        quad->bl.vertices.x = ax;
        quad->bl.vertices.y = ay;
        quad->br.vertices.x = bx;
        quad->br.vertices.y = by;
        quad->tl.vertices.x = dx;
        quad->tl.vertices.y = dy;
        quad->tr.vertices.x = cx;
        quad->tr.vertices.y = cy;
    } else {
        quad->bl.vertices.x = newPosition.x - size_2;
        quad->bl.vertices.y = newPosition.y - size_2;
        quad->br.vertices.x = newPosition.x + size_2;
        quad->br.vertices.y = newPosition.y - size_2;
        quad->tl.vertices.x = newPosition.x - size_2;
        quad->tl.vertices.y = newPosition.y + size_2;
        quad->tr.vertices.x = newPosition.x + size_2;
        quad->tr.vertices.y = newPosition.y + size_2;
    }
}

void CCParticleSystemQuad::postStep() {
    // (2.1.1 copied the quads into its vertex buffer here: they are drawn from memory)
}

// overriding draw
void CCParticleSystemQuad::draw() {
    // (2.1.1 binds the texture's name: a system with no texture crashed; it draws nothing)
    if (!m_pTexture) return;
    // the living particles' quads (m_uParticleIdx of them after an update),
    // each under its particle's own owner, with the system's blend function
    platform::BlendDst dst(m_tBlendFunc.dst);
    GLuint texture = m_pTexture->getName();
    // Bug fixed: the quads were written in the update for the emitter where
    // it was then; one moved since (in a timer: the game's touch handling,
    // run after the updates) drew every particle shifted by that move, so
    // the particles left behind jumped along with each step of whatever
    // moved the emitter (the pointer). They are shifted back by it, as the
    // free and relative particles' place requires.
    CCPoint moved = CCPointZero;
    if (m_ePositionType == kCCPositionTypeFree || m_ePositionType == kCCPositionTypeRelative)
        moved = ccpSub(emitterPosition(), m_tQuadsPosition);
    for (unsigned int i = 0; i < m_uParticleIdx && i < m_pQuads.size(); ++i) {
        const ccV3F_C4B_T2F_Quad& q = m_pQuads[i];
        float xy[8] = {q.bl.vertices.x - moved.x, q.bl.vertices.y - moved.y, q.br.vertices.x - moved.x,
                       q.br.vertices.y - moved.y, q.tl.vertices.x - moved.x, q.tl.vertices.y - moved.y,
                       q.tr.vertices.x - moved.x, q.tr.vertices.y - moved.y};
        float uv[8] = {q.bl.texCoords.u, q.bl.texCoords.v, q.br.texCoords.u, q.br.texCoords.v,
                       q.tl.texCoords.u, q.tl.texCoords.v, q.tr.texCoords.u, q.tr.texCoords.v};
        float rgba[4] = {q.bl.colors.r / 255.0f, q.bl.colors.g / 255.0f, q.bl.colors.b / 255.0f,
                         q.bl.colors.a / 255.0f};
        platform::drawQuad(m_pParticles[i].drawId, texture, xy, uv, rgba, m_tBlendFunc.src);
    }
}

void CCParticleSystemQuad::setTotalParticles(unsigned int tp) {
    // more than are allocated: new arrays
    if (tp > m_uAllocatedParticles) {
        size_t particlesSize = tp * sizeof(tCCParticle);
        tCCParticle* particlesNew = static_cast<tCCParticle*>(std::realloc(m_pParticles, particlesSize));
        if (!particlesNew) return;  // "Particle system: out of memory"
        m_pParticles = particlesNew;
        m_pQuads.resize(tp);
        // cleared
        std::memset(m_pParticles, 0, particlesSize);
        std::fill(m_pQuads.begin(), m_pQuads.end(), ccV3F_C4B_T2F_Quad{});
        m_uAllocatedParticles = tp;
        m_uTotalParticles = tp;
        // init particles
        if (m_pBatchNode)
            for (unsigned int i = 0; i < m_uTotalParticles; i++) m_pParticles[i].atlasIndex = i;
        setupIndices();
        // Fixed (not in 2.1.1, as cocos2d-x's issue 3990 later did): the
        // cleared quads had lost their texture coordinates, so every
        // particle showed the texture's corner texel (transparent: nothing).
        initTexCoordsWithRect(m_texRect);
    } else {
        m_uTotalParticles = tp;
    }
}

bool CCParticleSystemQuad::allocMemory() {
    // (2.1.1 asserts none is allocated yet and there is no batch node)
    m_pQuads.assign(m_uTotalParticles, ccV3F_C4B_T2F_Quad{});
    return true;
}

void CCParticleSystemQuad::setBatchNode(CCParticleBatchNode* batchNode) {
    if (m_pBatchNode != batchNode) {
        CCParticleBatchNode* oldBatch = m_pBatchNode;
        CCParticleSystem::setBatchNode(batchNode);
        // (2.1.1: back from a batch, its quads, indices and texture are
        // made again; into one, its quads are copied to the batch's atlas.
        // There are no batch nodes.)
        if (!batchNode && oldBatch) {
            allocMemory();
            setupIndices();
        }
    }
}

}  // namespace cocos2d
