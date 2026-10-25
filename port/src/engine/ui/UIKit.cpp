#include "engine/ui/UIKit.h"

#include <algorithm>
#include <cmath>

#include "platform/Assets.h"
#include "platform/DrawList.h"
#include "platform/Platform.h"
#include "platform/ImageFile.h"

namespace ui {

double now() { return platform::passTime(); }

// ------------------------------------------------------------------ Affine

Affine Affine::rotation(float r) {
    Affine m;
    m.a = std::cos(r);
    m.b = std::sin(r);
    m.c = -m.b;
    m.d = m.a;
    return m;
}

Affine Affine::translation(float x, float y) {
    Affine m;
    m.tx = x;
    m.ty = y;
    return m;
}

Affine Affine::scale(float sx, float sy) {
    Affine m;
    m.a = sx;
    m.d = sy;
    return m;
}

Affine Affine::translated(float x, float y) const { return translation(x, y).concat(*this); }
Affine Affine::rotated(float r) const { return rotation(r).concat(*this); }

Affine Affine::concat(const Affine& o) const {
    Affine m;
    m.a = a * o.a + b * o.c;
    m.b = a * o.b + b * o.d;
    m.c = c * o.a + d * o.c;
    m.d = c * o.b + d * o.d;
    m.tx = tx * o.a + ty * o.c + o.tx;
    m.ty = tx * o.b + ty * o.d + o.ty;
    return m;
}

Point Affine::apply(Point p) const { return {a * p.x + c * p.y + tx, b * p.x + d * p.y + ty}; }

Affine Affine::inverted() const {
    float det = a * d - b * c;
    if (std::fabs(det) < 1e-12f) return *this;
    Affine m;
    m.a = d / det;
    m.b = -b / det;
    m.c = -c / det;
    m.d = a / det;
    m.tx = -(m.a * tx + m.c * ty);
    m.ty = -(m.b * tx + m.d * ty);
    return m;
}

namespace {

void loadMatrix(const Affine& m) {
    float g[16] = {m.a, m.b, 0, 0, m.c, m.d, 0, 0, 0, 0, 1, 0, m.tx, m.ty, 0, 1};
    glMultMatrixf(g);
}

// A cubic Bézier timing function through (0,0), (x1,y1), (x2,y2), (1,1).
float bezier(float x1, float y1, float x2, float y2, float x) {
    float t = x;
    for (int i = 0; i < 8; ++i) {
        float u = 1 - t;
        float bx = 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t - x;
        float dx = 3 * u * u * x1 + 6 * u * t * (x2 - x1) + 3 * t * t * (1 - x2);
        if (std::fabs(dx) < 1e-6f) break;
        t = std::clamp(t - bx / dx, 0.0f, 1.0f);
    }
    float u = 1 - t;
    return 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t;
}

}  // namespace

// ------------------------------------------------------------------- Image

Image* Image::named(const std::string& name) {
    std::string file = name;
    if (file.find('.') == std::string::npos) file += ".png";
    std::vector<unsigned char> bytes;
    if (!assets::read(file, bytes)) {
        // A scale or device modifier ("x@2x.png", "tutePop~ipad.png"):
        // UIKit fell back to the plain name when the bundle had only that.
        size_t dot = file.rfind('.');
        size_t mark = dot == std::string::npos ? std::string::npos : file.find_first_of("@~");
        if (mark == std::string::npos || mark > dot ||
            !assets::read(file.substr(0, mark) + file.substr(dot), bytes)) {
            platform::debugTrace("image not found: %s", file.c_str());
            return nullptr;
        }
    }
    int w = 0, h = 0;
    std::vector<unsigned char> rgba;
    if (!platform::decodeImage(bytes.data(), bytes.size(), w, h, rgba)) return nullptr;
    return withPixels(w, h, std::move(rgba));
}

Image* Image::withPixels(int width, int height, std::vector<unsigned char> rgba) {
    Image* img = new Image;
    img->width_ = width;
    img->height_ = height;
    img->pixels_ = std::move(rgba);
    return img;
}

Image::~Image() {
    if (texture_) platform::releaseTexture(texture_);
}

GLuint Image::texture() {
    if (!texture_ && width_ > 0 && height_ > 0) {
        glGenTextures(1, &texture_);
        glBindTexture(GL_TEXTURE_2D, texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels_.data());
    }
    return texture_;
}

// ------------------------------------------------------------------- Touch

Point Touch::locationInView(const View* v) const { return v ? v->convertFromWindow(location) : location; }
Point Touch::previousLocationInView(const View* v) const { return v ? v->convertFromWindow(previous) : previous; }

void Responder::touchesBegan(const Touches& t) {
    if (Responder* n = nextResponder()) n->touchesBegan(t);
}
void Responder::touchesMoved(const Touches& t) {
    if (Responder* n = nextResponder()) n->touchesMoved(t);
}
void Responder::touchesEnded(const Touches& t) {
    if (Responder* n = nextResponder()) n->touchesEnded(t);
}

// --------------------------------------------------------------- Animation

Animation* Animation::basic(const std::string& keyPath) {
    Animation* a = new Animation;
    a->kind = kBasic;
    a->keyPath = keyPath;
    return a;
}

Animation* Animation::keyframe(const std::string& keyPath) {
    Animation* a = new Animation;
    a->kind = kKeyframe;
    a->keyPath = keyPath;
    return a;
}

Animation* Animation::transition() {
    Animation* a = new Animation;
    a->kind = kTransition;
    return a;
}

Animation::~Animation() {
    releaseRef(delegateOwner_);
    for (auto& kv : values) releaseRef(kv.second);
}

void Animation::setDelegate(Object* owner, AnimationDelegate* d) {
    retainRef(owner);
    releaseRef(delegateOwner_);
    delegateOwner_ = owner;
    delegate_ = d;
}

void Animation::setValue(const std::string& key, Object* v) {
    retainRef(v);
    auto it = values.find(key);
    if (it != values.end()) releaseRef(it->second);
    values[key] = v;
}

float Animation::t(double now) const {
    if (begin_ < 0) return 0;
    float x = duration > 0 ? static_cast<float>((now - begin_) / duration) : 1;
    x = std::clamp(x, 0.0f, 1.0f);
    switch (timing) {
        case kEaseIn: return bezier(0.42f, 0, 1, 1, x);
        case kEaseOut: return bezier(0, 0, 0.58f, 1, x);
        case kEaseInEaseOut: return bezier(0.42f, 0, 0.58f, 1, x);
        default: return x;
    }
}

// -------------------------------------------------------------------- View

View::View() = default;

View::~View() {
    for (View* v : subviews_) v->superview_ = nullptr;
    removeAllAnimations();
}

Rect View::frame() const {
    // With a transform the frame is the bounding box of the turned bounds.
    Affine m = Affine::translation(-bounds.width / 2, -bounds.height / 2).concat(transform).concat(
        Affine::translation(center.x, center.y));
    Point p[4] = {m.apply({0, 0}), m.apply({bounds.width, 0}), m.apply({0, bounds.height}),
                  m.apply({bounds.width, bounds.height})};
    float x0 = p[0].x, x1 = p[0].x, y0 = p[0].y, y1 = p[0].y;
    for (const Point& q : p) {
        x0 = std::min(x0, q.x);
        x1 = std::max(x1, q.x);
        y0 = std::min(y0, q.y);
        y1 = std::max(y1, q.y);
    }
    return {{x0, y0}, {x1 - x0, y1 - y0}};
}

void View::setFrame(const Rect& r) {
    bounds = r.size;
    center = {r.origin.x + r.size.width / 2, r.origin.y + r.size.height / 2};
}

void View::addSubview(View* v) { insertSubviewAtIndex(v, subviews_.count()); }

void View::insertSubviewAtIndex(View* v, int index) {
    if (!v) return;
    retainRef(v);
    v->removeFromSuperview();
    index = std::clamp(index, 0, subviews_.count());
    subviews_.insertObject(v, index);
    v->superview_ = this;
    releaseRef(v);
}

void View::insertSubviewBelow(View* v, View* below) {
    int i = subviews_.indexOfObject(below);
    insertSubviewAtIndex(v, i < 0 ? subviews_.count() : i);
}

void View::removeFromSuperview() {
    if (!superview_) return;
    // A touch on a view leaving the window is cancelled, as UIKit's: its
    // controls' targets (not retained) may be going with it, a view
    // controller killed mid-press (a mission won while a tool button was
    // held), and the touch's end would act on them.
    Window& w = Window::shared();
    if (w.touchView_ && w.touchView_->isDescendantOfView(this) && window() == &w) {
        releaseRef(w.touchView_);
        w.touchView_ = nullptr;
        w.tracking_ = false;
    }
    View* s = superview_;
    superview_ = nullptr;
    s->subviews_.removeObject(this);
}

bool View::isDescendantOfView(const View* v) const {
    for (const View* p = this; p; p = p->superview_)
        if (p == v) return true;
    return false;
}

View* View::viewWithTag(int t) {
    if (tag == t) return this;
    for (View* v : subviews_)
        if (View* f = v->viewWithTag(t)) return f;
    return nullptr;
}

Window* View::window() {
    View* top = this;
    while (top->superview_) top = top->superview_;
    return top == &Window::shared() ? &Window::shared() : nullptr;
}

Responder* View::nextResponder() {
    if (controller) return controller;
    return superview_;
}

void View::addAnimation(Animation* a, const std::string& key) {
    if (!a) return;
    retainRef(a);
    removeAnimationForKey(key);
    a->begin_ = now();
    a->started_ = a->done_ = false;
    animations_.push_back({key, a});
    if (a->kind == Animation::kTransition && this == &Window::shared()) {
        Window& w = Window::shared();
        for (View* v : w.transitionFrom_) releaseRef(v);
        w.transitionFrom_.clear();
        for (View* v : w.lastDrawn_) w.transitionFrom_.push_back(retainRef(v));
        retainRef(a);
        releaseRef(w.transition_);
        w.transition_ = a;
    }
}

void View::removeAnimationForKey(const std::string& key) {
    for (auto it = animations_.begin(); it != animations_.end(); ++it)
        if (it->first == key) {
            Animation* a = it->second;
            animations_.erase(it);
            releaseRef(a);
            return;
        }
}

void View::removeAllAnimations() {
    auto old = std::move(animations_);
    animations_.clear();
    for (auto& kv : old) releaseRef(kv.second);
}

Animation* View::animationForKey(const std::string& key) const {
    for (const auto& kv : animations_)
        if (kv.first == key) return kv.second;
    return nullptr;
}

void View::updateAnimations(double t) {
    // Callbacks may change the views: work on a retained copy.
    std::vector<std::pair<std::string, Animation*>> list = animations_;
    for (auto& kv : list) retainRef(kv.second);
    retainRef(this);
    for (auto& kv : list) {
        Animation* a = kv.second;
        if (!a->started_) {
            a->started_ = true;
            if (a->delegate_) a->delegate_->animationDidStart(a);
        }
        if (!a->done_ && t >= a->begin_ + a->duration) {
            a->done_ = true;
            if (a->removedOnCompletion) {
                auto it = std::find_if(animations_.begin(), animations_.end(),
                                       [a](const auto& e) { return e.second == a; });
                if (it != animations_.end()) {
                    animations_.erase(it);
                    releaseRef(a);
                }
            }
            if (a->delegate_) a->delegate_->animationDidStop(a, true);
        }
    }
    for (auto& kv : list) releaseRef(kv.second);
    std::vector<View*> subs(subviews_.begin(), subviews_.end());
    for (View* v : subs) retainRef(v);
    for (View* v : subs) v->updateAnimations(t);
    for (View* v : subs) releaseRef(v);
    releaseRef(this);
}

// The layer's presentation: an animation running (with a fill mode of
// removed, the default, a finished one shows the model value again).
Point View::presentedCenter(double t) const {
    Point p = center;
    for (const auto& kv : animations_) {
        const Animation* a = kv.second;
        if (a->keyPath != "position" || a->done_ || a->begin_ < 0) continue;
        float k = a->t(t);
        if (a->kind == Animation::kKeyframe && a->path.size() >= 2) {
            float segs = static_cast<float>(a->path.size() - 1);
            float f = k * segs;
            size_t i = std::min(static_cast<size_t>(f), a->path.size() - 2);
            float u = f - i;
            p = {a->path[i].x + (a->path[i + 1].x - a->path[i].x) * u,
                 a->path[i].y + (a->path[i + 1].y - a->path[i].y) * u};
        } else if (a->kind == Animation::kBasic) {
            Point from = a->hasFrom ? Point{a->from[0], a->from[1]} : center;
            Point to = a->hasTo ? Point{a->to[0], a->to[1]} : center;
            p = {from.x + (to.x - from.x) * k, from.y + (to.y - from.y) * k};
        }
    }
    return p;
}

float View::presentedAlpha(double t) const {
    float al = alpha;
    for (const auto& kv : animations_) {
        const Animation* a = kv.second;
        if (a->keyPath != "opacity" || a->kind != Animation::kBasic || a->done_ || a->begin_ < 0) continue;
        float from = a->hasFrom ? a->from[0] : alpha, to = a->hasTo ? a->to[0] : alpha;
        al = from + (to - from) * a->t(t);
    }
    return al;
}

float View::presentedRotation(double t) const {
    float r = 0;
    for (const auto& kv : animations_) {
        const Animation* a = kv.second;
        if (a->keyPath != "transform.rotation" || a->kind != Animation::kBasic || a->done_ || a->begin_ < 0)
            continue;
        float from = a->hasFrom ? a->from[0] : 0, to = a->hasTo ? a->to[0] : 0;
        r = from + (to - from) * a->t(t);
    }
    return r;
}

Affine View::toParent() const {
    double t = now();
    Point c = presentedCenter(t);
    Affine tr = transform;
    float rot = presentedRotation(t);
    if (rot != 0) tr = Affine::rotation(rot).concat(tr);
    return Affine::translation(-bounds.width / 2, -bounds.height / 2).concat(tr).concat(Affine::translation(c.x, c.y));
}

Point View::convertToWindow(Point local) const {
    Point p = local;
    for (const View* v = this; v; v = v->superview_) p = v->toParent().apply(p);
    return p;
}

Point View::convertFromWindow(Point w) const {
    std::vector<const View*> chain;
    for (const View* v = this; v; v = v->superview_) chain.push_back(v);
    Point p = w;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) p = (*it)->toParent().inverted().apply(p);
    return p;
}

