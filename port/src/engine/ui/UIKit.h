// A subset of UIKit and Core Animation, for the iOS games whose screens are
// UIKit views (AS2): views loaded from their nibs (as the JSON
// port/tools/nib2json.py writes, in a package's ui/), image views, buttons,
// text views, the touch responder chain, layer animations (basic, keyframe
// along a path, transitions), and timers.
//
// Views draw through the platform's recorded drawing (platform/DrawList.h)
// in the game screen's coordinates, each view under its own transform, so
// what an animation moves is smoothed between passes like anything else.
// Time is the pass clock (platform::passTime()): animations and timers
// advance exactly one pass per pass.
//
// Names follow UIKit's so the ported code reads against the original:
// center/bounds/transform, addSubview, insertSubview:atIndex:,
// setImage:forState:, addAnimation:forKey:, ...
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/Object.h"
#include "platform/GL.h"

namespace ui {

struct Color {
    float r = 0, g = 0, b = 0, a = 0;
};

// CGAffineTransform: x' = a x + c y + tx, y' = b x + d y + ty.
struct Affine {
    float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
    static Affine rotation(float radians);
    static Affine translation(float x, float y);
    static Affine scale(float sx, float sy);
    // CGAffineTransformTranslate(t, x, y): translate first, then this.
    Affine translated(float x, float y) const;
    Affine rotated(float radians) const;
    // CGAffineTransformConcat(this, o): this, then o.
    Affine concat(const Affine& o) const;
    Point apply(Point p) const;
    Affine inverted() const;
    bool isIdentity() const { return a == 1 && b == 0 && c == 0 && d == 1 && tx == 0 && ty == 0; }
};

// UIImage: a picture from the package's images (straight alpha), or made
// from pixels (UIGraphicsGetImageFromCurrentImageContext).
class Image : public Object {
public:
    // "name.png", "name.jpg" or "name" (PNG); nullptr when missing.
    static Image* named(const std::string& name);
    // Straight-alpha RGBA, top row first.
    static Image* withPixels(int width, int height, std::vector<unsigned char> rgba);
    ~Image() override;

    int width() const { return width_; }
    int height() const { return height_; }
    Size size() const { return {static_cast<float>(width_), static_cast<float>(height_)}; }
    const std::vector<unsigned char>& pixels() const { return pixels_; }
    GLuint texture();

private:
    Image() = default;
    int width_ = 0, height_ = 0;
    std::vector<unsigned char> pixels_;
    GLuint texture_ = 0;
};

class View;
class ViewController;
class Animation;

// UITouch: where it is now and was before, in window coordinates.
struct Touch {
    Point location, previous;
    unsigned long long id = 0;  // stable for one press (UITouch's hash)
    Point locationInView(const View* v) const;
    Point previousLocationInView(const View* v) const;
};
using Touches = std::vector<Touch>;

// UIResponder: touches not handled go to the next responder.
class Responder : public Object {
public:
    virtual void touchesBegan(const Touches& t);
    virtual void touchesMoved(const Touches& t);
    virtual void touchesEnded(const Touches& t);
    virtual Responder* nextResponder() { return nullptr; }
};

enum ContentMode {
    kScaleToFill = 0, kScaleAspectFit = 1, kScaleAspectFill = 2, kRedraw = 3, kCenter = 4, kTop = 5,
    kBottom = 6, kLeft = 7, kRight = 8, kTopLeft = 9, kTopRight = 10, kBottomLeft = 11, kBottomRight = 12,
};

// What a layer animation is a delegate of (CAAnimation's delegate).
class AnimationDelegate {
public:
    virtual ~AnimationDelegate() = default;
    virtual void animationDidStart(Animation*) {}
    virtual void animationDidStop(Animation*, bool /*finished*/) {}
};

// CABasicAnimation, CAKeyframeAnimation (position along a path) and
// CATransition, added to a view's layer.
class Animation : public Object {
public:
    enum Kind { kBasic, kKeyframe, kTransition };
    enum Timing { kLinear, kEaseIn, kEaseOut, kEaseInEaseOut, kDefault };

