// The surgery editor's levels in play, and what the game's screens ask of
// it (docs/as3.md, "Surgery editor").
//
// A level is played by the game's own SurgeryScreen: the level's JSON is
// given to it as a content file named "edited:play" (SurgeryParams::name,
// the original's SurgeryInfoState+4: the path of an edited version that
// SurgeryUtils::getSurgeryToLoadFullPath 0x75af90 loaded instead of the
// surgery's file; the original's test, EditorScreen::testCurrentSurgery
// 0x4504ac, started the surgery so). Unlike the original's test, a level's
// play is kept apart from the campaign: no tag partner, no stars, coins,
// failures, revives or stats are kept (the slot is put back as it was when
// play began), and its end shows the editor's own results, then goes back
// to the editor or the list of levels.
#pragma once

#include <string>

#include "as3/EditorLevel.h"
#include "core/Json.h"

namespace mt {

class Scene;
struct SurgeryParams;
struct SurgeryResult;

namespace editor {

// What the editor is editing, kept while a level is tested.
struct Session {
    Level level;
    std::string id, file;  // its files' id and the version loaded ("" none yet)
    bool dirty = false;    // changed since saved
    int stage = 1;         // the stage shown (1-based)
    int mode = 0;          // 0 the injuries, 1 the extra pictures (EditorMode)
};
Session& session();

// The slot has finished the game (won the story's last surgery, the
// Bunker's Hubris 2, in its main mode): the editor is open to it.
bool editorUnlocked();

enum class Back { Editor, Levels };
// The surgery screen playing a level (the slot's state kept aside first).
Scene* playLevel(const Level& level, Back back);
// A content file of a level in play ("edited:play"): Content.cpp's readContent.
bool isEditedName(const std::string& name);
bool readEditedLevel(const std::string& name, json::Value& out);
// The surgery screen's params are a level's.
bool isEditedPlay(const SurgeryParams& params);
// The surgery over (SurgeryScreen::finishWin / finishLose): the slot put
// back, the editor's results shown over it.
void playFinished(const SurgeryResult& result, bool won);
// The pause screen's quit and retry: the slot put back, the screen to go to.
Scene* leavePlay();
Scene* replay();
// (debug mode) AS_EDITED=<file>: that level played at once (a file of
// editedLevels/, or a path), back to the list of levels after.
Scene* editedLevelFromEnv();

}  // namespace editor
}  // namespace mt