bool View::pointInside(Point p) const { return p.x >= 0 && p.y >= 0 && p.x < bounds.width && p.y < bounds.height; }

View* View::hitTest(Point p) {
    if (hidden || !userInteractionEnabled || presentedAlpha(now()) < 0.01f) return nullptr;
    // Subviews may stick out of a view that does not clip (UIKit tests the
    // point against the view first; so does this).
    if (!pointInside(p)) return nullptr;
    for (int i = subviews_.count() - 1; i >= 0; --i) {
        View* v = subviews_[i];
        Point q = v->toParent().inverted().apply(p);
        if (View* hit = v->hitTest(q)) return hit;
    }
    return this;
}

void View::drawImage(Image* img, const Rect& r, float al, const float uvIn[4], float shade) {
    if (!img || al <= 0) return;
    GLuint tex = img->texture();
    if (!tex) return;
    platform::Sprite s;
    float x0 = r.origin.x, y0 = r.origin.y, x1 = x0 + r.size.width, y1 = y0 + r.size.height;
    float xy[8] = {x0, y0, x1, y0, x0, y1, x1, y1};
    std::copy(xy, xy + 8, s.xy);
    float u0 = uvIn ? uvIn[0] : 0, v0 = uvIn ? uvIn[1] : 0, u1 = uvIn ? uvIn[2] : 1, v1 = uvIn ? uvIn[3] : 1;
    float uv[8] = {u0, v0, u1, v0, u0, v1, u1, v1};
    float rgba[4] = {shade, shade, shade, al};
    platform::drawSprite(serial(), tex, s, uv, rgba, GL_SRC_ALPHA);
}

