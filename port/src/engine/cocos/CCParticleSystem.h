// cocos2d-x 2.1.1's CCParticleSystem (particle_nodes/CCParticleSystem),
// ported to Bleed Engine. MIT: LICENSE-cocos2d-x.txt.
//
// The emitter of a particle system: its particles' life, position, colour,
// size and spin, each a value and a variance (CCRANDOM_MINUS1_1, rand() as
// 2.1.1 had it), moved in gravity mode (A: gravity, speed, radial and
// tangential acceleration) or radius mode (B: a radius shrinking or growing
// while it turns about the source). Emission, life and death are exactly
// 2.1.1's update (a dead particle's slot taken by the last one). A system
// is read from a Particle Designer property list, which the package keeps as
// JSON (extract_ipa.py: the same keys and values): initWithFile /
// initWithDictionary, its texture a file beside it or the list's own
// base64 gzipped picture (textureImageData). Drawing is a subclass's
// (CCParticleSystemQuad).
// Left out of 2.1.1: CCParticleBatchNode (the game has none: the batch node
// pointer stays, always null), CCDictionary (the list is a json::Value),
// the profiler, the asserts. CCTextureProtocol is not a class here: its
// getTexture/setTexture and CCBlendProtocol's blend function are the
// system's own virtuals.
#pragma once

#include <cstdlib>
#include <string>

#include "engine/cocos/CCNode.h"

// ccMacros.h's random numbers, as 2.1.1 defined them: rand() over RAND_MAX
#ifndef CCRANDOM_MINUS1_1
#define CCRANDOM_MINUS1_1() ((2.0f * ((float)std::rand() / RAND_MAX)) - 1.0f)
#endif
#ifndef CCRANDOM_0_1
#define CCRANDOM_0_1() ((float)std::rand() / RAND_MAX)
#endif

namespace json {
struct Value;
}

namespace cocos2d {

class CCTexture2D;
class CCParticleBatchNode;

enum {
    // the emitter lives forever
    kCCParticleDurationInfinity = -1,
    // a particle's starting size is its ending size
    kCCParticleStartSizeEqualToEndSize = -1,
    // a particle's starting radius is its ending radius
    kCCParticleStartRadiusEqualToEndRadius = -1,
    kParticleStartSizeEqualToEndSize = kCCParticleStartSizeEqualToEndSize,
    kParticleDurationInfinity = kCCParticleDurationInfinity,
};

enum {
    kCCParticleModeGravity,  // mode A
    kCCParticleModeRadius,   // mode B
};

typedef enum {
    // living particles stay where they are in the world when the emitter moves
    kCCPositionTypeFree,
    // they stay in the world but follow the emitter's position (an emitter on a sprite)
    kCCPositionTypeRelative,
    // they are the emitter's: moved with it
    kCCPositionTypeGrouped,
} tCCPositionType;

enum {
    kPositionTypeFree = kCCPositionTypeFree,
    kPositionTypeGrouped = kCCPositionTypeGrouped,
};

// One particle (2.1.1's sCCParticle: 0x68 bytes in the original).
typedef struct sCCParticle {
    CCPoint pos;
    CCPoint startPos;
    ccColor4F color;
    ccColor4F deltaColor;
    float size;
    float deltaSize;
    float rotation;
    float deltaRotation;
    float timeToLive;
    unsigned int atlasIndex;
    // Mode A: gravity, direction, radial accel, tangential accel
    struct {
        CCPoint dir;
        float radialAccel;
        float tangentialAccel;
    } modeA;
    // Mode B: radius mode
    struct {
        float angle;
        float degreesPerSecond;
        float radius;
        float deltaRadius;
    } modeB;
    // Not in 2.1.1: the particle's own draw owner (Object::newSerial, set
    // when it is emitted), so the recorded drawing tells it apart from the
    // others from pass to pass as it moves (a dead one's slot taken by the
    // last keeps its id with it).
    unsigned long long drawId;
} tCCParticle;

class CCParticleSystem : public CCNode, public CCBlendProtocol {
public:
    CCParticleSystem();
    ~CCParticleSystem() override;