    static Animation* basic(const std::string& keyPath);     // animationWithKeyPath:
    static Animation* keyframe(const std::string& keyPath);  // CAKeyframeAnimation
    static Animation* transition();                          // CATransition animation

    Kind kind = kBasic;
    std::string keyPath;  // "position", "opacity", "transform.rotation"
    double duration = 0.25;
    bool removedOnCompletion = true;
    Timing timing = kDefault;
    // Basic: from (the current value when unset) to (the model value when unset).
    bool hasFrom = false, hasTo = false;
    float from[2] = {0, 0}, to[2] = {0, 0};
    // Keyframe: the path's points, each segment taking the same time.
    std::vector<Point> path;
    // Transition: type "fade", "moveIn", "push", "reveal"; subtype
    // "fromLeft", "fromRight", "fromTop", "fromBottom".
    std::string type = "fade", subtype;

    // Retained, as CAAnimation retained its delegate.
    void setDelegate(Object* owner, AnimationDelegate* d);
    AnimationDelegate* delegate() const { return delegate_; }
    // setValue:forKey: (the original keeps a view controller on a transition).
    std::map<std::string, Object*> values;  // retained
    void setValue(const std::string& key, Object* v);

    ~Animation() override;

private:
    friend class View;
    friend class Window;
    Animation() = default;
    Object* delegateOwner_ = nullptr;
    AnimationDelegate* delegate_ = nullptr;
    double begin_ = -1;  // when it was added
    bool started_ = false, done_ = false;
    float t(double now) const;  // timed progress 0..1
};

// UIView.
class View : public Responder {
public:
    View();
    ~View() override;

    std::string className;  // the nib's class (custom ones included)
    int tag = 0;
    float alpha = 1;
    bool hidden = false;
    bool userInteractionEnabled = true;
    bool clipsToBounds = false;
    int contentMode = kScaleToFill;
    bool hasBackground = false;
    Color backgroundColor;
    Point center;
    Size bounds;  // bounds.size; the bounds origin is always 0
    Affine transform;

    Rect frame() const;
    void setFrame(const Rect& r);
    void setBackgroundColor(Color c) {
        backgroundColor = c;
        hasBackground = true;
    }

    View* superview() const { return superview_; }
    const Array<View>& subviews() const { return subviews_; }
    void addSubview(View* v);
    void insertSubviewAtIndex(View* v, int index);
    void insertSubviewBelow(View* v, View* below);
    void removeFromSuperview();
    bool isDescendantOfView(const View* v) const;
    View* viewWithTag(int t);
    class Window* window();

    // The view controller this view is the root view of (its next responder).
    ViewController* controller = nullptr;
    Responder* nextResponder() override;

    // The layer.
    void addAnimation(Animation* a, const std::string& key);
    void removeAnimationForKey(const std::string& key);
    void removeAllAnimations();
    Animation* animationForKey(const std::string& key) const;

    // Window coordinates of a point of this view and back.
    Point convertToWindow(Point local) const;
    Point convertFromWindow(Point window) const;
    // The deepest visible, interactive view under the point (own coordinates).
    virtual View* hitTest(Point local);
    bool pointInside(Point local) const;

    // Draws itself and its subviews under `parent` (window to screen).
    void draw(const Affine& parent, float parentAlpha);

    // A view's touch is tracked by it (a button) or goes up the chain.
    virtual bool tracksTouches() const { return false; }
    virtual void trackBegan(const Touch&) {}
    virtual void trackMoved(const Touch&) {}
    virtual void trackEnded(const Touch&) {}