void View::draw(const Affine& parent, float parentAlpha) {
    if (hidden) return;
    float al = parentAlpha * presentedAlpha(now());
    if (al <= 0.001f) return;
    glPushMatrix();
    loadMatrix(toParent());
    platform::DrawOwner owner(serial());
    if (hasBackground && backgroundColor.a > 0) {
        float xy[8] = {0, 0, bounds.width, 0, 0, bounds.height, bounds.width, bounds.height};
        float rgba[4] = {backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a * al};
        platform::drawQuad(serial(), 0, xy, nullptr, rgba, GL_SRC_ALPHA);
    }
    drawContent(al);
    std::vector<View*> subs(subviews_.begin(), subviews_.end());
    for (View* v : subs) v->draw(parent, al);
    glPopMatrix();
}

// --------------------------------------------------------------- ImageView

ImageView::~ImageView() {
    releaseRef(image_);
    for (Image* i : frames_) releaseRef(i);
}

void ImageView::setImage(Image* img) {
    retainRef(img);
    releaseRef(image_);
    image_ = img;
}

void ImageView::setAnimationImages(std::vector<Image*> images) {
    for (Image* i : images) retainRef(i);
    for (Image* i : frames_) releaseRef(i);
    frames_ = std::move(images);
}

void ImageView::startAnimating() {
    if (!frames_.empty()) animStart_ = now();
}
void ImageView::stopAnimating() { animStart_ = -1; }

