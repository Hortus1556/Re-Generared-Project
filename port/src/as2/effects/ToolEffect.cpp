#include "as2/effects/ToolEffect.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <map>

#include "platform/Platform.h"

namespace as2 {

ToolEffect::~ToolEffect() { releaseRef(mSprite); }

void ToolEffect::setRed(float r, float g, float b) {
    origRed = r;
    origGreen = g;
    origBlue = b;
    if (!mSprite) return;
    shape()->color[0] = r;
    shape()->color[1] = g;
    shape()->color[2] = b;
}

void ToolEffect::makeSprite(const std::string& image, int w, int h, float x, float y, float offset, int layer,
                            float scale, bool above) {
    mSprite = Sprite::initWithImageNamed(image, w, h);
    mSprite->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* s = shape();
    s->layer = layer;
    s->offset = {offset, offset};
    s->pos = {x, y};
    s->scale = scale;
    // (Effects are made where the finger is, or where something happens:
    // shown as soon as made.)
    s->atFinger = true;
    if (above) SceneManager::sharedSceneManagerInstance().insertShape(s);
    else SceneManager::sharedSceneManagerInstance().addShape(s);
}

void ToolEffect::setTexture(const std::string& name) {
    shape()->tex = SingletonTextureManager::sharedTextureManagerInstance().getTexture2D(name);
}

void ToolEffect::removeShape() { SceneManager::sharedSceneManagerInstance().removeShape(shape()); }

bool ToolEffect::tick() {
    if (update()) return true;
    if (trailLife <= 0) return false;
    float f = 1 - static_cast<float>(++trailAge) / static_cast<float>(trailLife);
    if (f <= 0) {
        removeShape();
        return true;
    }
    shape()->fade = f;
    return false;
}

namespace {

// The flipbooks: the frame at which each picture takes over, the last frame.
struct Flipbook : ToolEffect {
    struct Step {
        int frame;
        const char* image;
    };
    std::vector<Step> steps;
    int last = 25;
    int life() const override { return last; }
    bool update() override {
        if (++frame > last) {
            removeShape();
            return true;
        }
        for (const Step& s : steps)
            if (frame == s.frame) setTexture(s.image);
        return false;
    }
};

// Grows from nothing over 10 frames, holds, fades out over the last 10
// (frames 16..25), tinted by its colour.
struct Splash : Flipbook {
    float fullScale = 1;
    bool update() override {
        if (Flipbook::update()) return true;
        Shape* s = shape();
        s->scale = fullScale;
        if (frame < 10) {
            s->scale = frame / 10.0f * fullScale;
        } else if (frame > 15) {
            float f = (25 - frame) / 10.0f;
            s->color[0] = origRed * f;
            s->color[1] = origGreen * f;
            s->color[2] = origBlue * f;
            s->color[3] = f;
        }
        return false;
    }
};

struct Flame : ToolEffect {
    Flame(float x, float y) { makeSprite("flameTest0001.png", 64, 64, x, y, -20, 29, 1); }
    int life() const override { return 22; }
    bool update() override {
        ++frame;
        if (frame < 23) {
            char name[24];
            std::snprintf(name, sizeof name, "flameTest%04d", frame);
            setTexture(name);
        } else {
            removeShape();
        }
        return frame > 22;
    }
};

struct SmokeEffect : ToolEffect {
    SmokeEffect(float x, float y) {
        makeSprite("smoke", 64, 64, x, y, -32, 29, 0.05f);
        Shape* s = shape();
        s->color[0] = s->color[1] = s->color[2] = 1;
        origRed = 1;
    }
    bool update() override {
        if (++frame > 20) {
            removeShape();
            return true;
        }
        Shape* s = shape();
        s->scale = frame / 20.0f;
        if (frame > 10) {
            // Fades to nothing from the 11th frame, a tenth a frame.
            s->color[0] = static_cast<float>(s->color[0] - 0.1);
            s->color[1] = s->color[2] = s->color[3] = s->color[0];
        }
        return false;
    }
};

struct ExplodeEffect : ToolEffect {
    ExplodeEffect(float x, float y) { makeSprite("explosion1", 256, 256, x, y, -128, 29, 1); }
    bool update() override {
        ++frame;
        if (frame < 20) {
            setTexture("explosion" + std::to_string(frame / 2 + 1));
        } else {
            removeShape();
        }
        return frame > 19;
    }
};

struct InjectionParticle : ToolEffect {
    // Above the tool's layer's other shapes (insertShape), under the HUD.
    InjectionParticle(float x, float y) { makeSprite("fingerTrailParticle", 64, 64, x, y, -32, 5, 0.5f, true); }
    int life() const override { return 32; }
    bool update() override {
        ++frame;
        shape()->scale = frame * 0.03125f + 0.5f;
        bool done = frame > 31;
        if (done) removeShape();
        return done;
    }
};

struct BubbleEffect : ToolEffect {
    float originX = 0, col = 0;
    BubbleEffect(float x, float y) {
        makeSprite("bubble", 32, 32, x, y, -16, 29, 1);
        Shape* s = shape();
        s->color[0] = s->color[1] = s->color[2] = s->color[3] = 0;
        originX = x;
    }
    bool update() override {
        if (++frame > 50) {
            removeShape();
            return true;
        }
        // Fades in over 10 frames, out over the last 10, wobbling as it rises.
        if (frame < 10) col = frame / 10.0f;
        else if (frame < 41) col = 1;
        else col = (50 - frame) / 10.0f;
        Shape* s = shape();
        s->color[0] = s->color[1] = s->color[2] = s->color[3] = col;
        s->pos.x = static_cast<float>(originX + std::sin(frame / 20.0f * 3.141592653589793) * 16.0);
        s->pos.y -= 5;
        return false;
    }
};

ToolEffect* flipbook(const std::string& first, int size, float offset, float scale, std::vector<Flipbook::Step> steps,
                     int last, float x, float y) {
    Flipbook* f = new Flipbook;
    f->makeSprite(first, size, size, x, y, offset, 29, scale);
    f->steps = std::move(steps);
    f->last = last;
    return f;
}

ToolEffect* splash(const std::string& first, int w, int h, float ox, float oy, float r, float g, float b, float x,
                   float y) {
    Splash* s = new Splash;
    s->makeSprite(first, w, h, x, y, ox, 29, 0.1f);
    s->shape()->offset.y = oy;
    s->steps = {{4, "creamSprite0002"}, {7, "creamSprite0003"}, {10, "creamSprite0004"}, {13, "creamSprite0005"}};
    s->ToolEffect::setRed(r, g, b);
    return s;
}

// SplatEffect: a splatter that changes to the bigger one at frame 10, four
// times the size.
struct SplatEffect : Splash {
    SplatEffect(float x, float y) {
        makeSprite("Splatter", 64, 128, x, y, -32, 29, 0.1f);
        shape()->offset.y = -64;
        fullScale = 4;
    }
    bool update() override {
        if (frame + 1 == 10) {
            shape()->offset = {-28, -103};
            setTexture("Splatter5");
        }
        return Splash::update();
    }
};

using Maker = std::function<ToolEffect*(float, float)>;

const std::map<std::string, Maker>& classes() {
    static const std::map<std::string, Maker> c = {
        {"Flame", [](float x, float y) { return new Flame(x, y); }},
        {"SmokeEffect", [](float x, float y) { return new SmokeEffect(x, y); }},
        {"ExplodeEffect", [](float x, float y) { return new ExplodeEffect(x, y); }},
        {"InjectionParticle", [](float x, float y) { return new InjectionParticle(x, y); }},
        {"BubbleEffect", [](float x, float y) { return new BubbleEffect(x, y); }},
        {"SplatEffect", [](float x, float y) { return new SplatEffect(x, y); }},
        {"BloodEffect",
         [](float x, float y) { return splash("creamSprite0001", 32, 32, -16, -16, 0.72f, 0.12f, 0.12f, x, y); }},
        {"CreamEffect",
         [](float x, float y) { return splash("creamSprite0001", 32, 32, -16, -16, 0.744f, 0.96f, 0.552f, x, y); }},
        {"CreamSpray",
         [](float x, float y) {
             ToolEffect* e = flipbook("creamSpray1", 128, -64, 0.5f,
                                      {{5, "creamSpray2"}, {10, "creamSpray3"}, {15, "creamSpray4"}, {20, "creamSpray5"}},
                                      25, x, y);
             e->ToolEffect::setRed(0.744f, 0.96f, 0.552f);
             return e;
         }},
        {"sparkParticle",
         [](float x, float y) {
             return flipbook("sparks1", 256, -128, 1, {{5, "sparks2"}, {10, "sparks3"}, {15, "sparks4"}, {20, "sparks5"}},
                             25, x, y);
         }},
        {"suckEffect",
         [](float x, float y) {
             // Its last change (frame 12) comes after its end (10).
             return flipbook("sparks5", 256, -128, 1, {{3, "sparks4"}, {6, "sparks3"}, {9, "sparks2"}}, 10, x, y);
         }},
        {"boneParticle",
         [](float x, float y) {
             return flipbook("boneParticle1", 128, -64, 1,
                             {{2, "boneParticle2"}, {4, "boneParticle3"}, {6, "boneParticle4"}, {8, "boneParticle5"}},
                             10, x, y);
         }},
    };
    return c;
}

}  // namespace

ToolEffect* ToolEffect::create(const std::string& className, float x, float y) {
    auto it = classes().find(className);
    if (it == classes().end()) {
        platform::debugTrace("effect: class %s is not ported", className.c_str());
        return nullptr;
    }
    return it->second(x, y);
}

}  // namespace as2
