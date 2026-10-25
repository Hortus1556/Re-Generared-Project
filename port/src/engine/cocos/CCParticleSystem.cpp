// cocos2d-x 2.1.1's CCParticleSystem.cpp, ported. MIT: LICENSE-cocos2d-x.txt.
//
// Ideas taken from:
//   . The ocean spray in your face [Jeff Lander]
//   . Building an Advanced Particle System [John van der Burg]
//   . LOVE game engine
// Radius mode support, from 71 squared (Particle Designer). Particle
// Designer's radius mode emits at a fixed 30 Hz; cocos2d emits by its
// emission rate instead, which looks almost the same.
#include "engine/cocos/CCParticleSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/Json.h"
#include "engine/cocos/CCFileUtils.h"
#include "engine/cocos/CCTexture2D.h"
#include "platform/Platform.h"
#include "stb_image.h"

namespace cocos2d {

namespace {

// CCDictionary::valueForKey(key)->floatValue() / intValue() / getCString():
// a missing key is an empty string, so 0 or "". The property list's reals,
// integers and strings are JSON numbers and strings here.
float floatValue(const json::Value& d, const char* key) {
    const json::Value& v = d[key];
    if (v.type == json::Value::kNumber) return static_cast<float>(v.number);
    if (v.type == json::Value::kString) return static_cast<float>(std::atof(v.string.c_str()));
    return 0;
}
int intValue(const json::Value& d, const char* key) {
    const json::Value& v = d[key];
    if (v.type == json::Value::kNumber) return static_cast<int>(v.number);
    if (v.type == json::Value::kString) return std::atoi(v.string.c_str());
    return 0;
}
std::string stringValue(const json::Value& d, const char* key) {
    const json::Value& v = d[key];
    return v.type == json::Value::kString ? v.string : std::string();
}

// support/base64's base64Decode: the bytes (characters outside the
// alphabet skipped, '=' ends it).
std::vector<unsigned char> base64Decode(const std::string& in) {
    static signed char table[256];
    static bool made = false;
    if (!made) {
        std::memset(table, -1, sizeof table);
        const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>(alphabet[i])] = static_cast<signed char>(i);
        made = true;
    }
    std::vector<unsigned char> out;
    unsigned int bits = 0;
    int count = 0;
    for (unsigned char c : in) {
        if (c == '=') break;
        if (table[c] < 0) continue;
        bits = (bits << 6) | static_cast<unsigned int>(table[c]);
        if (++count == 4) {
            out.push_back(static_cast<unsigned char>(bits >> 16));
            out.push_back(static_cast<unsigned char>(bits >> 8));
            out.push_back(static_cast<unsigned char>(bits));
            bits = 0;
            count = 0;
        }
    }
    if (count == 2) {
        out.push_back(static_cast<unsigned char>(bits >> 4));
    } else if (count == 3) {
        out.push_back(static_cast<unsigned char>(bits >> 10));
        out.push_back(static_cast<unsigned char>(bits >> 2));
    }
    return out;
}

// ZipUtils::ccInflateMemory: a gzip or zlib stream inflated (zlib's
// inflateInit2(15 + 32) takes either), here by stb_image's inflater.
bool inflateMemory(const std::vector<unsigned char>& in, std::vector<unsigned char>& out) {
    size_t start = 0;
    int header = 1;  // zlib
    if (in.size() >= 10 && in[0] == 0x1f && in[1] == 0x8b) {
        // gzip: its header, then raw deflate
        unsigned char flags = in[3];
        start = 10;
        if (flags & 4) {  // FEXTRA
            if (start + 2 > in.size()) return false;
            start += 2 + (in[start] | (in[start + 1] << 8));
        }
        if (flags & 8)  // FNAME
            while (start < in.size() && in[start++]) {}
        if (flags & 16)  // FCOMMENT
            while (start < in.size() && in[start++]) {}
        if (flags & 2) start += 2;  // FHCRC
        if (start >= in.size()) return false;
        header = 0;
    }
    int len = 0;
    char* data = stbi_zlib_decode_malloc_guesssize_headerflag(reinterpret_cast<const char*>(in.data() + start),
                                                              static_cast<int>(in.size() - start), 64 * 1024,
                                                              &len, header);
    if (!data) return false;
    out.assign(data, data + len);
    std::free(data);
    return true;
}

}  // namespace