namespace {

// Where contentMode puts content of `size` in a view of `bounds`.
Rect contentRect(int mode, Size content, Size bounds) {
    float w = content.width, h = content.height, bw = bounds.width, bh = bounds.height;
    switch (mode) {
        case kScaleAspectFit:
        case kScaleAspectFill: {
            float s = mode == kScaleAspectFit ? std::min(bw / w, bh / h) : std::max(bw / w, bh / h);
            return {{(bw - w * s) / 2, (bh - h * s) / 2}, {w * s, h * s}};
        }
        case kCenter: return {{(bw - w) / 2, (bh - h) / 2}, {w, h}};
        case kTop: return {{(bw - w) / 2, 0}, {w, h}};
        case kBottom: return {{(bw - w) / 2, bh - h}, {w, h}};
        case kLeft: return {{0, (bh - h) / 2}, {w, h}};
        case kRight: return {{bw - w, (bh - h) / 2}, {w, h}};
        case kTopLeft: return {{0, 0}, {w, h}};
        case kTopRight: return {{bw - w, 0}, {w, h}};
        case kBottomLeft: return {{0, bh - h}, {w, h}};
        case kBottomRight: return {{bw - w, bh - h}, {w, h}};
        default: return {{0, 0}, bounds};  // scale to fill, redraw
    }
}

}  // namespace

