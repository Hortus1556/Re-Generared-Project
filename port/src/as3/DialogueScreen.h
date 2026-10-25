// mt::DialogueScreen (0x23ec bytes; vtable 0x4164e8): the story scenes, a
// surgery's intro and outro and an area's ending, from Data/Dialogues/.
//
// A scene file is a list of lines ("scene"): each with its background, up to
// four characters (full-canvas pictures: 1 and 3 from the left, 2 and 4
// mirrored from the right, or any of them centred, abs_N), which of them
// are lit (highlighted: the others greyed), its text (or narration), the
// ambience and a sound to start, and where the camera looks. A line can ask
// for the player's tag partner ("TAG" stands for their picture,
// tag_partner_required) or for a number of the area's surgeries done
// (required: Completed_1..3); the lines that don't apply are left out.
// Next steps through them; skip, or next on the last, ends the scene: the
// intro goes to the surgery's pre-op screen, the outro to its results, the
// area's ending back to the world map, an area's intro (Android 1.43's
// Hospital) to its map.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "as3/Scene.h"
#include "as3/SurgeryInfo.h"

namespace mt {

class DialogueScreen : public Scene {
public:
    // a surgery's intro (0xc5e58: mode 0)
    static DialogueScreen* create(const SurgeryInfo& surgery, const SurgeryParams& params);
    // an area's ending (0xc6c54: mode 2), then the world map at `worldArea`
    static DialogueScreen* createAreaComplete(const AreaDef& area, int worldArea);
    // (Android 1.43) an area's intro (sceneAreaIntro 0x41f418: mode 3), its
    // first visit from the world map, then its map
    static DialogueScreen* createAreaIntro(const AreaDef& area);
    // a surgery's outro (0xc6b60: mode 1), then its results
    static DialogueScreen* createOutro(const SurgeryInfo& surgery, const SurgeryParams& params,
                                       const SurgeryResult& result);

    void load() override;  // vf87

private:
    enum Mode { kIntro = 0, kOutro = 1, kAreaComplete = 2, kAreaIntro = 3 };
    struct Line {  // 0x44 bytes
        std::string required;   // +0
        std::string bg;         // +4
        std::string chars[4];   // +8
        bool centred[4] = {};   // +0x18: abs_1..4
        std::vector<int> highlighted;  // +0x1c
        std::string bgm;        // +0x28
        std::string sfx;        // +0x2c
        std::string text;       // +0x30: the "dialogue" field
        std::string narrative;  // +0x34
        float cameraX = 0, cameraY = 0;  // +0x38
        std::string tagPartnerRequired;  // +0x40
    };

    DialogueScreen();  // 0xd3198
    using Scene::init;
    bool init(int mode, const std::string& file, const std::vector<std::string>& sheets);
    void show(const Line& line);  // 0xcaca8
    void onButton(cocos2d::CCObject* sender);  // 0xca9f8
    void finish();                // 0xcb7b4
    int completedInArea();        // 0xca9a8: the area's surgeries with stars

    SurgeryInfo m_surgery;        // +0x212c
    SurgeryParams m_params;       // +0x2250
    SurgeryResult m_result;       // +0x2288
    std::string m_file;           // +0x2264: the scene's file
    std::vector<std::string> m_sheets;  // +0x2268
    int m_mode = kIntro;          // +0x2284
    AreaDef m_area;               // +0x22c0 (the area's ending)
    int m_worldArea = 0;          // +0x2370
    UIDefinition m_def;           // +0x420

    cocos2d::CCSprite* m_camera = nullptr;  // +0x2374: holds the picture, moved by the camera
    cocos2d::CCSprite* m_bg = nullptr;      // +0x2378
    cocos2d::CCSprite* m_chars[4] = {};     // +0x237c
    cocos2d::CCPoint m_cameraAt;            // +0x23a0
    int m_index = 0;                        // +0x23ac
    std::vector<Line> m_lines;              // +0x23b0
    std::string m_ambience;                 // +0x23bc: the line's bgm last asked for
    std::string m_partner;                  // +0x23c0: the tag partner's picture sheet suffix
    std::map<std::string, std::string> m_partnerPictures;  // +0x23c4
    int m_offsets[4] = {-60, -40, 30, 30};  // +0x23dc: the characters' x from their edge
};

}  // namespace mt
