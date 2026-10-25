#include "as2/effects/Popup.h"

namespace as2 {

namespace {

SceneManager& scene() { return SceneManager::sharedSceneManagerInstance(); }

Sprite* comboSprite(const std::string& image, float ox, float x, float y) {
    Sprite* s = Sprite::initWithImageNamed(image, 32, 32);
    s->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* sh = s->currentShape();
    sh->layer = 30;
    sh->offset = {ox, 10};
    sh->pos = {x, y};
    sh->rotation = -30;
    return s;
}

void tint(Sprite* s, float v) {
    if (!s) return;
    float* c = s->currentShape()->color;
    c[0] = c[1] = c[2] = c[3] = v;
}

}  // namespace

Popup* Popup::initWithXY(int x, int y, const std::string& image, bool, int combo) {
    Popup* p = new Popup;
    float fx = static_cast<float>(x), fy = static_cast<float>(y);
    p->mSprite = Sprite::initWithImageNamed(image, 128, 128);
    p->mSprite->calculateTextureCoordsAtOffsetX(0, 0);
    Shape* s = p->mSprite->currentShape();
    s->layer = 30;
    s->offset = {-64, -64};
    s->pos = {fx, fy};
    if (combo > 0) {
        // "x" then the digits, hundreds and tens only when there are some.
        int hundreds = combo / 100, tens = combo % 100 / 10, ones = combo % 10;
        std::vector<int> digits;
        if (hundreds) digits = {hundreds, tens, ones};
        else if (tens) digits = {tens, ones};
        else digits = {ones};
        p->comboB[0] = comboSprite("xb", -6, fx, fy);
        p->comboY[0] = comboSprite("xy", -6, fx, fy);
        for (size_t i = 0; i < digits.size(); ++i) {
            float ox = 10.0f + 16.0f * i;
            p->comboB[i + 1] = comboSprite(std::to_string(digits[i]) + "b", ox, fx, fy);
            p->comboY[i + 1] = comboSprite(std::to_string(digits[i]) + "y", ox, fx, fy);
        }
        // The original's order: the yellow pictures, the outlines, the "x"'s
        // outline (each added under those before it).
        for (Sprite* c : p->comboY)
            if (c) scene().addShape(c->currentShape());
        for (int i = 1; i < 4; ++i)
            if (p->comboB[i]) scene().addShape(p->comboB[i]->currentShape());
        scene().addShape(p->comboB[0]->currentShape());
    }
    scene().addShape(s);
    return p;
}

Popup::~Popup() {
    releaseRef(mSprite);
    for (int i = 0; i < 4; ++i) {
        releaseRef(comboB[i]);
        releaseRef(comboY[i]);
    }
}

bool Popup::update() {
    Shape* s = mSprite->currentShape();
    s->scale = static_cast<float>(s->scale - 0.05);
    for (int i = 0; i < 4; ++i)
        if (comboB[i]) {
            float v = static_cast<float>(comboB[i]->currentShape()->scale - 0.05);
            comboB[i]->currentShape()->scale = v;
            comboY[i]->currentShape()->scale = v;
        }
    if (!(s->scale > 0)) return false;
    tint(mSprite, s->scale);
    for (int i = 0; i < 4; ++i) {
        tint(comboB[i], s->scale);
        tint(comboY[i], s->scale);
    }
    return true;
}

}  // namespace as2