void ImageView::drawContent(float al) {
    Image* img = image_;
    if (animStart_ >= 0 && !frames_.empty()) {
        double per = animationDuration > 0 ? animationDuration : frames_.size() / 30.0;
        double el = now() - animStart_;
        if (animationRepeatCount > 0 && el >= per * animationRepeatCount) {
            animStart_ = -1;  // done: back to the plain image
        } else {
            size_t i = static_cast<size_t>(std::fmod(el, per) / per * frames_.size());
            img = frames_[std::min(i, frames_.size() - 1)];
        }
    }
    if (img) drawImage(img, contentRect(contentMode, img->size(), bounds), al);
}

void ImageView::sizeToFit() {
    if (!image_) return;
    Rect f = frame();
    bounds = image_->size();
    center = {f.origin.x + bounds.width / 2, f.origin.y + bounds.height / 2};
}

// --------------------------------------------------------------- AlertView

void AlertView::show(const std::string& title, const std::string& message, std::vector<std::string> buttons,
                     std::function<void(int)> dismissed) {
    Window& w = Window::shared();
    AlertView* a = new AlertView;
    a->className = "UIAlertView";
    a->setFrame({{0, 0}, w.bounds});
    a->buttons_ = std::move(buttons);
    a->dismissed_ = std::move(dismissed);
    // iOS 4's alert: 284 points wide, the title in bold 18, the message in
    // 16, the buttons side by side, 43 high.
    const float width = 284;
    Size msg = textSize(message, "Helvetica", 16, width - 24);
    float height = 20 + 22 + 6 + msg.height + 16 + 43 + 15;
    a->box = {{(w.bounds.width - width) / 2, (w.bounds.height - height) / 2}, {width, height}};
    auto label = [&](const std::string& text, const std::string& font, float size, Rect r) {
        TextView* t = new TextView;
        t->text = text;
        t->fontName = font;
        t->fontSize = size;
        t->textColor = {1, 1, 1, 1};
        t->textAlignment = TextView::kCenterAlign;
        t->inset = 0;
        t->userInteractionEnabled = false;
        t->setFrame(r);
        a->addSubview(t);
        t->release();
        return t;
    };
    float x = a->box.origin.x, y = a->box.origin.y;
    a->title_ = label(title, "Helvetica-Bold", 18, {{x + 12, y + 18}, {width - 24, 24}});
    a->message_ = label(message, "Helvetica", 16, {{x + 12, y + 48}, {width - 24, msg.height + 4}});
    int n = static_cast<int>(std::min<size_t>(a->buttons_.size(), 4));
    float bw = n > 0 ? (width - 22 - 8 * (n - 1)) / n : 0;
    for (int i = 0; i < n; ++i) {
        a->buttonRects[i] = {{x + 11 + i * (bw + 8), y + height - 15 - 43}, {bw, 43}};
        Rect r = a->buttonRects[i];
        a->labels_[i] = label(a->buttons_[i], "Helvetica-Bold", 18, {{r.origin.x, r.origin.y + 11}, {r.size.width, 24}});
    }
    // (The trace's "alert <title> button <x>,<y>": its first button's middle,
    // for the play-tests.)
    if (n > 0)
        platform::debugTrace("alert %s button %.0f,%.0f", title.c_str(),
                             a->buttonRects[0].origin.x + a->buttonRects[0].size.width / 2,
                             a->buttonRects[0].origin.y + a->buttonRects[0].size.height / 2);
    w.addSubview(a);
    a->release();
}