    // a system read from a property list (its package path, or a bundle name)
    static CCParticleSystem* create(const char* plistFile);
    // a system of a fixed number of particles
    static CCParticleSystem* createWithTotalParticles(unsigned int numberOfParticles);

    bool init() override;
    bool initWithFile(const char* plistFile);
    bool initWithDictionary(const json::Value& dictionary);
    // `dirname`: the folder its texture file is looked for in (the list's own)
    bool initWithDictionary(const json::Value& dictionary, const char* dirname);
    virtual bool initWithTotalParticles(unsigned int numberOfParticles);  // vf175
    // adds a particle to the emitter (false when it is full)
    bool addParticle();         // 0x32b094
    void initParticle(tCCParticle* particle);  // 0x32b0c8
    // stops emitting: the living particles run on until they die
    void stopSystem();          // 0x32b704
    // kills every living particle and starts emitting again
    void resetSystem();         // 0x32b718
    bool isFull();

    virtual void updateQuadWithParticle(tCCParticle* particle, const CCPoint& newPosition);  // vf176
    virtual void postStep();    // vf177
    void update(float dt) override;  // 0x32b754
    virtual void updateWithNoTime();  // vf178

    // batch node (not used: always null)
    virtual CCParticleBatchNode* getBatchNode() { return m_pBatchNode; }
    virtual void setBatchNode(CCParticleBatchNode* batchNode);
    virtual unsigned int getAtlasIndex() { return m_uAtlasIndex; }
    virtual void setAtlasIndex(unsigned int i) { m_uAtlasIndex = i; }

    virtual unsigned int getParticleCount() { return m_uParticleCount; }
    virtual float getDuration() { return m_fDuration; }
    virtual void setDuration(float d) { m_fDuration = d; }
    virtual const CCPoint& getSourcePosition() { return m_tSourcePosition; }
    virtual void setSourcePosition(const CCPoint& p) { m_tSourcePosition = p; }
    virtual const CCPoint& getPosVar() { return m_tPosVar; }
    virtual void setPosVar(const CCPoint& p) { m_tPosVar = p; }
    virtual float getLife() { return m_fLife; }
    virtual void setLife(float v) { m_fLife = v; }
    virtual float getLifeVar() { return m_fLifeVar; }
    virtual void setLifeVar(float v) { m_fLifeVar = v; }
    virtual float getAngle() { return m_fAngle; }
    virtual void setAngle(float v) { m_fAngle = v; }
    virtual float getAngleVar() { return m_fAngleVar; }
    virtual void setAngleVar(float v) { m_fAngleVar = v; }

    // mode A (2.1.1 asserts the mode is gravity)
    virtual const CCPoint& getGravity() { return modeA.gravity; }
    virtual void setGravity(const CCPoint& g) { modeA.gravity = g; }
    virtual float getSpeed() { return modeA.speed; }
    virtual void setSpeed(float speed) { modeA.speed = speed; }
    virtual float getSpeedVar() { return modeA.speedVar; }
    virtual void setSpeedVar(float speed) { modeA.speedVar = speed; }
    virtual float getTangentialAccel() { return modeA.tangentialAccel; }
    virtual void setTangentialAccel(float t) { modeA.tangentialAccel = t; }
    virtual float getTangentialAccelVar() { return modeA.tangentialAccelVar; }
    virtual void setTangentialAccelVar(float t) { modeA.tangentialAccelVar = t; }
    virtual float getRadialAccel() { return modeA.radialAccel; }
    virtual void setRadialAccel(float t) { modeA.radialAccel = t; }
    virtual float getRadialAccelVar() { return modeA.radialAccelVar; }
    virtual void setRadialAccelVar(float t) { modeA.radialAccelVar = t; }
    // mode B (2.1.1 asserts the mode is radius)
    virtual float getStartRadius() { return modeB.startRadius; }
    virtual void setStartRadius(float r) { modeB.startRadius = r; }
    virtual float getStartRadiusVar() { return modeB.startRadiusVar; }
    virtual void setStartRadiusVar(float r) { modeB.startRadiusVar = r; }
    virtual float getEndRadius() { return modeB.endRadius; }
    virtual void setEndRadius(float r) { modeB.endRadius = r; }
    virtual float getEndRadiusVar() { return modeB.endRadiusVar; }
    virtual void setEndRadiusVar(float r) { modeB.endRadiusVar = r; }
    virtual float getRotatePerSecond() { return modeB.rotatePerSecond; }
    virtual void setRotatePerSecond(float degrees) { modeB.rotatePerSecond = degrees; }
    virtual float getRotatePerSecondVar() { return modeB.rotatePerSecondVar; }
    virtual void setRotatePerSecondVar(float degrees) { modeB.rotatePerSecondVar = degrees; }