    // Runs animation callbacks due by now and drops finished animations
    // (Window::update for every view).
    void updateAnimations(double now);

protected:
    // The view's own content, in its bounds (0..w, 0..h), under m.
    virtual void drawContent(float /*alpha*/) {}
    // A picture drawn into rect (view coordinates) with the view's alpha,
    // its colours times `shade`.
    void drawImage(Image* img, const Rect& rect, float alpha, const float uv[4] = nullptr, float shade = 1);
    Affine toParent() const;  // what the layer's presentation shows

private:
    friend class Window;
    View* superview_ = nullptr;
    Array<View> subviews_;
    std::vector<std::pair<std::string, Animation*>> animations_;  // retained
    // The layer's presentation values now (animations applied).
    Point presentedCenter(double now) const;
    float presentedAlpha(double now) const;
    float presentedRotation(double now) const;
};

// UIImageView (with animationImages).
class ImageView : public View {
public:
    ~ImageView() override;
    void setImage(Image* img);
    Image* image() const { return image_; }
    // Its size becomes the image's; the frame's top-left stays.
    void sizeToFit();
    // startAnimating: the images in turn over animationDuration, repeatCount
    // times (0 forever).
    void setAnimationImages(std::vector<Image*> images);  // retained
    double animationDuration = 0;
    int animationRepeatCount = 0;
    void startAnimating();
    void stopAnimating();
    bool isAnimating() const { return animStart_ >= 0; }

protected:
    void drawContent(float alpha) override;

private:
    Image* image_ = nullptr;
    std::vector<Image*> frames_;
    double animStart_ = -1;
};

class ActionTarget {
public:
    virtual ~ActionTarget() = default;
    virtual void performAction(const std::string& selector, View* sender) = 0;
};

// UIButton: images, background images and titles per state; sends its
// actions on touch up inside (UIControlEventTouchUpInside, 64).
class Button : public View {
public:
    enum State { kNormal = 0, kHighlighted = 1, kDisabled = 2, kSelected = 4 };
    enum Event { kTouchDown = 1, kTouchUpInside = 64 };
    ~Button() override;

    void setImage(Image* img, int state);            // setImage:forState:
    void setBackgroundImage(Image* img, int state);  // setBackgroundImage:forState:
    void setTitle(const std::string& t, int state);
    Image* imageForState(int state) const;
    Image* backgroundImageForState(int state) const;
    bool enabled = true;
    bool selected = false;
    bool highlighted = false;
    bool adjustsImageWhenHighlighted = true;
    bool adjustsImageWhenDisabled = true;
    std::string fontName = "Helvetica-Bold";
    float fontSize = 15;
    Color titleColor{1, 1, 1, 1};

    // addTarget:action:forControlEvents:
    void addTarget(ActionTarget* target, const std::string& selector, int events);
    void removeTargets() { targets_.clear(); }

    bool tracksTouches() const override { return enabled; }
    void trackBegan(const Touch& t) override;
    void trackMoved(const Touch& t) override;
    void trackEnded(const Touch& t) override;

protected:
    void drawContent(float alpha) override;

private:
    int state() const;
    struct StateContent {
        Image* image = nullptr;
        Image* background = nullptr;
        std::string title;
    };
    std::map<int, StateContent> content_;
    struct Target {
        ActionTarget* target;
        std::string selector;
        int events;
    };
    std::vector<Target> targets_;
    void send(int event);
};

// UITextView / UILabel: text in a font, wrapped to the view's width.
class TextView : public View {
public:
    enum Alignment { kLeft = 0, kCenterAlign = 1, kRightAlign = 2 };
    ~TextView() override;
    std::string text;
    std::string fontName = "Helvetica";
    float fontSize = 17;
    Color textColor{0, 0, 0, 1};
    int textAlignment = kLeft;
    // UITextView's text sits 8 points inside its bounds; a label's does not.
    float inset = 8;

protected:
    void drawContent(float alpha) override;

private:
    struct Rendered;
    Rendered* rendered_ = nullptr;
};

// UIAlertView: a modal box over everything with a title, a message and its
// buttons (cancel first); `dismissed` gets the index of the one pressed.
class AlertView : public View {
public:
    static void show(const std::string& title, const std::string& message, std::vector<std::string> buttons,
                     std::function<void(int index)> dismissed);
    void touchesBegan(const Touches& t) override;
    void touchesEnded(const Touches& t) override;

protected:
    void drawContent(float alpha) override;

private:
    AlertView() = default;
    int buttonAt(Point p) const;
    Rect box, buttonRects[4];
    std::vector<std::string> buttons_;
    std::function<void(int)> dismissed_;
    int pressed_ = -1;
    TextView* title_ = nullptr;
    TextView* message_ = nullptr;
    TextView* labels_[4] = {};
};

// The EAGLView: what the hook draws (the GL scene) is the view's content.
class GLView : public View {
public:
    std::function<void()> drawHook;

protected:
    void drawContent(float alpha) override;
};

// UIWindow: the root of the views; takes the platform's touches, runs the
// layers' animations and transitions, and draws everything.
class Window : public View {
public:
    static Window& shared();

