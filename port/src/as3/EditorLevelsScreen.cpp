#include "as3/EditorLevelsScreen.h"

#include <algorithm>
#include <cstdio>

#include "as3/AreaScreen.h"
#include "as3/AreasDefUtils.h"
#include "as3/EditorPlay.h"
#include "as3/EditorScreen.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCLayer.h"
#include "engine/cocos/CCMenu.h"
#include "engine/cocos/CCSprite.h"
#include "platform/Platform.h"

namespace mt {

using namespace cocos2d;
using namespace editor;

namespace {

constexpr int kRows = 5;
// the screen (points: 512 to 640 wide, 320 to 384 high by its shape)
float screenW() { return static_cast<float>(ResolutionMgr::sharedResolutionMgr()->width); }
float screenH() { return static_cast<float>(ResolutionMgr::sharedResolutionMgr()->height); }

std::string counts(const Level& l) {
    int injuries = 0;
    for (const LevelStage& s : l.stages) injuries += static_cast<int>(s.injuries.size());
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d stage%s, %d injur%s", static_cast<int>(l.stages.size()),
                  l.stages.size() == 1 ? "" : "s", injuries, injuries == 1 ? "y" : "ies");
    return buf;
}

// The map's button: a menu item made of the editor's plates.
class Entry : public CCObject {
public:
    void pressed(CCObject*) {
        if (SceneMgr::sharedSceneMgr()->sceneCount() > 1) return;  // (a popup over the map)
        SoundMgr::sharedSoundMgr()->playSound("button_click", false);
        SceneMgr::sharedSceneMgr()->replaceScene(EditorLevelsScreen::create());
    }
};

}  // namespace

void addEditorEntry(Scene* screen, const AreaDef& area) {
    // the Bunker's map, once the world map is open (the second patient won)
    const SavedGame& slot = SaveData::shared().currentSlot();
    if (area.areaId != "area_bunker" || slot.starsOf("surgery_bunker_prxy4", 0) == 0 || !screen->ui.menuA) return;
    static Entry* entry = new Entry;
    const bool unlocked = editorUnlocked();
    const char* label = unlocked ? "EDITOR" : "SHARED";
    auto face = [label, unlocked](bool down) {
        CCNode* n = CCNode::create();
        CCDrawNode* p = plate(96, 34, unlocked ? Tone::Purple : Tone::Blue, 9);
        n->addChild(p);
        if (CCLabelBMFont* t = text(label, 15, 86)) {
            t->setPosition(ccp(48, 18));
            n->addChild(t);
        }
        n->setContentSize(CCSizeMake(96, 34));
        if (down) n->setScale(0.94f), n->setPosition(ccp(3, 1));
        return n;
    };
    CCMenuItemSprite* item = CCMenuItemSprite::create(face(false), face(true), entry, menu_selector(Entry::pressed));
    item->setPosition(ccp(screenW() - 4 - 48, 4 + 17));
    screen->ui.menuA->addChild(item);
    log("editor: entry %s on the Bunker's map", label);
}

EditorLevelsScreen* EditorLevelsScreen::create() {
    EditorLevelsScreen* s = new EditorLevelsScreen;
    s->setName("EditorLevelsScreen");
    s->init();
    s->autorelease();
    return s;
}

void EditorLevelsScreen::load() {
    PanelScene::load();
    m_unlocked = editorUnlocked();
    // the Bunker's picture under a shade
    if (const AreaDef* bunker = AreasDefUtils::shared()->byId("area_bunker")) {
        loadSheets(this, {bunker->bgSpritesheet});
        if (CCSprite* bg = CCSprite::createWithSpriteFrameName(bunker->bgSprite.c_str())) {
            setPositionOnCanvas(bg, 320, 221);
            addChild(bg, -5);
        }
    }
    const float kW = screenW(), kH = screenH();
    addChild(CCLayerColor::create(ccc4(0, 0, 0, 120), kW, kH), -4);
    CCDrawNode* title = plate(300, 34, m_unlocked ? Tone::Purple : Tone::Blue, 10);
    title->setPosition(ccp((kW - 300) / 2, kH - 40));
    addChild(title, 1);
    if (CCLabelBMFont* t = text(m_unlocked ? "SURGERY EDITOR" : "SHARED SURGERIES", 19, 280)) {
        t->setPosition(ccp(kW / 2, kH - 22));
        addChild(t, 2);
    }
    Panel* bar = Panel::create(false);
    addPanel(bar, 10);
    bar->add(Button::create("BACK", 80, 30, Tone::Orange, [this] { back(); }, 14), 6, kH - 38);
    // (the buttons along the bottom, laid out for 568 points, shrunk to fit
    // a narrower screen, their touches with them)
    Panel* low = Panel::create(false);
    addPanel(low, 10);
    low->setScale(std::min(1.0f, kW / 568));
    float x = 8;
    if (m_unlocked) {
        low->add(Button::create("NEW SURGERY", 128, 30, Tone::Green, [this] { startNew(); }), x, 6);
        x += 136;
        low->add(Button::create("FROM A PATIENT", 136, 30, Tone::Green, [this] { openPatients(0); }), x, 6);
        x += 144;
    }
    low->add(Button::create("PASTE CODE", 124, 30, Tone::Purple, [this] { pasteCode(); }), x, 6);
    x += 132;
    low->add(Button::create("IMPORT FILES", 128, 30, Tone::Blue, [this] { importFiles(); }), x, 6);
    if (!m_unlocked) {
        CCLabelBMFont* note = text("Finish the game in this slot to unlock the editor.", 11, 280);
        if (note) {
            note->setAnchorPoint(ccp(0, 0.5f));
            note->setPosition(ccp(x + 136, 21));
            low->addChild(note, 2);
        }
    }
    m_list = Panel::create(false);
    addPanel(m_list, 5);
    refresh();
    SoundMgr::sharedSoundMgr()->stopMusic(true);
}

