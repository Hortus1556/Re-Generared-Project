// The game's layouts (re/as3/notes/ui.md): every screen is built from a JSON
// layout in common/ (ui_*.json...): images and buttons on the design canvas,
// anchored to the edges the screen shows, and text fields in its fonts.
// Their names are not in the binary; these are the port's.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "engine/cocos/CCLabelBMFont.h"
#include "engine/cocos/CCMenu.h"

namespace mt {

// item types (0x429f40) and anchor modes (0x44f4d0)
enum UIItemType { kUIButton = 0, kUIToggle = 1, kUISubmenu = 2, kUIFront = 3, kUIOther = 4 };
enum UIMode { kUIAbsolute, kUITopLeft, kUITop, kUITopRight, kUIRight, kUIBottomRight, kUIBottom, kUIBottomLeft, kUILeft };

struct UIItemDef {  // 0x18 bytes in the original
    std::string name;
    std::string spritePrefix;
    int type = kUIOther;
    int x = 0, y = 0;
    int mode = kUIAbsolute;
};

struct UITextDef {  // 0x2c bytes
    std::string textId;
    std::string fontName;
    int alignH = 0, alignV = 0;  // left/centre/right, bottom/middle/top: 0, 1, 2
    unsigned char colour[4] = {255, 255, 255, 255};
    float x = 0, y = 0, width = 0, height = 0;
    float rotation = 0;  // degrees
    int layoutMode = kUIAbsolute;
};

// A layout as read (0x4ffcc; 0x1d0c bytes: up to 192 items, 64 texts).
struct UIDefinition {
    std::string name;
    std::vector<UIItemDef> items;
    std::vector<UITextDef> texts;
    const UITextDef* text(const std::string& id) const;  // 0x2e568
};
UIDefinition loadUIDefinition(const char* path);  // 0x4ffcc

// A layout built (0x2d36c; 0x33c bytes, a screen's +0xd4): every node by its
// index and name; the two menus (menuB under menuA).
struct UIInstance {
    std::map<cocos2d::CCNode*, int> indexOf;
    std::vector<cocos2d::CCNode*> nodes;  // items, then texts
    std::map<std::string, cocos2d::CCNode*> byName;
    int count = 0;
    cocos2d::CCMenu* menuA = nullptr;  // buttons, toggles, front images
    cocos2d::CCMenu* menuB = nullptr;  // the other images, behind

    cocos2d::CCNode* node(const std::string& name);                // 0x2e4c0
    cocos2d::CCMenuItemImage* item(const std::string& name);       // 0x2e488
    cocos2d::CCLabelBMFont* label(const std::string& name);        // 0x2e530
    void setVisible(const std::string& name, bool visible);        // 0x2e8e0
    // a text field's text, with a space added for each control character in
    // it (up to 32), as the original did (0x2e8f8)
    void setText(const std::string& name, const std::string& text);
    // the item's normal image becomes frame "<prefix>-<suffix>.png" (0x2e5a0)
    void setItemImage(const UIDefinition& def, const std::string& name, const std::string& suffix);
    // the name of the item a sender is ("" if none)
    std::string nameOf(const UIDefinition& def, cocos2d::CCObject* sender);
};
// Builds the layout into `parent`: buttons call target->selector(item).
void buildUI(UIInstance& ui, cocos2d::CCNode* parent, const UIDefinition& def, cocos2d::CCObject* target,
             cocos2d::SEL_MenuHandler selector);

// A canvas position given relative to an edge or corner the screen shows (0x50e00).
cocos2d::CCPoint anchorOnCanvas(int mode, float x, float y);
// A label in one of the game's fonts, `width` in canvas points (0xa9e7c).
cocos2d::CCLabelBMFont* createLabel(const char* text, const char* font, float width,
                                    cocos2d::CCTextAlignment alignment);
// The text field of a layout as a label (0x2e248).
cocos2d::CCLabelBMFont* createTextLabel(const UITextDef& t);
// The sheets' frames loaded, and a batch node of each added to `scene` (0x2cd08).
void loadSheets(cocos2d::CCNode* scene, const std::vector<std::string>& names);
// The sheets' frames taken out of the frame cache, and their textures (.png
// and .pvr.ccz) out of the texture cache (0x2d094).
void unloadSheets(const std::vector<std::string>& names);

}  // namespace mt
