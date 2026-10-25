// OpenGL entry point for game code.
//
// The original targets OpenGL ES 1.1. Desktop OpenGL's compatibility profile
// has the same fixed-function pipeline, so game code calls GL 1.x directly;
// only the GLES-specific names differ (glOrthof -> glOrtho).
//
// On Android the game runs on OpenGL ES 1.1 itself: the desktop names the
// shared code uses are mapped to it here (glOrtho, the sized texture
// formats, which ES 1.1 takes only as the unsized one the data is in).
#pragma once

#if defined(__ANDROID__)
#include <GLES/gl.h>
#include <GLES/glext.h>
#define glOrtho glOrthof
// ES 1.1 wants the internal format the same as the data's (always RGBA
// here: RGB pictures are uploaded as RGBA too).
#undef GL_RGBA8
#define GL_RGBA8 GL_RGBA
#undef GL_RGB8
#define GL_RGB8 GL_RGBA
#ifndef GL_BGRA
#define GL_BGRA GL_BGRA_EXT
#endif
// Framebuffer objects: ES 1.1's extension (OES_framebuffer_object), its
// entry points loaded by name in Platform.cpp.
typedef PFNGLGENFRAMEBUFFERSOESPROC PFNGLGENFRAMEBUFFERSPROC;
typedef PFNGLBINDFRAMEBUFFEROESPROC PFNGLBINDFRAMEBUFFERPROC;
typedef PFNGLFRAMEBUFFERTEXTURE2DOESPROC PFNGLFRAMEBUFFERTEXTURE2DPROC;
typedef PFNGLCHECKFRAMEBUFFERSTATUSOESPROC PFNGLCHECKFRAMEBUFFERSTATUSPROC;
typedef PFNGLDELETEFRAMEBUFFERSOESPROC PFNGLDELETEFRAMEBUFFERSPROC;
typedef PFNGLGENRENDERBUFFERSOESPROC PFNGLGENRENDERBUFFERSPROC;
typedef PFNGLBINDRENDERBUFFEROESPROC PFNGLBINDRENDERBUFFERPROC;
typedef PFNGLRENDERBUFFERSTORAGEOESPROC PFNGLRENDERBUFFERSTORAGEPROC;
typedef PFNGLFRAMEBUFFERRENDERBUFFEROESPROC PFNGLFRAMEBUFFERRENDERBUFFERPROC;
typedef PFNGLDELETERENDERBUFFERSOESPROC PFNGLDELETERENDERBUFFERSPROC;
#define GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define GL_RENDERBUFFER GL_RENDERBUFFER_OES
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_OES
#define GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#else
#include <SDL3/SDL_opengl.h>
#endif

// Game code's matrix calls go to a matrix stack the engine keeps on the CPU
// (GLMatrices.cpp), which recording reads (DrawList): the game's thread
// makes no GL call for them. (With Android's GL every call there waited for
// the display thread's: recording a frame took up to 180 ms.) The replay
// sets GL's own: DrawList.cpp defines BLEED_GL_MATRICES before including.
namespace platform::glm {
void matrixMode(GLenum mode);
void loadIdentity();
void ortho(double left, double right, double bottom, double top, double zNear, double zFar);
void translate(float x, float y, float z);
void rotate(float angle, float x, float y, float z);
void scale(float x, float y, float z);
void push();
void pop();
void mult(const float* m);
void load(const float* m);
// The modelview and projection matrices; anything else asked of GL.
void getFloatv(GLenum pname, float* out);
}  // namespace platform::glm
#if !defined(BLEED_GL_MATRICES)
#undef glOrtho
#define glMatrixMode platform::glm::matrixMode
#define glLoadIdentity platform::glm::loadIdentity
#define glOrtho platform::glm::ortho
#define glOrthof platform::glm::ortho
#define glTranslatef platform::glm::translate
#define glRotatef platform::glm::rotate
#define glScalef platform::glm::scale
#define glPushMatrix platform::glm::push
#define glPopMatrix platform::glm::pop
#define glMultMatrixf platform::glm::mult
#define glLoadMatrixf platform::glm::load
#define glGetFloatv platform::glm::getFloatv
#endif

// Texture combiners (OpenGL 1.3, OpenGL ES 1.1; Windows' headers stop at 1.1).
#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_PRIMARY_COLOR 0x8577
#endif
#ifndef GL_SRC0_RGB
#define GL_SRC0_RGB 0x8580
#define GL_SRC1_RGB 0x8581
#define GL_SRC0_ALPHA 0x8588
#endif
#ifndef GL_OPERAND0_RGB
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND0_ALPHA 0x8598
#endif

namespace platform {
// The format rasterised texels are uploaded in, after toTexelOrder
// (Platform.h): BGRA on the desktop, RGBA on OpenGL ES.
#if defined(__ANDROID__)
constexpr GLenum kTexelFormat = GL_RGBA;
#else
constexpr GLenum kTexelFormat = GL_BGRA;
#endif
}  // namespace platform