CCParticleSystem::CCParticleSystem() {
    modeA.gravity = CCPointZero;
    m_tSourcePosition = CCPointZero;
    m_tPosVar = CCPointZero;
    m_tBlendFunc.src = CC_BLEND_SRC;
    m_tBlendFunc.dst = CC_BLEND_DST;
}

CCParticleSystem* CCParticleSystem::create(const char* plistFile) {
    CCParticleSystem* p = new CCParticleSystem;
    if (p->initWithFile(plistFile)) {
        p->autorelease();
        return p;
    }
    delete p;
    return nullptr;
}

CCParticleSystem* CCParticleSystem::createWithTotalParticles(unsigned int numberOfParticles) {
    CCParticleSystem* p = new CCParticleSystem;
    if (p->initWithTotalParticles(numberOfParticles)) {
        p->autorelease();
        return p;
    }
    delete p;
    return nullptr;
}

bool CCParticleSystem::init() { return initWithTotalParticles(150); }

bool CCParticleSystem::initWithFile(const char* plistFile) {
    m_sPlistFile = CCFileUtils::sharedFileUtils()->fullPathForFilename(plistFile);
    std::string text;
    json::Value dict;
    if (m_sPlistFile.empty() || !CCFileUtils::sharedFileUtils()->readText(m_sPlistFile, text) ||
        !json::parse(text, dict)) {
        // 2.1.1 asserts ("Particles: file not found") and goes on with no dictionary
        platform::debugTrace("particles not found: %s", plistFile);
        return false;
    }
    // the texture is looked for in the list's own folder
    std::string listFilePath = plistFile;
    if (listFilePath.find('/') != std::string::npos) {
        listFilePath = listFilePath.substr(0, listFilePath.rfind('/') + 1);
        return initWithDictionary(dict, listFilePath.c_str());
    }
    return initWithDictionary(dict, "");
}

bool CCParticleSystem::initWithDictionary(const json::Value& dictionary) { return initWithDictionary(dictionary, ""); }

