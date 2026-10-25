// The matrix stack game code draws under, kept on the CPU (GL.h): GL's
// modelview and projection stacks as the fixed-function pipeline keeps them,
// column-major, each call multiplying on the right as GL's do.
#define BLEED_GL_MATRICES
#include "platform/GL.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace platform::glm {
namespace {

struct M {
    float m[16];
};
M identity() {
    M r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
}
// a * b (GL's order: b applied first)
M times(const M& a, const M& b) {
    M r{};
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

// (per thread: the game's records with them; any other thread's are its own)
struct Stacks {
    std::vector<M> modelview{identity()}, projection{identity()};
    bool projectionMode = false;
    std::vector<M>& current() { return projectionMode ? projection : modelview; }
};
thread_local Stacks gStacks;

void multiply(const M& by) { gStacks.current().back() = times(gStacks.current().back(), by); }

}  // namespace

void matrixMode(GLenum mode) { gStacks.projectionMode = mode == GL_PROJECTION; }

void loadIdentity() { gStacks.current().back() = identity(); }

void ortho(double l, double r, double b, double t, double n, double f) {
    M o = identity();
    o.m[0] = static_cast<float>(2 / (r - l));
    o.m[5] = static_cast<float>(2 / (t - b));
    o.m[10] = static_cast<float>(-2 / (f - n));
    o.m[12] = static_cast<float>(-(r + l) / (r - l));
    o.m[13] = static_cast<float>(-(t + b) / (t - b));
    o.m[14] = static_cast<float>(-(f + n) / (f - n));
    multiply(o);
}

void translate(float x, float y, float z) {
    M t = identity();
    t.m[12] = x;
    t.m[13] = y;
    t.m[14] = z;
    multiply(t);
}

void rotate(float angle, float x, float y, float z) {
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len <= 0) return;
    x /= len;
    y /= len;
    z /= len;
    const float a = angle * 3.14159265358979f / 180, c = std::cos(a), s = std::sin(a), k = 1 - c;
    M r = identity();
    r.m[0] = x * x * k + c;
    r.m[1] = y * x * k + z * s;
    r.m[2] = x * z * k - y * s;
    r.m[4] = x * y * k - z * s;
    r.m[5] = y * y * k + c;
    r.m[6] = y * z * k + x * s;
    r.m[8] = x * z * k + y * s;
    r.m[9] = y * z * k - x * s;
    r.m[10] = z * z * k + c;
    multiply(r);
}

void scale(float x, float y, float z) {
    M s = identity();
    s.m[0] = x;
    s.m[5] = y;
    s.m[10] = z;
    multiply(s);
}

void push() { gStacks.current().push_back(gStacks.current().back()); }

void pop() {
    if (gStacks.current().size() > 1) gStacks.current().pop_back();
}

void mult(const float* m) {
    M b;
    std::memcpy(b.m, m, sizeof b.m);
    multiply(b);
}

void load(const float* m) { std::memcpy(gStacks.current().back().m, m, sizeof(float) * 16); }

void getFloatv(GLenum pname, float* out) {
    if (pname == GL_MODELVIEW_MATRIX) std::memcpy(out, gStacks.modelview.back().m, sizeof(float) * 16);
    else if (pname == GL_PROJECTION_MATRIX) std::memcpy(out, gStacks.projection.back().m, sizeof(float) * 16);
    else ::glGetFloatv(pname, out);
}

}  // namespace platform::glm
