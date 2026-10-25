// mt::EditorScreen: the surgery editor (Android 1.43 only: EditorScreen
// 0x446874.., 74 functions; never shown: EditorScreen::scene 0x446e2c has
// no caller, and iOS stripped it). docs/as3.md, "Surgery editor".
//
// Ported from the Android build: the stage shown with its background, its
// extra pictures and its injuries drawn by their own classes
// (refreshScene 0x44bebc: setupBackgroundForSurgeryState,
// setupInjurySpritesForSurgeryState); two modes, the injuries and the
// extra pictures (switchToEditorMode 0x450840, "<Switch to bg editing>");
// an injury picked by a touch on it, the topmost first, and dragged
// (update 0x4498f0, processTouchMovedEvent 0x44caf0); turned left and right
// (clickedRotateLeftButton 0x44a4f0: a degree a press), deleted
// (clickedDeleteButton 0x44a718); a new one from the toolbox of injury
// classes at the screen's centre, named <class>_<n> after the highest
// number used (clickedToolboxButton 0x449e44, generateNameForNewInjury
// 0x44a0ec); the stages stepped through, a new one past the last when this
// one has injuries (clickedStageButton 0x449c9c, createNewStage 0x44ce5c:
// its picture and music the stage's, health and time -1); the stage's
// properties (clickedStagePropsButton 0x44e7ec: name, bg, bg_flip_x/y, bgm,
// body_level, to_surgery_stage, health, time), the injury's (refresh/apply
// PropertyToolbox 0x44cfc0/0x44d8d4: name, x, y, rotation,
// to_surgery_stage), the surgery's (openSurgeryPropertiesPopup 0x44def4:
// is_challenge, is_sudden_death, the stars' scores and times) and tools
// (openAvailableToolsPopup 0x44e1a0); an injury's visual and interaction
// dependencies (clickedVisualDepsButton 0x44a9e4, ...InteractionDeps
// 0x44ad7c: the stage's other injuries ticked); the stage's injury list,
// reordered and deleted (InjuryListPopup); the menu (EditorMenu 0x43aa2c:
// switch version, save overwrite / new, test, delete this version,
// discard changes, the surgery's metadata and tools, close).
//
// The port's: it looks like the game (EditorWidgets.h) and works by mouse
// and touch; the toolbox is a palette of the injury classes drawn as they
// look, in groups, dragged out onto the patient or tapped; injuries turn
// by a handle as well as by buttons; the extra pictures and the stage's
// background are picked from pictures; the email button
// (sendCurrentSurgeryByEmail 0x45036c) is the share code and the file
// export; a test plays the level apart from the campaign (EditorPlay.h).
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "as3/EditorPlay.h"
#include "as3/EditorWidgets.h"
#include "as3/SurgeryData.h"

namespace cocos2d {
class CCSprite;
}

namespace mt {

class InjuryState;

class EditorScreen : public editor::PanelScene {
public:
    static EditorScreen* create();  // (scene 0x446e2c) edits editor::session()
    ~EditorScreen() override;
    void load() override;     // loadAssets 0x446f64
    void onRemoved() override;
    void keyBackClicked() override;  // Escape: the top popup closed, else the menu

private:
    enum Drag { kNoDrag, kMove, kRotate };
    enum Palette { kInjuries, kPictures, kBackgrounds };

    editor::LevelStage& stage();
    void buildBars();
    void refreshBars();
    void showStage();                 // refreshScene 0x44bebc
    void showBackground();
    void buildView(size_t index);     // one injury drawn by its class
    void clearViews();
    void refreshOverlay();
    void select(int index);           // selectInjury 0x44c1f8 / selectBGImage 0x44c308
    void changeStage(int stage);      // changeStage 0x44be6c
    void stepStage(int by);           // clickedStageButton 0x449c9c
    void createNewStage();            // 0x44ce5c
    void deleteStage();
    void setMode(int mode);           // switchToEditorMode 0x450840
    void addInjury(const std::string& className, const cocos2d::CCPoint& at);  // clickedToolboxButton 0x449e44
    void addPicture(const std::string& frame, const cocos2d::CCPoint& at);
    void rotateSelected(int by);      // clickedRotateLeft/RightButton 0x44a4f0/0x44a604
    void deleteSelected();            // clickedDeleteButton 0x44a718
    void duplicateSelected();
    std::string newName(const std::string& className);  // generateNameForNewInjury 0x44a0ec
    void changed();                   // +0x468: unsaved changes

    // the popups
    void openMenu();                  // EditorMenu
    void openPalette(Palette kind, int page);
    void openStageProps();            // clickedStagePropsButton 0x44e7ec
    void openInjuryProps();           // the property toolbox
    void openPictureProps();          // applyChangesToBGObject 0x44dbb0's fields
    void openSurgeryProps();          // openSurgeryPropertiesPopup 0x44def4
    void openTools();                 // openAvailableToolsPopup 0x44e1a0
    void openDeps(bool visual, int page);  // clickedVisualDeps/InteractionDepsButton
    void openInjuryList(int page);    // clickedInjuryListButton 0x44eb08, InjuryListPopup
    void openVersions();              // openVersionSelectorPopup 0x4503e8
    void save(bool newVersion);       // saveSurgeryOverwrite 0x450114 / saveSurgeryNew 0x44f958
    void test();                      // testCurrentSurgery 0x4504ac
    void shareCode();                 // (sendCurrentSurgeryByEmail 0x45036c's place)
    void exportFile();
    void discard();                   // discardUnsavedChanges 0x4507d4
    void deleteVersion();             // deleteCurrentVersion 0x4505a8
    void close();
    void back() override;
    void clearPreviews();

    bool canvasBegan(const cocos2d::CCPoint& p) override;  // processTouchBeganEvent 0x44c584
    void canvasMoved(const cocos2d::CCPoint& p) override;  // processTouchMovedEvent 0x44caf0
    void canvasEnded(const cocos2d::CCPoint& p) override;  // processTouchEndEvent 0x44cce0

    // the sheets: each loaded once, under its own node so it can be let go
    void needSheets(const std::vector<std::string>& sheets);
    void trimSheets(const std::vector<std::string>& keep);
    const InjuryDef* def(const std::string& className);

    cocos2d::CCSprite* m_bg = nullptr;
    cocos2d::CCNode* m_bgExtras = nullptr;
    cocos2d::CCNode* m_injuryLayer = nullptr;
    cocos2d::CCDrawNode* m_overlay = nullptr;
    cocos2d::CCNode* m_sheetNode = nullptr;
    std::map<std::string, cocos2d::CCNode*> m_sheets;
    std::map<std::string, InjuryDef> m_defs;
    std::vector<InjuryState*> m_views;          // the stage's injuries, drawn
    std::vector<InjuryState*> m_previews;       // the palette's
    editor::Panel* m_top = nullptr;
    editor::Panel* m_bottom = nullptr;
    editor::Button* m_stageButton = nullptr;
    editor::Button* m_modeButton = nullptr;
    cocos2d::CCLabelBMFont* m_info = nullptr;
    int m_selected = -1;
    Drag m_drag = kNoDrag;
    cocos2d::CCPoint m_grab, m_dragFrom;
    bool m_dragged = false;
    int m_rotation = 0;  // the handle's, while it is dragged
};

}  // namespace mt
