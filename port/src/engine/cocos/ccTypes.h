// cocos2d-x 2.1.1's basic types (include/ccTypes.h, the parts in use), ported
// to Bleed Engine. MIT: LICENSE-cocos2d-x.txt.
#pragma once

#include "engine/cocos/CCGeometry.h"
#include "platform/GL.h"

namespace cocos2d {

typedef float ccTime;

struct ccColor3B {
    GLubyte r, g, b;
};
inline ccColor3B ccc3(GLubyte r, GLubyte g, GLubyte b) { return {r, g, b}; }
const ccColor3B ccWHITE = {255, 255, 255};
const ccColor3B ccYELLOW = {255, 255, 0};
const ccColor3B ccBLUE = {0, 0, 255};
const ccColor3B ccGREEN = {0, 255, 0};
const ccColor3B ccRED = {255, 0, 0};
const ccColor3B ccMAGENTA = {255, 0, 255};
const ccColor3B ccBLACK = {0, 0, 0};
const ccColor3B ccORANGE = {255, 127, 0};
const ccColor3B ccGRAY = {166, 166, 166};
inline bool ccc3BEqual(const ccColor3B& a, const ccColor3B& b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

struct ccColor4B {
    GLubyte r, g, b, a;
};
inline ccColor4B ccc4(GLubyte r, GLubyte g, GLubyte b, GLubyte o) { return {r, g, b, o}; }

struct ccColor4F {
    GLfloat r, g, b, a;
};
inline ccColor4F ccc4FFromccc3B(ccColor3B c) { return {c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f}; }
inline ccColor4F ccc4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { return {r, g, b, a}; }
inline ccColor4F ccc4FFromccc4B(ccColor4B c) { return {c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f}; }
inline ccColor4B ccc4BFromccc4F(ccColor4F c) {
    return {(GLubyte)(c.r * 255), (GLubyte)(c.g * 255), (GLubyte)(c.b * 255), (GLubyte)(c.a * 255)};
}
inline bool ccc4FEqual(ccColor4F a, ccColor4F b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

struct ccVertex2F {
    GLfloat x, y;
};
inline ccVertex2F vertex2(float x, float y) { return {x, y}; }
struct ccVertex3F {
    GLfloat x, y, z;
};
inline ccVertex3F vertex3(float x, float y, float z) { return {x, y, z}; }
struct ccTex2F {
    GLfloat u, v;
};
inline ccTex2F tex2(float u, float v) { return {u, v}; }

struct ccV2F_C4B_T2F {
    ccVertex2F vertices;
    ccColor4B colors;
    ccTex2F texCoords;
};
struct ccV2F_C4F_T2F {
    ccVertex2F vertices;
    ccColor4F colors;
    ccTex2F texCoords;
};
struct ccV3F_C4B_T2F {
    ccVertex3F vertices;
    ccColor4B colors;
    ccTex2F texCoords;
};
struct ccV2F_C4B_T2F_Triangle {
    ccV2F_C4B_T2F a, b, c;
};
// a sprite's or particle's quad, in cocos's order
struct ccV3F_C4B_T2F_Quad {
    ccV3F_C4B_T2F tl, bl, tr, br;
};
struct ccV2F_C4F_T2F_Quad {
    ccV2F_C4F_T2F bl, br, tl, tr;
};

struct ccBlendFunc {
    GLenum src;
    GLenum dst;
};
const ccBlendFunc kCCBlendFuncDisable = {GL_ONE, GL_ZERO};
// for premultiplied textures (cocos's default); straight-alpha ones use GL_SRC_ALPHA
#define CC_BLEND_SRC GL_ONE
#define CC_BLEND_DST GL_ONE_MINUS_SRC_ALPHA

enum CCVerticalTextAlignment { kCCVerticalTextAlignmentTop, kCCVerticalTextAlignmentCenter, kCCVerticalTextAlignmentBottom };
enum CCTextAlignment { kCCTextAlignmentLeft, kCCTextAlignmentCenter, kCCTextAlignmentRight };

}  // namespace cocos2d
