// mt::EditorLevelsScreen: the list of the surgery editor's levels (the
// port's; the original's editor opened on one surgery and switched
// between its versions, VersionSelectorPopup 0x860f30). docs/as3.md,
// "Surgery editor".
//
// The levels in <save folder>/editedLevels/: played, edited, their share
// code copied, deleted; a new one made, or one started from a patient's
// surgery; a code pasted from the clipboard or files imported from
// editedLevels/import/. A slot that has finished the game (the Bunker's
// Hubris 2 won in its main mode) has the editor; any other slot whose
// world map is open may play the levels shared with it, without editing.
// It is reached from the Bunker's map (addEditorEntry).
#pragma once

#include <string>
#include <vector>

#include "as3/EditorLevel.h"
#include "as3/EditorWidgets.h"

namespace mt {

struct AreaDef;
class Scene;

class EditorLevelsScreen : public editor::PanelScene {
public:
    static EditorLevelsScreen* create();
    void load() override;

private:
    void refresh();
    void back() override;
    void pasteCode();
    void importFiles();
    void startNew();
    void openPatients(int page);
    void edit(const editor::LevelFile& f);
    void playFile(const editor::LevelFile& f);
    void copyCode(const editor::LevelFile& f);
    void remove(const editor::LevelFile& f);

    editor::Panel* m_list = nullptr;
    std::vector<editor::LevelFile> m_files;
    int m_page = 0;
    bool m_unlocked = false;
};

// The Bunker's map's button to the list (AreaScreen::load's hook): shown
// once the slot's world map is open; "EDITOR" once the game is finished,
// else "SHARED".
void addEditorEntry(Scene* areaScreen, const AreaDef& area);

}  // namespace mt
