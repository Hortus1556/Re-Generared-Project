// mt::Animation (made by 0x5932c; 0x19c bytes; vtable 0x4149d0): the game's
// data-driven frame animations, from common/anim_*_anim.json (Mediatonic's
// export of Flash timelines), drawn with the sheets' frames. A node: its
// sprites are its children.
//
// The file (0x59488):
// - frame_rate: the timelines' frames a second;
// - clips: each a Flash movie clip whose frames are the sprite frames
//   "<clip>_1.png".."<clip>_<frames>.png" (frames.frames of them), placed at
//   (x, y) on the 640x384 stage (y down), each frame's picture offset by
//   frames.offset (given for some frames; the others repeat the last given),
//   drawn at z depth*10; a clip's control points (cp: named points, a
//   position each frame) are nodes that follow it, to which other nodes
//   can be attached (a maggot's breath, its tail);
// - each clip's props: named timelines, a list of segments, each frames
//   from..to (backwards if to < from) played `count` times, -1 for ever.
// The clips' props of the same name make one named animation ("hanging":
// the bat's head and body), as long as its longest timeline; playing it
// shows those clips (only), each on its own timeline.
//
// The node's own origin is the stage's bottom-left corner (canvas (0, 0)),
// in cocos points (canvas points × canvasScale): its user places it where
// the clips' registration point is to be (a creature at its injury).
//
// Playing (0x5a800): a named animation, a number of times (-1: for ever),
// maybe backwards, stepped by a timer at the frame rate; queue (0x5aa40)
// adds one to play after it (a queue of 10). stop, pause, resume; one frame
// shown (0x5a64c). Its names are the port's (the binary has none).
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/Json.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

class Animation : public cocos2d::CCNode {
public:
    // 0x5932c (new_0x19c): `file` read (common/anim_<name>_anim.json); with
    // `prepare` 1, its sprites made at once (prepare(): the clips' sheets
    // must be loaded).
    static Animation* create(const char* file, int prepare);
    ~Animation() override;  // 0x590fc

    // reads the file (0x59488)
    bool initWithFile(const char* file);
    // the clips' frames from the sprite frame cache, a sprite each and a
    // node each control point (0x5a1a4)
    void prepare();

    // plays `name` `loops` times (-1: for ever), backwards if `reverse`, from
    // `startFrame` (1 the first; -1 the start); what was queued is dropped
    // (0x5a800)
    void play(const char* name, int loops, bool reverse, int startFrame = -1);
    // to play when what plays ends; one playing for ever then ends at its
    // end (0x5aa40)
    void queue(const char* name, int loops, bool reverse);
    // shows `name` at frame `frame` (0 the first; one step on from it the way
    // it last played), not playing (0x5a64c)
    void showFrame(const char* name, int frame);
    // stops where it is (0x59300); ignored while paused (as the original)
    void stop();
    void pause();      // 0x5abd0
    void resume();     // 0x5a9f8
    bool isPlaying();  // 0x5aa24: playing and not paused
    // the animation playing (or paused), nullptr if none (0x5abf0)
    const char* currentName();
    // the frames `name` lasts (its longest timeline) (0x5ac00)
    int lengthOf(const char* name);
    // the first clip's sprite's size (0x5ad84)
    cocos2d::CCSize getSpriteSize();
    // `node` follows the control point `controlPoint` of the clips showing
    // (a child of its node), now and whenever an animation starts (0x5ada4)
    void attach(const char* controlPoint, cocos2d::CCNode* node);
    void setColor(const cocos2d::ccColor3B& color);  // 0x5af60: every clip's sprite
    void setOpacity(float opacity);                  // 0x5af90: every clip's sprite
    void setFlipX(bool flip);                        // 0x5afcc: every clip's sprite

    // a step of the timer (0x5b014): the next frame; at the end, again, the
    // queued one, or stop
    void update(float dt) override;

    // not in the original: the frame counter (1 the first), for the debug viewer
    int frame() const { return m_frame; }
    bool isPrepared() const { return m_prepared; }

private:
    struct ControlPoint {  // 0x10 bytes
        std::string name;
        std::vector<cocos2d::CCPoint> pos;  // each frame, from the clip's sprite's centre
    };
    struct Clip {  // 0x4c bytes
        std::string name;    // +0: the frames' prefix
        int x = 0, y = 0;    // +4: the stage's (y down); prepare() makes them its first frame's centre on the canvas
        int depth = 0;       // +0xc: z order depth*10
        int rotation = 0;    // +0x10: read, not used
        int frameCount = 0;  // +0x14
        std::vector<cocos2d::CCPoint> offsets;           // +0x18: each frame (y up)
        std::vector<ControlPoint> controlPoints;         // +0x24
        cocos2d::CCSprite* sprite = nullptr;             // +0x30 (retained)
        std::vector<cocos2d::CCSpriteFrame*> frames;     // +0x34 (retained)
        std::vector<cocos2d::CCNode*> controlNodes;      // +0x40 (retained)
    };
    struct Segment {  // 0x14 bytes
        int from = 1, to = 1;  // frames, 1 the first
        int count = 1;         // -1: for ever
        int length = 1;        // |to - from| + 1
        int total = 1;         // length × count (length when for ever)
    };
    struct Timeline {  // 0x10 bytes: a clip's part in a named animation
        Clip* clip = nullptr;
        std::vector<Segment> segments;
    };
    struct Prop {  // a named animation (the map's value)
        std::string name;                // +0
        std::vector<Timeline> timelines; // +4
        int length = 0;                  // +0x10: frames, its longest timeline
    };
    struct Queued {  // 0xc bytes
        std::string name;
        int loops = 0;
        bool reverse = false;
    };
    struct Attachment {  // 8 bytes
        std::string controlPoint;
        cocos2d::CCNode* node = nullptr;  // not retained (as the original)
    };
    static constexpr int kQueueSize = 10;  // AnimationConstants::QueueMaxSize

    Animation() = default;
    // a clip's prop: its timeline added to the named animation (0x5a384)
    void addTimeline(Clip* clip, const std::string& name, const json::Value& timeline);
    // the attached nodes taken off their control points (0x5a7b8)
    void detachAll();

    std::vector<std::unique_ptr<Clip>> m_clips;  // +0xc8, in the file's key order (sorted)
    std::map<std::string, Prop> m_props;         // +0xd4
    int m_frameRate = 30;                        // +0xec
    bool m_prepared = false;                     // +0xf0
    int m_frame = 0;                             // +0xf4: the counter, 1 the first frame
    bool m_reverse = false;                      // +0xf8
    Prop* m_current = nullptr;                   // +0xfc
    int m_loopsLeft = 0;                         // +0x100: after this pass; -1 for ever
    bool m_addPending = true;                    // +0x104: the sprites to add at the next step
    Queued m_queue[kQueueSize];                  // +0x108
    int m_queueTail = -1;                        // +0x180: the last queued
    int m_queueHead = -1;                        // +0x184: the last taken
    bool m_flipX = false;                        // +0x188
    std::vector<Attachment> m_attachments;       // +0x18c
    bool m_paused = false;                       // +0x198
};

}  // namespace mt
