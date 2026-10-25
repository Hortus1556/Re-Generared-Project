// Image/text button with up, rollover, down and disabled looks (port of
// A3IButton).
//
// buttonState: 0 up, 1 rollover (finger on a push button), 2 down (toggle
// button pressed), 3 disabled. buttonType 0 is a push button that fires on
// release; type 1 stays down once pressed. Each look is a stack of layers,
// each either an AdvancedImage (placed with a FrameData) or a text string
// (placed with a ButtonStringData).
#pragma once

#include <string>
#include <vector>

#include "core/Object.h"
#include "engine/AdvancedImage.h"
#include "engine/TextDrawer.h"

class ButtonStringData : public Object {
public:
    int x = 0, y = 0;
    float ratio = 1, orientation = 0, alpha = 1;
};

class Button : public Object {
public:
    enum State { kUp = 0, kRollover = 1, kDown = 2, kDisabled = 3 };
    enum Type { kPush = 0, kToggle = 1 };

    static Button* initWithNameTypeWidthHeightTextDrawer(const std::string& name, int type, int width, int height,
                                                         TextDrawer* textDrawer);
    ~Button() override;

    // `data` may be null (a default FrameData is used). Animated images get
    // nextFrame()/setFrame() and draw with their own frames.
    void addUpObjectDataAnimated(AdvancedImage* img, FrameData* data, bool animated);
    void addRolloverObjectDataAnimated(AdvancedImage* img, FrameData* data, bool animated);
    void addDownObjectDataAnimated(AdvancedImage* img, FrameData* data, bool animated);
    void addDisableObjectDataAnimated(AdvancedImage* img, FrameData* data, bool animated);
    void addUpTextData(const std::string& text, ButtonStringData* data);
    void addRolloverTextData(const std::string& text, ButtonStringData* data);
    void drawWithRatio(float ratio);

    // Return true when the touch hits (down/moved) or activates (up) the button.
    bool dispatchTouchDownWithXY(int x, int y);
    bool dispatchTouchMovedWithXY(int x, int y);
    bool dispatchTouchUpWithXY(int x, int y);
    bool detectAreaWithXY(int x, int y) const;

    void setPositionWithXY(int x, int y);
    int getPositionX() const { return positionX; }
    int getPositionY() const { return positionY; }
    // The middle of its touch area.
    Point centre() const { return {detectPoint.x + width / 2.0f, detectPoint.y + height / 2.0f}; }
    void setWidthHeight(int w, int h) {
        width = w;
        height = h;
    }
    void setDetectOffset(float x, float y) {
        detectOffsetX = static_cast<int>(x);
        detectOffsetY = static_cast<int>(y);
    }
    void setDetectPoint(float x, float y) { detectPoint = {x, y}; }
    void setButtonState(int s) {
        buttonState = s;
        if (s == kDown) active = true;
    }
    int getButtonState() const { return buttonState; }
    void setAlpha(float a);
    void nextFrame();
    void setFrame(int f);

    int buttonState = kUp;
    int buttonType = kPush;
    int positionX = 0, positionY = 0;
    int width = 0, height = 0;
    float orientation = 0;
    std::string name;
    bool active = false;
    int detectOffsetX = 0, detectOffsetY = 0;
    Point detectPoint;

private:
    struct Layer {
        AdvancedImage* image = nullptr;  // retained; null for a text layer
        std::string text;
        FrameData* frame = nullptr;           // image placement (retained)
        ButtonStringData* textData = nullptr;  // text placement (retained)
    };
    Button() = default;
    void addImage(std::vector<Layer>& look, AdvancedImage* img, FrameData* data, bool animated);
    void addText(std::vector<Layer>& look, const std::string& text, ButtonStringData* data);
    void drawLook(const std::vector<Layer>& look, float ratio);
    static void releaseLook(std::vector<Layer>& look);

    std::vector<Layer> upLayers, rolloverLayers, downLayers, disableLayers;
    Array<AdvancedImage> animatedObject;
    TextDrawer* textDrawer = nullptr;
};
