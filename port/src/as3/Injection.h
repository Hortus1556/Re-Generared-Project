// mt::Injection (vtable 0x4151e0; 0x62c bytes; ctor 0x6c4d0): the syringe.
// Two modes, chosen with the HUD's two buttons ("ingame_tool_Injection" and
// "ingame_tool_Injection_out", the screen's 0x6de50):
// - in (mode 1): pressed on the patient it is stuck in there (planted), its
//   needle where pressed and its body turned towards the middle of the
//   canvas; pushing its plunger down heals (event 5, by how far it went), as
//   long as it is not pushed too fast (the pushes of the last 0.25 s added
//   up against its rate); too fast or pulled back: damage, and it is done.
//   Fully pushed it is done; then it cools down.
// - out (mode 0): stuck into something to extract (a boil, a leech, poison:
//   their classes' injection hit test read its fill), its plunger is pulled
//   up, slowly enough; the fluid takes the colour of what it draws.
// The upgrades: its fluid (heal 1, full travel), its cooldown (2 s instead of
// 4), its speed (a faster rate), its use (heal 1.2).
#pragma once

#include <deque>

#include "as3/ToolState.h"
#include "engine/cocos/CCLayer.h"

namespace mt {

class Injection : public ToolState {
public:
    Injection();  // 0x6c4d0
    ~Injection() override;

    void begin(const cocos2d::CCPoint& touch) override;     // vf2 0x6cbe8
    void moved(const cocos2d::CCPoint& touch) override;     // vf3 0x6ccd4: the plunger
    void end(const cocos2d::CCPoint&) override { pulling = false; }  // vf4 0x6db2c: stays in
    void frame(float dt) override;                          // vf6 0x6db34: the plunger and fluid drawn
    void setPosition(const cocos2d::CCPoint& p) override;   // vf7 0x6db3c: not once stuck in
    void misuse(const cocos2d::CCPoint& touch) override;    // vf8 0x6db4c
    void start() override;                                  // vf9 0x6c8d8
    void stop() override;                                   // vf10 0x6cad0
    void setEfficiency(float) override {}                   // vf11 0x6c8d4

    // the fluid's colour (0x6cb00): what it extracts (0x79afc by class)
    void setFluidColour(const cocos2d::ccColor3B& c);
    // a touch on its plunger (0x6cb28)
    bool onPlunger(const cocos2d::CCPoint& touch);
    // the extraction fluid's colour for an injury class (0x79afc)
    static cocos2d::ccColor3B colourForClass(int classIndex);

    int mode = 1;            // +0x520: 1 in (heal), 0 out (extract)
    bool planted = false;    // +0x524: stuck in, from its start to its stop
    bool pulling = false;    // +0x525: its plunger held
    float fill = 0;          // +0x528: how far the plunger went (0..1)
    // (1.45's constructor 0x8e230; 1.02's: 0.5, 0.8, 0.6)
    float window = 0.25f;    // +0x52c: the time the rate adds up
    float maxRate = 0.7f;    // +0x530: plunger travel a window at most
    float healRate = 0.85f;  // +0x534: health a full push (x25)
    float travel = 0.8f;     // +0x538: of its full length the plunger goes

private:
    void drawPlunger();  // vf6's and vf9's drawing
    // the "failed" popup (tag popup kind 3 too fast, 2 the wrong way) and
    // the damage (0x6d168)
    void failed(int damage, bool tooFast);
    void push(float amount);    // 0x74b74: into the recent pushes
    float recent() const;       // 0x6c430: the pushes of the last window
    static double now();

    struct Push {  // 0xc bytes (+0x540, 16 of them)
        float amount;
        double time;
    };
    std::deque<Push> m_pushes;
    float m_plungerOffset = 0;  // +0x53c
    float m_fluidHeight = 0;    // +0x60c
    float m_fluidWidth = 0;     // +0x610
    cocos2d::CCLayerColor* m_fluid = nullptr;  // +0x618
    cocos2d::CCSprite* m_mask = nullptr;       // +0x61c (made, never shown)
    cocos2d::CCSprite* m_plastic = nullptr;    // +0x620
    cocos2d::CCSprite* m_plunger = nullptr;    // +0x624
    cocos2d::CCSprite* m_tip = nullptr;        // +0x628
};

}  // namespace mt