void EditorLevelsScreen::refresh() {
    m_files = listLevels();
    const float kW = screenW(), kH = screenH();
    m_list->removeAllChildrenWithCleanup(true);
    m_list->buttons.clear();
    const int pages = std::max(1, (static_cast<int>(m_files.size()) + kRows - 1) / kRows);
    m_page = std::max(0, std::min(m_page, pages - 1));
    if (m_files.empty()) {
        if (CCLabelBMFont* t = text(m_unlocked ? "No surgeries yet: make one, or paste a code someone shared."
                                               : "No surgeries yet: paste a code someone shared with you.",
                                    13, 500)) {
            t->setPosition(ccp(kW / 2, kH / 2 + 20));
            m_list->addChild(t);
        }
    }
    for (int k = 0; k < kRows; ++k) {
        int idx = m_page * kRows + k;
        if (idx >= static_cast<int>(m_files.size())) break;
        const LevelFile f = m_files[static_cast<size_t>(idx)];
        const float y = kH - 84 - 40 * static_cast<float>(k);
        CCDrawNode* row = plate(kW - 16, 36, Tone::Dark, 8);
        row->setPosition(ccp(8, y));
        m_list->addChild(row);
        std::string name = f.error.empty() ? f.title : "(can't be read) " + f.file;
        if (CCLabelBMFont* t = text(name, 13, 250)) {
            t->setAnchorPoint(ccp(0, 0.5f));
            t->setPosition(ccp(18, y + 23));
            m_list->addChild(t, 2);
        }
        std::string sub = f.error.empty() ? "version " + std::to_string(f.version) + (f.imported ? "  - shared with you" : "")
                                          : f.error;
        if (CCLabelBMFont* t = text(sub, 9, 250)) {
            t->setAnchorPoint(ccp(0, 0.5f));
            t->setPosition(ccp(18, y + 9));
            t->setColor(f.error.empty() ? ccc3(200, 200, 200) : ccc3(255, 120, 100));
            m_list->addChild(t, 2);
        }
        float bx = kW - 14 - 50;
        m_list->add(Button::create("DEL", 46, 28, Tone::Red, [this, f] { remove(f); }, 11), bx, y + 4);
        if (f.error.empty()) {
            bx -= 62;
            m_list->add(Button::create("CODE", 58, 28, Tone::Purple, [this, f] { copyCode(f); }, 11), bx, y + 4);
            if (m_unlocked) {
                bx -= 62;
                m_list->add(Button::create("EDIT", 58, 28, Tone::Orange, [this, f] { edit(f); }, 11), bx, y + 4);
            }
            bx -= 66;
            m_list->add(Button::create("PLAY", 62, 28, Tone::Green, [this, f] { playFile(f); }, 11), bx, y + 4);
        }
    }
    if (pages > 1) {
        m_list->add(Button::create("<", 36, 26, Tone::Blue, [this] {
                        --m_page;
                        refresh();
                    }, 14), kW / 2 - 70, 46);
        if (CCLabelBMFont* t = text(std::to_string(m_page + 1) + " / " + std::to_string(pages), 12, 60)) {
            t->setPosition(ccp(kW / 2, 59));
            m_list->addChild(t);
        }
        m_list->add(Button::create(">", 36, 26, Tone::Blue, [this] {
                        ++m_page;
                        refresh();
                    }, 14), kW / 2 + 34, 46);
    }
}

void EditorLevelsScreen::back() {
    const AreaDef* bunker = AreasDefUtils::shared()->byId("area_bunker");
    if (bunker) SceneMgr::sharedSceneMgr()->replaceScene(AreaScreen::create(*bunker, 0));
}