bool CCParticleSystem::initWithDictionary(const json::Value& dictionary, const char* dirname) {
    int maxParticles = intValue(dictionary, "maxParticles");
    // self, not super
    if (!initWithTotalParticles(maxParticles)) return false;
    // angle
    m_fAngle = floatValue(dictionary, "angle");
    m_fAngleVar = floatValue(dictionary, "angleVariance");
    // duration
    m_fDuration = floatValue(dictionary, "duration");
    // blend function
    m_tBlendFunc.src = intValue(dictionary, "blendFuncSource");
    m_tBlendFunc.dst = intValue(dictionary, "blendFuncDestination");
    // color
    m_tStartColor.r = floatValue(dictionary, "startColorRed");
    m_tStartColor.g = floatValue(dictionary, "startColorGreen");
    m_tStartColor.b = floatValue(dictionary, "startColorBlue");
    m_tStartColor.a = floatValue(dictionary, "startColorAlpha");
    m_tStartColorVar.r = floatValue(dictionary, "startColorVarianceRed");
    m_tStartColorVar.g = floatValue(dictionary, "startColorVarianceGreen");
    m_tStartColorVar.b = floatValue(dictionary, "startColorVarianceBlue");
    m_tStartColorVar.a = floatValue(dictionary, "startColorVarianceAlpha");
    m_tEndColor.r = floatValue(dictionary, "finishColorRed");
    m_tEndColor.g = floatValue(dictionary, "finishColorGreen");
    m_tEndColor.b = floatValue(dictionary, "finishColorBlue");
    m_tEndColor.a = floatValue(dictionary, "finishColorAlpha");
    m_tEndColorVar.r = floatValue(dictionary, "finishColorVarianceRed");
    m_tEndColorVar.g = floatValue(dictionary, "finishColorVarianceGreen");
    m_tEndColorVar.b = floatValue(dictionary, "finishColorVarianceBlue");
    m_tEndColorVar.a = floatValue(dictionary, "finishColorVarianceAlpha");
    // particle size
    m_fStartSize = floatValue(dictionary, "startParticleSize");
    m_fStartSizeVar = floatValue(dictionary, "startParticleSizeVariance");
    m_fEndSize = floatValue(dictionary, "finishParticleSize");
    m_fEndSizeVar = floatValue(dictionary, "finishParticleSizeVariance");
    // position
    float x = floatValue(dictionary, "sourcePositionx");
    float y = floatValue(dictionary, "sourcePositiony");
    setPosition(ccp(x, y));
    m_tPosVar.x = floatValue(dictionary, "sourcePositionVariancex");
    m_tPosVar.y = floatValue(dictionary, "sourcePositionVariancey");
    // spinning
    m_fStartSpin = floatValue(dictionary, "rotationStart");
    m_fStartSpinVar = floatValue(dictionary, "rotationStartVariance");
    m_fEndSpin = floatValue(dictionary, "rotationEnd");
    m_fEndSpinVar = floatValue(dictionary, "rotationEndVariance");

    m_nEmitterMode = intValue(dictionary, "emitterType");
    if (m_nEmitterMode == kCCParticleModeGravity) {
        // mode A: gravity + tangential accel + radial accel
        modeA.gravity.x = floatValue(dictionary, "gravityx");
        modeA.gravity.y = floatValue(dictionary, "gravityy");
        modeA.speed = floatValue(dictionary, "speed");
        modeA.speedVar = floatValue(dictionary, "speedVariance");
        modeA.radialAccel = floatValue(dictionary, "radialAcceleration");
        modeA.radialAccelVar = floatValue(dictionary, "radialAccelVariance");
        modeA.tangentialAccel = floatValue(dictionary, "tangentialAcceleration");
        modeA.tangentialAccelVar = floatValue(dictionary, "tangentialAccelVariance");
    } else if (m_nEmitterMode == kCCParticleModeRadius) {
        // mode B: radius movement
        modeB.startRadius = floatValue(dictionary, "maxRadius");
        modeB.startRadiusVar = floatValue(dictionary, "maxRadiusVariance");
        modeB.endRadius = floatValue(dictionary, "minRadius");
        modeB.endRadiusVar = 0.0f;
        modeB.rotatePerSecond = floatValue(dictionary, "rotatePerSecond");
        modeB.rotatePerSecondVar = floatValue(dictionary, "rotatePerSecondVariance");
    } else {
        // "Invalid emitterType in config file"
        return false;
    }

    // life span
    m_fLife = floatValue(dictionary, "particleLifespan");
    m_fLifeVar = floatValue(dictionary, "particleLifespanVariance");
    // emission rate
    m_fEmissionRate = m_uTotalParticles / m_fLife;

    // a batch node's texture is the batch's
    if (!m_pBatchNode) {
        // a compatible default for the alpha transfer
        m_bOpacityModifyRGB = false;
        // the texture: from the cache, beside the list
        std::string textureName = stringValue(dictionary, "textureFileName");
        size_t rPos = textureName.rfind('/');
        if (rPos != std::string::npos) {
            std::string textureDir = textureName.substr(0, rPos + 1);
            if (dirname && textureDir != dirname) textureName = std::string(dirname) + textureName.substr(rPos + 1);
        } else if (dirname) {
            textureName = std::string(dirname) + textureName;
        }
        CCTexture2D* tex = nullptr;
        // (2.1.1 turns the file utils' missing-file message off for this)
        CCFileUtils* files = CCFileUtils::sharedFileUtils();
        if (!textureName.empty() && files->isFileExist(files->fullPathForFilename(textureName.c_str())))
            tex = CCTextureCache::sharedTextureCache()->addImage(textureName.c_str());
        if (tex) {
            setTexture(tex);
        } else {
            // else the list's own picture: base64, gzipped
            std::string textureData = stringValue(dictionary, "textureImageData");
            if (!textureData.empty()) {
                std::vector<unsigned char> buffer = base64Decode(textureData), deflated;
                if (buffer.empty()) return false;  // "error decoding textureImageData"
                if (!inflateMemory(buffer, deflated)) return false;  // "error ungzipping textureImageData"
                tex = CCTextureCache::sharedTextureCache()->addImageData(deflated.data(), deflated.size(),
                                                                         textureName.c_str());
                if (!tex) return false;  // "error init image with Data"
                setTexture(tex);
            }
        }
        // 2.1.1 asserts it has one ("error loading the texture"); here it draws nothing
        if (!m_pTexture) platform::debugTrace("particles: no texture %s", textureName.c_str());
    }
    return true;
}

