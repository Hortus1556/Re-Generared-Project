#include "as2/menus/Panel.h"

#include <cstdio>

#include "as2/Connector.h"
#include "as2/GameController.h"
#include "as2/GameData.h"
#include "as2/GameState.h"
#include "as2/MenuManager.h"
#include "as2/MyOpenAL.h"
#include "as2/injuries/InjuryGraphic.h"
#include "as2/menus/Hud.h"
#include "as2/tools/ITool.h"
#include "as2/tools/Stitch.h"
#include "platform/Platform.h"

namespace as2 {

namespace {

int missionId() { return Connector::sharedConnector().current_mission_id; }

const char* const kOutlets[10] = {"mBtnCutter", "mBtnTongs",     "mBtnLighter", "mBtnCream",   "mBtnStitch",
                                  "mBtnVacuum", "mBtnChainsaw", "mBtnInjection", "mBtnBattery", "mBtnScanner"};

}  // namespace

Panel::~Panel() { releaseRef(mPopup); }

void Panel::connectOutlet(const std::string& name, ui::View* v) {
    for (int i = 0; i < 10; ++i)
        if (name == kOutlets[i]) mBtn[i] = dynamic_cast<ui::Button*>(v);
    if (name == "mBtnPause") mBtnPause = dynamic_cast<ui::Button*>(v);
    else if (name == "mImgViewTutPanel") mImgViewTutPanel = dynamic_cast<ui::ImageView*>(v);
    else if (name == "mTextViewTutorial") mTextViewTutorial = dynamic_cast<ui::TextView*>(v);
}

void Panel::viewDidLoad() {
    // The tools and pause are the Flash HUD's (Hud): the nib's buttons stay
    // hidden.
    for (ui::Button* b : mBtn)
        if (b) b->hidden = true;
    if (mBtnPause) mBtnPause->hidden = true;
    if (mTextViewTutorial) {
        mTextViewTutorial->fontName = "MarkerFelt-Thin";
        mTextViewTutorial->fontSize = 28;
    }
    // The original turned the view a quarter (its portrait window); the
    // port's window is landscape already.
}

void Panel::setupButtons() {
    std::string offered;
    for (ITool* t : gameState->_tools) offered += " " + t->toolData->name;
    platform::debugTrace("tools%s", offered.c_str());
}

void Panel::action(const std::string& selector, ui::View* sender) {
    if (selector == "Pause:") Pause();
    else if (selector == "SelectButton:") SelectButton(sender);
}

void Panel::Pause() {
    gameState->paused = true;
    // (The CFO pack's own pause, the first pack's the Flash AS2's.)
    const bool cfo = missionId() >= 20;
    MenuManager::sharedMenuManager().setupNewViewController(cfo ? "CFOPauseMenu" : "PauseMenu",
                                                            cfo ? "CFOPauseMenu" : "PauseMenu");
}

void Panel::SelectButton(ui::View* sender) { selectTool(sender->tag, true); }

void Panel::selectTool(int index, bool clicked) {
    if (clicked) MyOpenAL::sharedMyOpenAL().playSFXWithKey("SELECT");
    if (index < 0 || index > 9) return;
    ITool* current = gameState->currentTool;
    if (current && current->isTurnedOn()) current->turnOff(true);
    for (ITool* t : gameState->_tools) {
        if (t->toolData->mId != index + 1) continue;
        if (t == current) {
            // The one in hand: put down.
            gameState->currentTool = nullptr;
            platform::debugTrace("tool none");
            return;
        }
        gameState->currentTool = t;
        platform::debugTrace("tool %s", t->toolData->name.c_str());
        gameState->checkTutorial(5, t->toolData->mId);
        return;
    }
}

void Panel::toolKey(int n) {
    // (As Flash's: the key picks the tool, or puts it down if in hand.)
    if (!mPopup) selectTool(n - 1, false);
}

void Panel::pauseKey() {
    if (!mPopup) Pause();
}

void Panel::addPopupView(const std::string& name) {
    ui::ViewController* vc = MenuManager::make(name, name);
    if (!vc) return;
    view()->addSubview(vc->view());
    removePopupView();
    mPopup = vc;
    ITool* t = gameState->currentTool;
    if (t && t->isTurnedOn()) t->turnOff(false);
}

void Panel::removePopupView() {
    if (!mPopup) return;
    mPopup->view()->removeFromSuperview();
    releaseRef(mPopup);
    mPopup = nullptr;
}

void Panel::KeepInBounds() {
    GameState& gs = *gameState;
    float s = gs.scale;
    if (s * -240 + gs._xScroll + 240 > 0) gs._xScroll = static_cast<float>(static_cast<int>(s * 240 - 240));
    if (s * -160 + gs._yScroll + 160 > 0) gs._yScroll = static_cast<float>(static_cast<int>(s * 160 - 160));
    if (s * 240 + gs._xScroll + 240 < 480) gs._xScroll = static_cast<float>(static_cast<int>(s * -240 + 240));
    if (s * 160 + gs._yScroll + 160 < 320) gs._yScroll = static_cast<float>(static_cast<int>(s * -160 + 160));
}

void Panel::touchesBegan(const ui::Touches& touches) {
    if (mPopup || touches.size() != 1) return;
    const ui::Touch& t = touches[0];
    Point p = t.locationInView(view());
    // On the HUD: a tool card picks its tool (or puts it down), the pause
    // button pauses.
    if (int tool = Hud::shared().toolAt(p)) {
        selectTool(tool - 1, true);
        return;
    }
    if (Hud::shared().pauseAt(p)) {
        Pause();
        return;
    }
    gameState->setMouseXY(static_cast<float>(static_cast<int>(p.x * 0.5f)), static_cast<float>(static_cast<int>(p.y * 0.5f)));
    gameState->checkBleedFingerFix();
    ITool* tool = gameState->currentTool;
    if (!tool || tool->turnedOn) return;
    gameState->newTouch = true;
    tool->turnOn();
    mToolHash = t.id;
    // The needle starts a new thread where the finger is.
    if (Stitch* st = dynamic_cast<Stitch*>(tool))
        if (missionId() != 19) {
            st->mStitchCount = 0;
            st->mStitches[0] = p;
        }
}

void Panel::touchesMoved(const ui::Touches& touches) {
    if (mPopup) return;
    if (touches.size() != 1) {
        // Two fingers zoomed (HandleTouchPinch:) and put the tool down; a
        // mouse has one.
        ITool* tool = gameState->currentTool;
        if (tool && tool->turnedOn) tool->turnOff(false);
        return;
    }
    const ui::Touch& t = touches[0];
    ITool* tool = gameState->currentTool;
    if (!tool) {
        // No tool: the finger drags the (zoomed) view.
        Point p = t.locationInView(view()), q = t.previousLocationInView(view());
        gameState->_xScroll = static_cast<float>(static_cast<int>(gameState->_xScroll + (p.x - q.x)));
        gameState->_yScroll = static_cast<float>(static_cast<int>(gameState->_yScroll + (p.y - q.y)));
        // The clown (mission 7): a still finger still points.
        if (missionId() == 6 && q.x == gameState->unscaledMouseX && q.y == gameState->unscaledMouseY)
            gameState->setMouseXY(static_cast<float>(static_cast<int>(p.x * 0.5f)),
                                  static_cast<float>(static_cast<int>(p.y * 0.5f)));
        KeepInBounds();
        return;
    }
    Point p = t.locationInView(view());
    if (t.id == mToolHash)
        gameState->setMouseXY(static_cast<float>(static_cast<int>(p.x * 0.5f)), static_cast<float>(static_cast<int>(p.y * 0.5f)));
    if (tool->isTurnedOn())
        if (Stitch* st = dynamic_cast<Stitch*>(tool))
            if (missionId() != 19) {
                if (++st->mStitchCount > 2047) st->mStitchCount = 0;
                st->mStitches[st->mStitchCount] = p;
            }
}

void Panel::touchesEnded(const ui::Touches& touches) {
    ITool* tool = gameState->currentTool;
    if (!tool || !tool->isTurnedOn()) return;
    for (const ui::Touch& t : touches)
        if (t.id == mToolHash) tool->turnOff(false);
    InjuryGraphic::traceWorked();
}

}  // namespace as2