void EditorLevelsScreen::pasteCode() {
    std::string error;
    Level l;
    if (!decodeCode(platform::clipboardText(), l, error)) {
        log("editor: paste refused: %s", error.c_str());
        message("CAN'T PASTE", error);
        return;
    }
    l.imported = true;
    confirm("ADD THIS SURGERY?", "\"" + l.title + "\"\n" + counts(l), "ADD", [this, l] {
        std::string id, file, error2;
        if (!saveLevel(l, id, file, true, error2)) {
            message("NOT ADDED", error2);
            return;
        }
        log("editor: pasted \"%s\" as %s", l.title.c_str(), file.c_str());
        m_page = 0;
        refresh();
    });
}

void EditorLevelsScreen::importFiles() {
    std::string msg;
    importFolder(msg);
    m_page = 0;
    refresh();
    message("IMPORT FILES", msg);
}

void EditorLevelsScreen::startNew() {
    Session& s = session();
    s = Session();
    s.level = newLevel();
    SceneMgr::sharedSceneMgr()->replaceScene(EditorScreen::create());
}

void EditorLevelsScreen::openPatients(int page) {
    if (popupOpen()) popPopup();
    const std::vector<Catalog::Patient>& all = Catalog::shared().patients;
    const int perPage = 10, pages = std::max(1, (static_cast<int>(all.size()) + perPage - 1) / perPage);
    page = std::max(0, std::min(page, pages - 1));
    Popup* p = Popup::create("START FROM A PATIENT'S SURGERY", 460, 290, Tone::Green);
    for (int k = 0; k < perPage; ++k) {
        int idx = page * perPage + k;
        if (idx >= static_cast<int>(all.size())) break;
        const Catalog::Patient pt = all[static_cast<size_t>(idx)];
        p->put(Button::create(pt.title, 206, 30, Tone::Grey, [this, pt] {
                   Level l;
                   std::string error;
                   if (!Catalog::shared().levelFromSurgery(pt.id, l, error)) {
                       log("editor: can't open %s: %s", pt.id.c_str(), error.c_str());
                       message("CAN'T OPEN", error);
                       return;
                   }
                   Session& s = session();
                   s = Session();
                   s.level = l;
                   s.dirty = true;
                   SceneMgr::sharedSceneMgr()->replaceScene(EditorScreen::create());
               }, 11),
               18 + 218 * static_cast<float>(k % 2), 214 - 36 * static_cast<float>(k / 2));
    }
    p->put(Button::create("<", 36, 28, Tone::Blue, [this, page] { openPatients(page - 1); }, 14), 18, 14);
    p->putText(std::to_string(page + 1) + " / " + std::to_string(pages), 90, 28, 12, 60, true);
    p->put(Button::create(">", 36, 28, Tone::Blue, [this, page] { openPatients(page + 1); }, 14), 126, 14);
    p->put(Button::create("CLOSE", 100, 30, Tone::Red, [this] { popPopup(); }), 340, 14);
    pushPopup(p);
}

void EditorLevelsScreen::edit(const LevelFile& f) {
    Level l;
    std::string error;
    if (!loadLevel(f.file, l, error)) {
        message("CAN'T OPEN", error);
        return;
    }
    Session& s = session();
    s = Session();
    s.level = l;
    s.id = f.id;
    s.file = f.file;
    SceneMgr::sharedSceneMgr()->replaceScene(EditorScreen::create());
}

void EditorLevelsScreen::playFile(const LevelFile& f) {
    Level l;
    std::string error;
    if (!loadLevel(f.file, l, error) || !playable(l, error)) {
        message("CAN'T PLAY", error);
        return;
    }
    SceneMgr::sharedSceneMgr()->replaceScene(playLevel(l, Back::Levels));
}

void EditorLevelsScreen::copyCode(const LevelFile& f) {
    Level l;
    std::string error;
    if (!loadLevel(f.file, l, error) || !playable(l, error)) {
        message("NO CODE", error);
        return;
    }
    l.imported = false;  // (the receiver marks it shared)
    std::string code = encodeCode(l);
    bool copied = platform::setClipboardText(code);
    log("editor: code %s", code.c_str());
    message("SHARE CODE", copied ? "The code (" + std::to_string(code.size()) +
                                       " letters) is copied: paste it anywhere to share this surgery."
                                 : "The code couldn't be copied.");
}

void EditorLevelsScreen::remove(const LevelFile& f) {
    confirm("DELETE", "Delete \"" + (f.title.empty() ? f.file : f.title) + "\" (version " + std::to_string(f.version) +
                          ")?",
            "DELETE", [this, f] {
                deleteLevel(f.file);
                refresh();
            });
}

}  // namespace mt