bool CCParticleSystem::initWithTotalParticles(unsigned int numberOfParticles) {
    m_uTotalParticles = numberOfParticles;
    std::free(m_pParticles);
    m_pParticles = static_cast<tCCParticle*>(std::calloc(m_uTotalParticles ? m_uTotalParticles : 1, sizeof(tCCParticle)));
    if (!m_pParticles) return false;  // "not enough memory"
    m_uAllocatedParticles = numberOfParticles;
    if (m_pBatchNode)
        for (unsigned int i = 0; i < m_uTotalParticles; i++) m_pParticles[i].atlasIndex = i;
    // default, active
    m_bIsActive = true;
    // default blend function
    m_tBlendFunc.src = CC_BLEND_SRC;
    m_tBlendFunc.dst = CC_BLEND_DST;
    // default movement type
    m_ePositionType = kCCPositionTypeFree;
    // by default in mode A
    m_nEmitterMode = kCCParticleModeGravity;
    m_bIsAutoRemoveOnFinish = false;
    // for a batch node
    m_bTransformSystemDirty = false;
    // updated after the actions have run
    scheduleUpdateWithPriority(1);
    return true;
}

CCParticleSystem::~CCParticleSystem() {
    unscheduleUpdate();
    std::free(m_pParticles);
    CC_SAFE_RELEASE(m_pTexture);
}

bool CCParticleSystem::addParticle() {
    if (isFull()) return false;
    tCCParticle* particle = &m_pParticles[m_uParticleCount];
    initParticle(particle);
    particle->drawId = Object::newSerial();
    ++m_uParticleCount;
    return true;
}

