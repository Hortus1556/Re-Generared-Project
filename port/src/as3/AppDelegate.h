// Amateur Surgeon 3 on the PC: the app delegate's start-up
// (AppDelegate::applicationDidFinishLaunching, vf2 at 0xc240) and the
// director's loop on the engine's passes, with the pointer as the touch.
#pragma once

#include "platform/Platform.h"

namespace as3 {

class AppDelegate : public platform::TouchSink {
public:
    AppDelegate();  // applicationDidFinishLaunching
    ~AppDelegate() override;

    // One pass: the director's frame (its display link's in the original).
    void cycle();

    void touchBegan(float x, float y) override;
    void touchMoved(float x, float y, float prevX, float prevY) override;
    void touchEnded(float x, float y, float prevX, float prevY) override;
    // Escape: the keypad's back (Android's back key in the original's other build).
    void escapeKey() override;
    // F1 (debug mode): the debug menu (DebugMenu.h)
    void debugMenuKey() override;
    // the number keys (1..9, 0, the key after it): a surgery's tools
    void numberKey(int n) override;
    // a letter: I, the injection to draw out (SurgeryHud.cpp)
    void charKey(char c) override;
    // typing, while the surgery editor asks for a text (EditorWidgets.h)
    void textInput(const char* utf8) override;
    void textEditKey(platform::EditKey key) override;
};

}  // namespace as3