int AlertView::buttonAt(Point p) const {
    for (int i = 0; i < static_cast<int>(std::min<size_t>(buttons_.size(), 4)); ++i)
        if (buttonRects[i].contains(p)) return i;
    return -1;
}

void AlertView::touchesBegan(const Touches& t) { pressed_ = buttonAt(t[0].locationInView(this)); }

void AlertView::touchesEnded(const Touches& t) {
    int i = buttonAt(t[0].locationInView(this));
    if (i < 0 || i != pressed_) return;
    retainRef(this);
    removeFromSuperview();
    if (dismissed_) dismissed_(i);
    releaseRef(this);
}

void AlertView::drawContent(float al) {
    auto fill = [&](const Rect& r, Color c) {
        float x0 = r.origin.x, y0 = r.origin.y, x1 = x0 + r.size.width, y1 = y0 + r.size.height;
        float xy[8] = {x0, y0, x1, y0, x0, y1, x1, y1};
        float rgba[4] = {c.r, c.g, c.b, c.a * al};
        platform::drawQuad(serial(), 0, xy, nullptr, rgba, GL_SRC_ALPHA);
    };
    fill({{0, 0}, bounds}, {0, 0, 0, 0.4f});
    fill(box, {0.08f, 0.16f, 0.38f, 0.92f});
    for (int i = 0; i < static_cast<int>(std::min<size_t>(buttons_.size(), 4)); ++i)
        fill(buttonRects[i], i == pressed_ ? Color{0.35f, 0.45f, 0.7f, 1} : Color{0.22f, 0.3f, 0.52f, 1});
}

// ------------------------------------------------------------------ Button

Button::~Button() {
    for (auto& kv : content_) {
        releaseRef(kv.second.image);
        releaseRef(kv.second.background);
    }
}

void Button::setImage(Image* img, int st) {
    retainRef(img);
    releaseRef(content_[st].image);
    content_[st].image = img;
}

void Button::setBackgroundImage(Image* img, int st) {
    retainRef(img);
    releaseRef(content_[st].background);
    content_[st].background = img;
}

void Button::setTitle(const std::string& t, int st) { content_[st].title = t; }

Image* Button::imageForState(int st) const {
    auto it = content_.find(st);
    if (it != content_.end() && it->second.image) return it->second.image;
    it = content_.find(kNormal);
    return it != content_.end() ? it->second.image : nullptr;
}

Image* Button::backgroundImageForState(int st) const {
    auto it = content_.find(st);
    if (it != content_.end() && it->second.background) return it->second.background;
    it = content_.find(kNormal);
    return it != content_.end() ? it->second.background : nullptr;
}

int Button::state() const {
    if (!enabled) return kDisabled;
    if (highlighted) return kHighlighted;
    if (selected) return kSelected;
    return kNormal;
}

void Button::drawContent(float al) {
    int st = state();
    if (Image* bg = backgroundImageForState(st)) drawImage(bg, {{0, 0}, bounds}, al);
    if (Image* img = imageForState(st)) {
        // With no picture of its own for the state, the normal one is drawn
        // darker when highlighted (adjustsImageWhenHighlighted) and lighter
        // when disabled (adjustsImageWhenDisabled): half shaded, half faded.
        bool own = content_.count(st) && content_.at(st).image;
        float shade = 1, alpha = al;
        if (!own && st == kHighlighted && adjustsImageWhenHighlighted) shade = 0.5f;
        if (!own && st == kDisabled && adjustsImageWhenDisabled) alpha = al * 0.5f;
        // UIButton centres its image, shrunk (keeping its shape) to fit.
        Size s = img->size();
        if (s.width > bounds.width || s.height > bounds.height) {
            drawImage(img, contentRect(kScaleAspectFit, s, bounds), alpha, nullptr, shade);
        } else {
            drawImage(img, contentRect(kCenter, s, bounds), alpha, nullptr, shade);
        }
    }
}