void CCParticleSystem::initParticle(tCCParticle* particle) {
    // timeToLive: no negative life (prevents a division by 0)
    particle->timeToLive = m_fLife + m_fLifeVar * CCRANDOM_MINUS1_1();
    particle->timeToLive = std::max(0.0f, particle->timeToLive);

    // position
    particle->pos.x = m_tSourcePosition.x + m_tPosVar.x * CCRANDOM_MINUS1_1();
    particle->pos.y = m_tSourcePosition.y + m_tPosVar.y * CCRANDOM_MINUS1_1();

    // color
    ccColor4F start;
    start.r = clampf(m_tStartColor.r + m_tStartColorVar.r * CCRANDOM_MINUS1_1(), 0, 1);
    start.g = clampf(m_tStartColor.g + m_tStartColorVar.g * CCRANDOM_MINUS1_1(), 0, 1);
    start.b = clampf(m_tStartColor.b + m_tStartColorVar.b * CCRANDOM_MINUS1_1(), 0, 1);
    start.a = clampf(m_tStartColor.a + m_tStartColorVar.a * CCRANDOM_MINUS1_1(), 0, 1);
    ccColor4F end;
    end.r = clampf(m_tEndColor.r + m_tEndColorVar.r * CCRANDOM_MINUS1_1(), 0, 1);
    end.g = clampf(m_tEndColor.g + m_tEndColorVar.g * CCRANDOM_MINUS1_1(), 0, 1);
    end.b = clampf(m_tEndColor.b + m_tEndColorVar.b * CCRANDOM_MINUS1_1(), 0, 1);
    end.a = clampf(m_tEndColor.a + m_tEndColorVar.a * CCRANDOM_MINUS1_1(), 0, 1);
    particle->color = start;
    particle->deltaColor.r = (end.r - start.r) / particle->timeToLive;
    particle->deltaColor.g = (end.g - start.g) / particle->timeToLive;
    particle->deltaColor.b = (end.b - start.b) / particle->timeToLive;
    particle->deltaColor.a = (end.a - start.a) / particle->timeToLive;

    // size
    float startS = m_fStartSize + m_fStartSizeVar * CCRANDOM_MINUS1_1();
    startS = std::max(0.0f, startS);  // no negative value
    particle->size = startS;
    if (m_fEndSize == kCCParticleStartSizeEqualToEndSize) {
        particle->deltaSize = 0;
    } else {
        float endS = m_fEndSize + m_fEndSizeVar * CCRANDOM_MINUS1_1();
        endS = std::max(0.0f, endS);  // no negative values
        particle->deltaSize = (endS - startS) / particle->timeToLive;
    }

    // rotation
    float startA = m_fStartSpin + m_fStartSpinVar * CCRANDOM_MINUS1_1();
    float endA = m_fEndSpin + m_fEndSpinVar * CCRANDOM_MINUS1_1();
    particle->rotation = startA;
    particle->deltaRotation = (endA - startA) / particle->timeToLive;

    // position
    if (m_ePositionType == kCCPositionTypeFree)
        particle->startPos = convertToWorldSpace(CCPointZero);
    else if (m_ePositionType == kCCPositionTypeRelative)
        particle->startPos = m_obPosition;

    // direction
    float a = CC_DEGREES_TO_RADIANS(m_fAngle + m_fAngleVar * CCRANDOM_MINUS1_1());

    if (m_nEmitterMode == kCCParticleModeGravity) {
        // mode A: gravity
        CCPoint v(cosf(a), sinf(a));
        float s = modeA.speed + modeA.speedVar * CCRANDOM_MINUS1_1();
        // direction
        particle->modeA.dir = ccpMult(v, s);
        // radial accel
        particle->modeA.radialAccel = modeA.radialAccel + modeA.radialAccelVar * CCRANDOM_MINUS1_1();
        // tangential accel
        particle->modeA.tangentialAccel = modeA.tangentialAccel + modeA.tangentialAccelVar * CCRANDOM_MINUS1_1();
    } else {
        // mode B: radius. The particle's diameter about the source position
        float startRadius = modeB.startRadius + modeB.startRadiusVar * CCRANDOM_MINUS1_1();
        float endRadius = modeB.endRadius + modeB.endRadiusVar * CCRANDOM_MINUS1_1();
        particle->modeB.radius = startRadius;
        if (modeB.endRadius == kCCParticleStartRadiusEqualToEndRadius)
            particle->modeB.deltaRadius = 0;
        else
            particle->modeB.deltaRadius = (endRadius - startRadius) / particle->timeToLive;
        particle->modeB.angle = a;
        particle->modeB.degreesPerSecond =
            CC_DEGREES_TO_RADIANS(modeB.rotatePerSecond + modeB.rotatePerSecondVar * CCRANDOM_MINUS1_1());
    }
}

void CCParticleSystem::stopSystem() {
    m_bIsActive = false;
    m_fElapsed = m_fDuration;
    m_fEmitCounter = 0;
}

void CCParticleSystem::resetSystem() {
    m_bIsActive = true;
    m_fElapsed = 0;
    for (m_uParticleIdx = 0; m_uParticleIdx < m_uParticleCount; ++m_uParticleIdx)
        m_pParticles[m_uParticleIdx].timeToLive = 0;
}

