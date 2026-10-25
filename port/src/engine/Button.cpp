#include "engine/Button.h"

Button* Button::initWithNameTypeWidthHeightTextDrawer(const std::string& name, int type, int width, int height,
                                                      TextDrawer* textDrawer) {
    Button* b = new Button;
    b->name = name;
    b->buttonType = type;
    b->textDrawer = retainRef(textDrawer);
    b->width = width;
    b->height = height;
    return b;
}

void Button::releaseLook(std::vector<Layer>& look) {
    for (Layer& l : look) {
        releaseRef(l.image);
        releaseRef(l.frame);
        releaseRef(l.textData);
    }
    look.clear();
}

Button::~Button() {
    releaseLook(upLayers);
    releaseLook(rolloverLayers);
    releaseLook(downLayers);
    releaseLook(disableLayers);
    releaseRef(textDrawer);
}

void Button::addImage(std::vector<Layer>& look, AdvancedImage* img, FrameData* data, bool animated) {
    if (!img) return;  // adding nil to an NSMutableArray threw; nil images come from missing files
    Layer l;
    l.image = retainRef(img);
    l.frame = data ? retainRef(data) : new FrameData;
    look.push_back(l);
    if (animated && !animatedObject.containsObject(img)) animatedObject.addObject(img);
}

void Button::addText(std::vector<Layer>& look, const std::string& text, ButtonStringData* data) {
    Layer l;
    l.text = text;
    l.textData = data ? retainRef(data) : new ButtonStringData;
    look.push_back(l);
}

void Button::addUpObjectDataAnimated(AdvancedImage* img, FrameData* data, bool animated) {
    addImage(upLayers, img, data, animated);
}
void Button::addRolloverObjectDataAnimated(AdvancedImage* img, FrameData* data, bool animated) {
    addImage(rolloverLayers, img, data, animated);
}
void Button::addDownObjectDataAnimated(AdvancedImage* img, FrameData* data, bool animated) {
    addImage(downLayers, img, data, animated);
}
void Button::addDisableObjectDataAnimated(AdvancedImage* img, FrameData* data, bool animated) {
    addImage(disableLayers, img, data, animated);
}
void Button::addUpTextData(const std::string& text, ButtonStringData* data) { addText(upLayers, text, data); }
void Button::addRolloverTextData(const std::string& text, ButtonStringData* data) {
    addText(rolloverLayers, text, data);
}
void Button::drawLook(const std::vector<Layer>& look, float ratio) {
    for (const Layer& l : look) {
        if (!l.image) {
            if (textDrawer)
                textDrawer->drawStringWithStringXYRatioOrientationAlpha(l.text, l.textData->x, l.textData->y,
                                                                        l.textData->ratio, l.textData->orientation,
                                                                        l.textData->alpha);
            continue;
        }
        l.image->setPositionWithXY(positionX, positionY);
        l.image->setRatio(ratio);
        if (animatedObject.containsObject(l.image)) {
            l.image->draw();
        } else {
            l.image->setPositionWithXY(positionX + l.frame->offsetX, positionY + l.frame->offsetY);
            l.image->drawWithFrameData(l.frame);
        }
    }
}

void Button::drawWithRatio(float ratio) {
    const std::vector<Layer>* look = buttonState == kUp         ? &upLayers
                                     : buttonState == kRollover ? &rolloverLayers
                                     : buttonState == kDown     ? &downLayers
                                     : buttonState == kDisabled ? &disableLayers
                                                                : nullptr;
    if (!look) return;
    // The original drew the whole look once per layer (an accidental nested
    // loop); kept because overdraw changes how translucent edges look.
    for (size_t i = 0; i < look->size(); ++i) drawLook(*look, ratio);
}

// The hit box spans [p - offset, p + size + 2 * offset] on each axis (the
// original's asymmetric formula).
bool Button::detectAreaWithXY(int x, int y) const {
    float fx = static_cast<float>(x), fy = static_cast<float>(y);
    return fx >= detectPoint.x - detectOffsetX && fx <= detectPoint.x + width + detectOffsetX * 2 &&
           fy >= detectPoint.y - detectOffsetY && fy <= detectPoint.y + height + detectOffsetY * 2;
}

bool Button::dispatchTouchDownWithXY(int x, int y) {
    if (buttonState == kDisabled) return detectAreaWithXY(x, y);
    if (!detectAreaWithXY(x, y)) return false;
    if (buttonState == kUp) {
        active = true;
        buttonState = buttonType == kToggle ? kDown : kRollover;
    }
    return true;
}

bool Button::dispatchTouchMovedWithXY(int x, int y) {
    if (buttonState == kDisabled) return false;
    if (detectAreaWithXY(x, y)) {
        if (buttonState == kUp) {
            active = true;
            if (buttonType == kPush) buttonState = kRollover;
            else if (buttonType == kToggle) buttonState = kDown;
        }
        return true;
    }
    if (buttonType == kPush) buttonState = kUp;
    return false;
}

bool Button::dispatchTouchUpWithXY(int x, int y) {
    if (buttonState == kRollover) {
        bool hit = detectAreaWithXY(x, y);
        if (!hit) {
            if (active) {
                buttonState = kUp;
                active = false;
            }
            return false;
        }
        if (buttonType == kPush) {
            buttonState = kUp;
            return true;
        }
    } else if (buttonState == kDown) {
        if (detectAreaWithXY(x, y)) {
            buttonState = active ? kDown : kUp;
            active = false;
            return true;
        }
        active = false;
    }
    return false;
}

void Button::setPositionWithXY(int x, int y) {
    positionX = x;
    positionY = y;
    detectPoint = {static_cast<float>(x), static_cast<float>(y)};
}

void Button::setAlpha(float a) {
    for (auto* look : {&upLayers, &rolloverLayers, &disableLayers})
        for (Layer& l : *look)
            if (l.image) l.image->setAlpha(a);
}

void Button::nextFrame() {
    for (AdvancedImage* img : animatedObject) img->nextFrame();
}

void Button::setFrame(int f) {
    for (AdvancedImage* img : animatedObject) img->setFrame(f);
}