    void touchBegan(Point p);
    void touchMoved(Point p, Point prev);
    void touchEnded(Point p, Point prev);

    // Timers and animation callbacks due by now; once a pass, before draw.
    void update();
    // Draws the window filling the game screen.
    void drawAll();

private:
    Window();
    View* touchView_ = nullptr;     // retained, while a touch lasts
    bool tracking_ = false;          // touchView_ tracks it itself
    Touch touch_;
    unsigned long long nextTouchId_ = 1;
    std::vector<View*> lastDrawn_;  // retained: what the window showed last pass
    std::vector<View*> transitionFrom_;  // retained, while a transition runs
    Animation* transition_ = nullptr;
    void deliver(int phase);
    friend class View;
};

// NSTimer scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:.
class Timer : public Object {
public:
    static Timer* scheduled(double interval, bool repeats, std::function<void(Timer*)> fire);
    void invalidate();
    bool isValid() const { return valid_; }

private:
    friend class Window;
    double interval_ = 0, next_ = 0;
    bool repeats_ = false, valid_ = true;
    std::function<void(Timer*)> fire_;
};

// The UI clock: the time the current pass was due.
double now();

// A nib's objects (ui/<name>.json): its top-level views, the File's Owner's
// outlets by name, and the actions to wire.
struct Nib {
    std::vector<View*> views;  // retained by the Nib's caller (see ViewController)
    std::map<std::string, View*> outlets;
    struct Action {
        View* source;
        std::string selector;
        int events;
    };
    std::vector<Action> actions;
};
// False if ui/<name>.json is missing or not a nib.
bool loadNib(const std::string& name, Nib& out);
// A game's own view class, made for nib views of that class name (else
// the view is its UIKit class).
void registerViewClass(const std::string& name, std::function<View*()> make);

// UIViewController loaded from a nib.
class ViewController : public Responder, public ActionTarget, public AnimationDelegate {
public:
    explicit ViewController(const std::string& nibName);
    ~ViewController() override;

    // Loads the view on first use, connects outlets and actions, then
    // calls viewDidLoad().
    View* view();
    bool isViewLoaded() const { return view_ != nullptr; }
    Responder* nextResponder() override;

    void performAction(const std::string& selector, View* sender) override { action(selector, sender); }

protected:
    // An outlet the nib connects to the File's Owner (other than `view`).
    virtual void connectOutlet(const std::string& /*name*/, View* /*v*/) {}
    virtual void viewDidLoad() {}
    // An action a nib button sends (SelectButton:, Pause:...).
    virtual void action(const std::string& /*selector*/, View* /*sender*/) {}

private:
    std::string nibName_;
    View* view_ = nullptr;  // retained
    std::vector<View*> topLevel_;  // retained
};

// Text drawing (UIText.cpp): the kind of system font that stands in for an
// iOS one (platform::systemFont's kinds).
std::string fontKind(const std::string& iosName);
// The text's size in points, wrapped to `width` (0: one line).
Size textSize(const std::string& text, const std::string& font, float size, float width);

}  // namespace ui