bool CCParticleSystem::isFull() { return m_uParticleCount == m_uTotalParticles; }

// the main loop
void CCParticleSystem::update(float dt) {
    if (m_bIsActive && m_fEmissionRate) {
        float rate = 1.0f / m_fEmissionRate;
        // issue #1201: no bursts of particles from too high an emit counter
        if (m_uParticleCount < m_uTotalParticles) m_fEmitCounter += dt;
        while (m_uParticleCount < m_uTotalParticles && m_fEmitCounter > rate) {
            addParticle();
            m_fEmitCounter -= rate;
        }
        m_fElapsed += dt;
        if (m_fDuration != -1 && m_fDuration < m_fElapsed) stopSystem();
    }

    m_uParticleIdx = 0;
    CCPoint currentPosition = emitterPosition();
    m_tQuadsPosition = currentPosition;

    if (m_bVisible) {
        while (m_uParticleIdx < m_uParticleCount) {
            tCCParticle* p = &m_pParticles[m_uParticleIdx];
            // life
            p->timeToLive -= dt;
            if (p->timeToLive > 0) {
                if (m_nEmitterMode == kCCParticleModeGravity) {
                    // mode A: gravity, direction, tangential accel & radial accel
                    CCPoint tmp, radial, tangential;
                    radial = CCPointZero;
                    // radial acceleration
                    if (p->pos.x || p->pos.y) radial = ccpNormalize(p->pos);
                    tangential = radial;
                    radial = ccpMult(radial, p->modeA.radialAccel);
                    // tangential acceleration
                    float newy = tangential.x;
                    tangential.x = -tangential.y;
                    tangential.y = newy;
                    tangential = ccpMult(tangential, p->modeA.tangentialAccel);
                    // (gravity + radial + tangential) * dt
                    tmp = ccpAdd(ccpAdd(radial, tangential), modeA.gravity);
                    tmp = ccpMult(tmp, dt);
                    p->modeA.dir = ccpAdd(p->modeA.dir, tmp);
                    tmp = ccpMult(p->modeA.dir, dt);
                    p->pos = ccpAdd(p->pos, tmp);
                } else {
                    // mode B: the angle and radius of the particle
                    p->modeB.angle += p->modeB.degreesPerSecond * dt;
                    p->modeB.radius += p->modeB.deltaRadius * dt;
                    p->pos.x = -cosf(p->modeB.angle) * p->modeB.radius;
                    p->pos.y = -sinf(p->modeB.angle) * p->modeB.radius;
                }
                // color
                p->color.r += (p->deltaColor.r * dt);
                p->color.g += (p->deltaColor.g * dt);
                p->color.b += (p->deltaColor.b * dt);
                p->color.a += (p->deltaColor.a * dt);
                // size
                p->size += (p->deltaSize * dt);
                p->size = std::max(0.0f, p->size);
                // angle
                p->rotation += (p->deltaRotation * dt);

                // the values in the quad
                CCPoint newPos;
                if (m_ePositionType == kCCPositionTypeFree || m_ePositionType == kCCPositionTypeRelative) {
                    CCPoint diff = ccpSub(currentPosition, p->startPos);
                    newPos = ccpSub(p->pos, diff);
                } else {
                    newPos = p->pos;
                }
                // a batch node has no transform of its own: the position is
                // put in here (not into the particle, where it would upset
                // the radial and tangential accelerations)
                if (m_pBatchNode) {
                    newPos.x += m_obPosition.x;
                    newPos.y += m_obPosition.y;
                }
                updateQuadWithParticle(p, newPos);
                ++m_uParticleIdx;
            } else {
                // life < 0: the last particle takes its place
                if (m_uParticleIdx != m_uParticleCount - 1) m_pParticles[m_uParticleIdx] = m_pParticles[m_uParticleCount - 1];
                --m_uParticleCount;
                if (m_uParticleCount == 0 && m_bIsAutoRemoveOnFinish) {
                    unscheduleUpdate();
                    // may delete it: nothing of it is used after
                    if (m_pParent) m_pParent->removeChild(this, true);
                    return;
                }
            }
        }
        m_bTransformSystemDirty = false;
    }
    if (!m_pBatchNode) postStep();
}