    // scaled or turned: the system's transform is dirty (for a batch node)
    void setScale(float s) override;
    void setRotation(float newRotation) override;
    void setScaleX(float newScaleX) override;
    void setScaleY(float newScaleY) override;

    virtual bool isActive() { return m_bIsActive; }
    virtual bool isBlendAdditive();
    virtual void setBlendAdditive(bool value);

    virtual float getStartSize() { return m_fStartSize; }
    virtual void setStartSize(float v) { m_fStartSize = v; }
    virtual float getStartSizeVar() { return m_fStartSizeVar; }
    virtual void setStartSizeVar(float v) { m_fStartSizeVar = v; }
    virtual float getEndSize() { return m_fEndSize; }
    virtual void setEndSize(float v) { m_fEndSize = v; }
    virtual float getEndSizeVar() { return m_fEndSizeVar; }
    virtual void setEndSizeVar(float v) { m_fEndSizeVar = v; }
    virtual const ccColor4F& getStartColor() { return m_tStartColor; }
    virtual void setStartColor(const ccColor4F& c) { m_tStartColor = c; }
    virtual const ccColor4F& getStartColorVar() { return m_tStartColorVar; }
    virtual void setStartColorVar(const ccColor4F& c) { m_tStartColorVar = c; }
    virtual const ccColor4F& getEndColor() { return m_tEndColor; }
    virtual void setEndColor(const ccColor4F& c) { m_tEndColor = c; }
    virtual const ccColor4F& getEndColorVar() { return m_tEndColorVar; }
    virtual void setEndColorVar(const ccColor4F& c) { m_tEndColorVar = c; }
    virtual float getStartSpin() { return m_fStartSpin; }
    virtual void setStartSpin(float v) { m_fStartSpin = v; }
    virtual float getStartSpinVar() { return m_fStartSpinVar; }
    virtual void setStartSpinVar(float v) { m_fStartSpinVar = v; }
    virtual float getEndSpin() { return m_fEndSpin; }
    virtual void setEndSpin(float v) { m_fEndSpin = v; }
    virtual float getEndSpinVar() { return m_fEndSpinVar; }
    virtual void setEndSpinVar(float v) { m_fEndSpinVar = v; }
    virtual float getEmissionRate() { return m_fEmissionRate; }
    virtual void setEmissionRate(float v) { m_fEmissionRate = v; }
    virtual unsigned int getTotalParticles() { return m_uTotalParticles; }
    virtual void setTotalParticles(unsigned int v);
    // CCTextureProtocol
    virtual CCTexture2D* getTexture() { return m_pTexture; }
    virtual void setTexture(CCTexture2D* texture);
    // CCBlendProtocol
    ccBlendFunc getBlendFunc() override { return m_tBlendFunc; }
    void setBlendFunc(ccBlendFunc blendFunc) override;
    virtual bool getOpacityModifyRGB() { return m_bOpacityModifyRGB; }
    virtual void setOpacityModifyRGB(bool v) { m_bOpacityModifyRGB = v; }
    virtual tCCPositionType getPositionType() { return m_ePositionType; }
    virtual void setPositionType(tCCPositionType v) { m_ePositionType = v; }
    // removed from its parent when it has no particles left (default false)
    virtual bool isAutoRemoveOnFinish() { return m_bIsAutoRemoveOnFinish; }
    virtual void setAutoRemoveOnFinish(bool v) { m_bIsAutoRemoveOnFinish = v; }
    virtual int getEmitterMode() { return m_nEmitterMode; }
    virtual void setEmitterMode(int v) { m_nEmitterMode = v; }