void Button::addTarget(ActionTarget* target, const std::string& selector, int events) {
    targets_.push_back({target, selector, events});
}

void Button::send(int event) {
    std::vector<Target> list = targets_;
    retainRef(this);
    for (const Target& t : list)
        if (t.events & event) t.target->performAction(t.selector, this);
    releaseRef(this);
}

void Button::trackBegan(const Touch&) {
    highlighted = true;
    send(kTouchDown);
}

void Button::trackMoved(const Touch& t) {
    // UIKit keeps a control highlighted a little way outside it.
    Point p = t.locationInView(this);
    highlighted = p.x >= -70 && p.y >= -70 && p.x < bounds.width + 70 && p.y < bounds.height + 70;
}

void Button::trackEnded(const Touch& t) {
    Point p = t.locationInView(this);
    bool inside = p.x >= -70 && p.y >= -70 && p.x < bounds.width + 70 && p.y < bounds.height + 70;
    highlighted = false;
    if (inside && enabled) send(kTouchUpInside);
}

// ------------------------------------------------------------------ GLView

void GLView::drawContent(float) {
    if (drawHook) drawHook();
}

// ------------------------------------------------------------------ Window

Window& Window::shared() {
    static Window* w = new Window;
    return *w;
}

Window::Window() {
    className = "UIWindow";
    bounds = {static_cast<float>(platform::screenWidth()), static_cast<float>(platform::screenHeight())};
    center = {bounds.width / 2, bounds.height / 2};
}

void Window::deliver(int phase) {
    if (!touchView_) return;
    Touches ts{touch_};
    if (tracking_) {
        if (phase == 0) touchView_->trackBegan(touch_);
        else if (phase == 1) touchView_->trackMoved(touch_);
        else touchView_->trackEnded(touch_);
    } else {
        if (phase == 0) touchView_->touchesBegan(ts);
        else if (phase == 1) touchView_->touchesMoved(ts);
        else touchView_->touchesEnded(ts);
    }
}

void Window::touchBegan(Point p) {
    if (touchView_) touchEnded(touch_.location, touch_.location);
    // While a transition runs the window takes no touches (the original
    // turned user interaction off for its menus' transitions anyway).
    View* hit = userInteractionEnabled ? hitTest(p) : nullptr;
    if (!hit) return;
    touch_ = {p, p, nextTouchId_++};
    touchView_ = retainRef(hit);
    tracking_ = hit->tracksTouches();
    deliver(0);
}

void Window::touchMoved(Point p, Point prev) {
    if (!touchView_) return;
    touch_.previous = touch_.location;
    touch_.location = p;
    (void)prev;
    deliver(1);
}

void Window::touchEnded(Point p, Point prev) {
    if (!touchView_) return;
    touch_.previous = touch_.location;
    touch_.location = p;
    (void)prev;
    deliver(2);
    releaseRef(touchView_);
    touchView_ = nullptr;
}

namespace {
std::vector<Timer*>& timers() {
    static std::vector<Timer*> t;  // retained
    return t;
}
}  // namespace

Timer* Timer::scheduled(double interval, bool repeats, std::function<void(Timer*)> fire) {
    Timer* t = new Timer;
    t->interval_ = interval;
    t->next_ = now() + interval;
    t->repeats_ = repeats;
    t->fire_ = std::move(fire);
    timers().push_back(retainRef(t));
    t->release();  // the run loop's reference is what keeps it (as NSTimer)
    return t;
}

void Timer::invalidate() { valid_ = false; }

void Window::update() {
    double t = now();
    std::vector<Timer*> due;
    for (Timer* tm : timers())
        if (tm->valid_ && t >= tm->next_ - 1e-9) due.push_back(retainRef(tm));
    for (Timer* tm : due) {
        if (!tm->valid_) continue;
        if (tm->repeats_) {
            tm->next_ += tm->interval_;
            if (tm->next_ < t) tm->next_ = t + tm->interval_;
        } else {
            tm->valid_ = false;
        }
        tm->fire_(tm);
    }
    for (Timer* tm : due) releaseRef(tm);
    auto& all = timers();
    for (auto it = all.begin(); it != all.end();) {
        if (!(*it)->valid_) {
            releaseRef(*it);
            it = all.erase(it);
        } else {
            ++it;
        }
    }
    updateAnimations(t);
    if (transition_ && transition_->done_) {
        releaseRef(transition_);
        transition_ = nullptr;
        for (View* v : transitionFrom_) releaseRef(v);
        transitionFrom_.clear();
    }
}