void CCParticleSystem::updateWithNoTime() { update(0.0f); }

CCPoint CCParticleSystem::emitterPosition() {
    if (m_ePositionType == kCCPositionTypeFree) return convertToWorldSpace(CCPointZero);
    if (m_ePositionType == kCCPositionTypeRelative) return m_obPosition;
    return CCPointZero;
}

void CCParticleSystem::updateQuadWithParticle(tCCParticle*, const CCPoint&) {
    // a subclass's
}

void CCParticleSystem::postStep() {
    // a subclass's
}

void CCParticleSystem::setTexture(CCTexture2D* var) {
    if (m_pTexture != var) {
        CC_SAFE_RETAIN(var);
        CC_SAFE_RELEASE(m_pTexture);
        m_pTexture = var;
        updateBlendFunc();
    }
}

void CCParticleSystem::updateBlendFunc() {
    // (2.1.1 asserts there is no batch node)
    if (m_pTexture) {
        bool premultiplied = m_pTexture->hasPremultipliedAlpha();
        m_bOpacityModifyRGB = false;
        if (m_tBlendFunc.src == CC_BLEND_SRC && m_tBlendFunc.dst == CC_BLEND_DST) {
            if (premultiplied) {
                m_bOpacityModifyRGB = true;
            } else {
                m_tBlendFunc.src = GL_SRC_ALPHA;
                m_tBlendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
            }
        }
    }
}

void CCParticleSystem::setBlendAdditive(bool additive) {
    if (additive) {
        m_tBlendFunc.src = GL_SRC_ALPHA;
        m_tBlendFunc.dst = GL_ONE;
    } else if (m_pTexture && !m_pTexture->hasPremultipliedAlpha()) {
        m_tBlendFunc.src = GL_SRC_ALPHA;
        m_tBlendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
    } else {
        m_tBlendFunc.src = CC_BLEND_SRC;
        m_tBlendFunc.dst = CC_BLEND_DST;
    }
}

bool CCParticleSystem::isBlendAdditive() { return m_tBlendFunc.src == GL_SRC_ALPHA && m_tBlendFunc.dst == GL_ONE; }

void CCParticleSystem::setTotalParticles(unsigned int var) {
    // (2.1.1 asserts var <= m_uAllocatedParticles: only quads resize)
    m_uTotalParticles = var;
}

void CCParticleSystem::setBlendFunc(ccBlendFunc blendFunc) {
    if (m_tBlendFunc.src != blendFunc.src || m_tBlendFunc.dst != blendFunc.dst) {
        m_tBlendFunc = blendFunc;
        updateBlendFunc();
    }
}

void CCParticleSystem::setBatchNode(CCParticleBatchNode* batchNode) {
    if (m_pBatchNode != batchNode) {
        m_pBatchNode = batchNode;  // weak
        // each particle needs a unique index
        if (batchNode)
            for (unsigned int i = 0; i < m_uTotalParticles; i++) m_pParticles[i].atlasIndex = i;
    }
}

// no transform matrix of their own: faster
void CCParticleSystem::setScale(float s) {
    m_bTransformSystemDirty = true;
    CCNode::setScale(s);
}

void CCParticleSystem::setRotation(float newRotation) {
    m_bTransformSystemDirty = true;
    CCNode::setRotation(newRotation);
}

void CCParticleSystem::setScaleX(float newScaleX) {
    m_bTransformSystemDirty = true;
    CCNode::setScaleX(newScaleX);
}

void CCParticleSystem::setScaleY(float newScaleY) {
    m_bTransformSystemDirty = true;
    CCNode::setScaleY(newScaleY);
}

}  // namespace cocos2d