    // Not in 2.1.1 (for tests): the particles, the first getParticleCount() alive.
    tCCParticle* getParticles() { return m_pParticles; }

protected:
    virtual void updateBlendFunc();  // vf179

    std::string m_sPlistFile;
    // seconds since the system started
    float m_fElapsed = 0;
    // mode A: gravity, tangential and radial acceleration
    struct {
        CCPoint gravity;
        float speed = 0;
        float speedVar = 0;
        float tangentialAccel = 0;
        float tangentialAccelVar = 0;
        float radialAccel = 0;
        float radialAccelVar = 0;
    } modeA;
    // mode B: circular movement
    struct {
        float startRadius = 0;
        float startRadiusVar = 0;
        float endRadius = 0;
        float endRadiusVar = 0;
        float rotatePerSecond = 0;
        float rotatePerSecondVar = 0;
    } modeB;
    tCCParticle* m_pParticles = nullptr;  // calloc'd, m_uAllocatedParticles long
    // what is left over of the emission's time
    float m_fEmitCounter = 0;
    // the particle being updated (the quads drawn after an update)
    unsigned int m_uParticleIdx = 0;
    // (not in 2.1.1) the emitter's position the quads were last written
    // for: update's currentPosition (world for a free system, the node's
    // own for a relative one). An emitter moved after its update (a game
    // placing it in a timer, which 2.1.1's scheduler runs after the
    // updates) has its quads shifted back by the move when drawn
    // (CCParticleSystemQuad::draw), so its particles stay where they are.
    CCPoint m_tQuadsPosition;
    // the emitter's position now, as update reckons it (currentPosition)
    CCPoint emitterPosition();
    CCParticleBatchNode* m_pBatchNode = nullptr;  // weak
    unsigned int m_uAtlasIndex = 0;
    bool m_bTransformSystemDirty = false;
    unsigned int m_uAllocatedParticles = 0;
    bool m_bIsActive = true;
    unsigned int m_uParticleCount = 0;
    float m_fDuration = 0;
    CCPoint m_tSourcePosition;
    CCPoint m_tPosVar;
    float m_fLife = 0, m_fLifeVar = 0;
    float m_fAngle = 0, m_fAngleVar = 0;
    float m_fStartSize = 0, m_fStartSizeVar = 0;
    float m_fEndSize = 0, m_fEndSizeVar = 0;
    ccColor4F m_tStartColor = {0, 0, 0, 0}, m_tStartColorVar = {0, 0, 0, 0};
    ccColor4F m_tEndColor = {0, 0, 0, 0}, m_tEndColorVar = {0, 0, 0, 0};
    float m_fStartSpin = 0, m_fStartSpinVar = 0;
    float m_fEndSpin = 0, m_fEndSpinVar = 0;
    float m_fEmissionRate = 0;
    unsigned int m_uTotalParticles = 0;
    CCTexture2D* m_pTexture = nullptr;
    ccBlendFunc m_tBlendFunc = {CC_BLEND_SRC, CC_BLEND_DST};
    bool m_bOpacityModifyRGB = false;
    bool m_bIsBlendAdditive = false;
    tCCPositionType m_ePositionType = kCCPositionTypeFree;
    bool m_bIsAutoRemoveOnFinish = false;
    int m_nEmitterMode = kCCParticleModeGravity;
};

}  // namespace cocos2d