void Window::drawAll() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, platform::screenWidth(), platform::screenHeight(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Affine id;
    // Retained while drawn: drawing the GL view runs the game, which may
    // swap screens (removing and freeing a view of this list).
    std::vector<View*> now_(subviews().begin(), subviews().end());
    for (View* v : now_) retainRef(v);
    if (!transition_ || hidden) {
        for (View* v : now_) v->draw(id, alpha);
    } else {
        // CATransition: the window as it was (transitionFrom_) gives way to
        // the window as it is; views in both stay as they are.
        float p = transition_->t(now());
        auto in = [](const std::vector<View*>& list, View* v) {
            return std::find(list.begin(), list.end(), v) != list.end();
        };
        const std::string& type = transition_->type;
        const std::string& sub = transition_->subtype;
        float W = bounds.width, H = bounds.height;
        float dx = sub == "fromLeft" ? -W : sub == "fromRight" ? W : 0;
        float dy = sub == "fromTop" ? -H : sub == "fromBottom" ? H : 0;
        if (dx == 0 && dy == 0) dx = W;
        auto drawShifted = [&](View* v, float ox, float oy, float al) {
            glPushMatrix();
            glTranslatef(ox, oy, 0);
            v->draw(id, alpha * al);
            glPopMatrix();
        };
        for (View* v : now_)
            if (in(transitionFrom_, v)) v->draw(id, alpha);
        if (type == "fade") {
            for (View* v : now_)
                if (!in(transitionFrom_, v)) v->draw(id, alpha);
            for (View* v : transitionFrom_)
                if (!in(now_, v)) v->draw(id, alpha * (1 - p));
        } else if (type == "reveal") {
            for (View* v : now_)
                if (!in(transitionFrom_, v)) v->draw(id, alpha);
            for (View* v : transitionFrom_)
                if (!in(now_, v)) drawShifted(v, -dx * p, -dy * p, 1);
        } else {  // moveIn, push
            if (type == "push")
                for (View* v : transitionFrom_)
                    if (!in(now_, v)) drawShifted(v, -dx * p, -dy * p, 1);
            if (type != "push")
                for (View* v : transitionFrom_)
                    if (!in(now_, v)) v->draw(id, alpha);
            for (View* v : now_)
                if (!in(transitionFrom_, v)) drawShifted(v, dx * (1 - p), dy * (1 - p), 1);
        }
    }
    for (View* v : lastDrawn_) releaseRef(v);
    lastDrawn_.clear();
    // (now_'s references pass to lastDrawn_.)
    lastDrawn_ = std::move(now_);
}

// ---------------------------------------------------------- ViewController

ViewController::ViewController(const std::string& nibName) : nibName_(nibName) {}

ViewController::~ViewController() {
    if (view_) view_->controller = nullptr;
    releaseRef(view_);
    for (View* v : topLevel_) releaseRef(v);
}

View* ViewController::view() {
    if (view_) return view_;
    Nib nib;
    if (nibName_.empty() || !loadNib(nibName_, nib)) {
        if (!nibName_.empty()) platform::debugTrace("ui: no nib %s", nibName_.c_str());
        view_ = new View;
        view_->bounds = {static_cast<float>(platform::screenWidth()), static_cast<float>(platform::screenHeight())};
        view_->center = {view_->bounds.width / 2, view_->bounds.height / 2};
    } else {
        topLevel_ = nib.views;
        auto it = nib.outlets.find("view");
        view_ = retainRef(it != nib.outlets.end() ? it->second : (nib.views.empty() ? new View : nib.views[0]));
    }
    view_->controller = this;
    for (auto& kv : nib.outlets)
        if (kv.first != "view") connectOutlet(kv.first, kv.second);
    for (const Nib::Action& a : nib.actions)
        if (Button* b = dynamic_cast<Button*>(a.source)) b->addTarget(this, a.selector, a.events);
    viewDidLoad();
    return view_;
}

Responder* ViewController::nextResponder() { return view_ ? view_->superview() : nullptr; }

}  // namespace ui
