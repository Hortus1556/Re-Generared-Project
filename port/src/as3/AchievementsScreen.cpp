#include "as3/AchievementsScreen.h"

#include <algorithm>
#include <string>

#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/RestoredContent.h"
#include "as3/SaveData.h"
#include "as3/SceneMgr.h"
#include "as3/SoundMgr.h"
#include "as3/WelcomeScreen.h"
#include "engine/cocos/CCDirector.h"
#include "engine/cocos/CCScheduler.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

const char* const kSheets[] = {"upgrade_screen_sheet", "preop_common"};
// the board: 5 columns of frames, 6 rows (the layout's row 1 at the top)
constexpr int kColumns = 5, kRows = 6;

// the tech tree's column colours, as the layout names its icons (UpgradeScreen.cpp)
const char* columnColour(int column) {
    switch (column) {
        case 2: return "orange";
        case 3: return "red";
        case 4: return "green";
        case 5: return "purple";
        default: return "blue";
    }
}

std::string frameName(int row, int column) {
    return "upgrades_frame_row_" + std::to_string(row) + "_col_" + std::to_string(column);
}
// the layout's icon in that frame (its icons are named by upgrade row, 1 at the bottom)
std::string iconName(int row, int column) {
    return std::string("upgrades_") + columnColour(column) + "icons0" + std::to_string(kRows + 1 - row);
}

CCSpriteFrame* frameOf(const std::string& name) {
    return CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(name.c_str());
}

// An achievement's icon: its "-normal"/"-on" frame earned, its "-grey"/"-off"
// twin not (or the same picture darkened), fitted into `box` (points).
CCSprite* iconSprite(const AchievementDef& a, bool earned, const CCSize& box) {
    CCSpriteFrame* f = nullptr;
    bool greyed = false;
    if (!earned) {
        for (const char* s : {"-grey.png", "-off.png"})
            if (!f) f = frameOf(a.icon + s);
        greyed = f == nullptr;
    }
    for (const char* s : {"-normal.png", "-on.png", ".png"})
        if (!f) f = frameOf(a.icon + s);
    if (!f) return nullptr;
    CCSprite* sp = CCSprite::createWithSpriteFrame(f);
    if (greyed) sp->setColor(ccc3(80, 80, 80));
    const CCSize sz = sp->getContentSize();
    if (sz.width > 0 && sz.height > 0 && box.width > 0)
        sp->setScale(std::min(box.width / sz.width, box.height / sz.height));
    return sp;
}

void hide(UIInstance& ui, const std::string& name) { ui.setVisible(name, false); }

}  // namespace

AchievementsScreen* AchievementsScreen::create() {
    AchievementsScreen* s = new AchievementsScreen;
    s->setName("AchievementsScreen");
    if (s->init()) {
        s->autorelease();
        return s;
    }
    delete s;
    return nullptr;
}

void AchievementsScreen::load() {
    SoundMgr* sound = SoundMgr::sharedSoundMgr();
    sound->stopMusic(true);
    sound->playAmbience("tech_tree_ambience");
    StatsMgr* stats = StatsMgr::sharedInstance();
    stats->catchUp();
    loadSheets(this, {kSheets[0], kSheets[1]});
    if (CCSprite* bg = CCSprite::createWithSpriteFrameName("upgrades_background.png")) {
        setPositionOnCanvas(bg, 320, 192);
        addChild(bg);
    }
    // the Upgrades Machine's board, emptied of the upgrades
    m_def = loadUIDefinition("common/ui_techtree.json");
    buildUI(ui, this, m_def, this, menu_selector(AchievementsScreen::onButton));
    for (int r = 1; r <= kRows; ++r) {
        hide(ui, "upgrades_chain_row_" + std::to_string(r));
        ui.setText("ui_upgrades_starcost_row_" + std::to_string(r), "");
        for (int c = 1; c <= kColumns; ++c) {
            const std::string at = "_row_" + std::to_string(r) + "_col_" + std::to_string(c);
            hide(ui, iconName(r, c));
            hide(ui, "upgrades_starbox" + at);
            ui.setText("ui_upgrades_starcost" + at, "");
            hide(ui, frameName(r, c));
        }
    }
    for (const char* n : {"ui_button_buy", "upgrades_coin", "upgrades_tickbox", "upgrades_screenicon", "upgrades_blueframe"})
        hide(ui, n);
    // a tile a achievement, left to right, top to bottom
    const std::vector<AchievementDef>& defs = stats->achievements();
    m_tiles = CCMenu::create();
    m_tiles->setPosition(CCPointZero);
    for (size_t i = 0; i < defs.size() && i < static_cast<size_t>(kColumns * (kRows - 1)); ++i) {
        const int row = static_cast<int>(i) / kColumns + 1, column = static_cast<int>(i) % kColumns + 1;
        const bool earned = stats->earned(defs[i]);
        CCNode* frame = ui.node(frameName(row, column));
        if (frame) {
            frame->setVisible(true);
            ui.setItemImage(m_def, frameName(row, column), earned ? "gold" : "normal");
        }
        m_frames.push_back(frame);
        CCNode* place = ui.node(iconName(row, column));
        if (!place) continue;
        const CCSize box = place->getContentSize();
        CCSize inner = CCSizeMake(box.width * 0.8f, box.height * 0.8f);
        CCSprite* normal = iconSprite(defs[i], earned, inner);
        CCSprite* pressed = iconSprite(defs[i], earned, inner);
        if (!normal || !pressed) continue;
        pressed->setColor(ccc3(170, 170, 170));
        // each centred in the tile's box
        CCNode* n = CCNode::create();
        n->setContentSize(box);
        normal->setPosition(ccp(box.width * 0.5f, box.height * 0.5f));
        n->addChild(normal);
        CCNode* p = CCNode::create();
        p->setContentSize(box);
        pressed->setPosition(ccp(box.width * 0.5f, box.height * 0.5f));
        p->addChild(pressed);
        CCMenuItemSprite* tile = CCMenuItemSprite::create(n, p, this, menu_selector(AchievementsScreen::onTile));
        tile->setTag(static_cast<int>(i));
        tile->setAnchorPoint(place->getAnchorPoint());
        tile->setPosition(place->getParent() ? place->getParent()->convertToWorldSpace(place->getPosition())
                                             : place->getPosition());
        m_tiles->addChild(tile);
    }
    addChild(m_tiles, 30);
    // the machine's screen: nothing picked yet
    ui.setText("upgrades_iconstitle", "ACHIEVEMENTS");
    ui.setText("upgrades_iconstext", std::to_string(stats->earnedCount()) + " of " + std::to_string(defs.size()) +
                                         " earned. Select one to see what it takes.");
    ui.setText("upgrades_starsprice", "");
    // the Director's cut, under the board
    refreshDirectorsCut();
    log("achievements: %d of %d earned", stats->earnedCount(), static_cast<int>(defs.size()));
}

void AchievementsScreen::unload() {}

void AchievementsScreen::refreshDirectorsCut() {
    if (m_cut) {
        m_cut->removeFromParent();
        m_cut = nullptr;
    }
    const bool open = restored::directorsCutUnlocked() || restored::directorsCut();
    std::string text = open ? std::string("DIRECTOR'S CUT: ") + (restored::directorsCut() ? "ON" : "OFF")
                            : "DIRECTOR'S CUT: earn " + std::to_string(restored::kDirectorsCutAchievements) +
                                  " achievements to open it";
    CCLabelBMFont* normal = createLabel(text.c_str(), "font-Upgrades02", 0, kCCTextAlignmentCenter);
    CCLabelBMFont* pressed = createLabel(text.c_str(), "font-Upgrades02", 0, kCCTextAlignmentCenter);
    if (!normal || !pressed) return;
    if (!open) normal->setColor(ccc3(150, 150, 150));
    pressed->setColor(ccc3(255, 220, 120));
    m_cut = CCMenuItemSprite::create(normal, pressed, this, menu_selector(AchievementsScreen::onDirectorsCut));
    m_cut->setEnabled(open);
    m_cut->setAnchorPoint(ccp(0.5f, 0.5f));
    // the board's free bottom row
    CCNode* left = ui.node(iconName(kRows, 1));
    CCNode* right = ui.node(iconName(kRows, kColumns));
    if (left && right) {
        CCPoint a = left->getParent()->convertToWorldSpace(left->getPosition());
        CCPoint b = right->getParent()->convertToWorldSpace(right->getPosition());
        const CCSize box = right->getContentSize();
        m_cut->setPosition(ccp((a.x + b.x + box.width) * 0.5f, a.y + box.height * 0.5f));
    }
    m_tiles->addChild(m_cut);
}

void AchievementsScreen::onButton(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    std::string name = ui.nameOf(m_def, sender);
    if (name == "ui_button_backsmall") {
        SoundMgr::sharedSoundMgr()->playSound("button_click", false);
        SceneMgr::sharedSceneMgr()->replaceScene(WelcomeScreen::create());
    }
}

void AchievementsScreen::onTile(CCObject* sender) {
    if (shownForLessThanMinimum()) return;
    CCNode* n = dynamic_cast<CCNode*>(sender);
    if (!n) return;
    SoundMgr::sharedSoundMgr()->playSound("button_click", false);
    select(n->getTag());
}

void AchievementsScreen::onDirectorsCut(CCObject*) {
    if (shownForLessThanMinimum()) return;
    SoundMgr::sharedSoundMgr()->playSound(restored::directorsCut() ? "button_click" : "upgrade_confirm", false);
    restored::setDirectorsCut(!restored::directorsCut());
    refreshDirectorsCut();
}

void AchievementsScreen::select(int index) {
    StatsMgr* stats = StatsMgr::sharedInstance();
    const std::vector<AchievementDef>& defs = stats->achievements();
    if (index < 0 || index >= static_cast<int>(defs.size())) return;
    m_selected = index;
    const AchievementDef& a = defs[index];
    const bool earned = stats->earned(a);
    const int count = std::min(stats->getStatValue(static_cast<StatTypes::Enum>(a.statType)), a.countToUnlock);
    // the tile picked outlined, as the machine's upgrades are (UpgradeScreen::select)
    CCNode* blue = ui.node("upgrades_blueframe");
    CCNode* place = ui.node(iconName(index / kColumns + 1, index % kColumns + 1));
    if (blue && place) {
        blue->setVisible(true);
        blue->setPosition(ccp(place->getPositionX() - 2.4f, place->getPositionY() + 0.7f));
    }
    ui.setText("upgrades_iconstitle", a.title);
    ui.setText("upgrades_iconstext", a.description);
    ui.setText("upgrades_starsprice", earned ? "" : std::to_string(count) + " / " + std::to_string(a.countToUnlock));
    ui.setVisible("upgrades_tickbox", earned);
    // its picture on the machine's screen
    if (m_bigIcon) {
        m_bigIcon->removeFromParent();
        m_bigIcon = nullptr;
    }
    // (the layout's picture has no frame of its own: the size of the
    // machine's pictures, upgrades_screenicons-*)
    CCNode* screen = ui.node("upgrades_screenicon");
    CCSpriteFrame* sized = frameOf("upgrades_screenicons-blue01.png");
    if (screen && sized) {
        const CCSize box = sized->getOriginalSize();
        m_bigIcon = iconSprite(a, earned, CCSizeMake(box.width * 0.7f, box.height * 0.7f));
        if (m_bigIcon) {
            CCPoint at = screen->getParent()->convertToWorldSpace(screen->getPosition());
            const CCPoint anchor = screen->getAnchorPoint();
            m_bigIcon->setPosition(ccp(at.x + box.width * (0.5f - anchor.x), at.y + box.height * (0.5f - anchor.y)));
            addChild(m_bigIcon, 31);
        }
    }
    log("achievements: %s %s %d/%d", a.id.c_str(), earned ? "earned" : "not yet", count, a.countToUnlock);
}

// ---------------------------------------------------------------- the banner

namespace {
constexpr float kSlide = 0.35f, kHold = 2.6f;
}

AchievementPopup* AchievementPopup::shared() {
    static AchievementPopup* p = [] {
        AchievementPopup* n = new AchievementPopup;
        n->init();
        CCDirector::sharedDirector()->getScheduler()->scheduleSelector(schedule_selector(AchievementPopup::tick), n,
                                                                       0, false);
        return n;
    }();
    return p;
}

void AchievementPopup::show(const AchievementDef& a) {
    AchievementPopup* p = shared();
    p->m_queue.push_back(a);
    if (p->m_time < 0) p->startNext();
}

void AchievementPopup::startNext() {
    if (m_queue.empty()) return;
    CCDirector* director = CCDirector::sharedDirector();
    // (a movie playing has the notification node: wait for it)
    if (director->getNotificationNode() && director->getNotificationNode() != this) return;
    const AchievementDef a = m_queue.front();
    m_queue.pop_front();
    // the top bar's plate (global_ui_sheet: its frames read here if no
    // screen has them; the sprites keep the texture while they show)
    ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    CCSpriteFrameCache* cache = CCSpriteFrameCache::sharedSpriteFrameCache();
    if (!cache->spriteFrameByName("ui_global_totals_background.png"))
        cache->addSpriteFramesWithFile(r->artPath("global_ui_sheet", "plist").c_str());
    m_banner = CCNode::create();
    CCSize size = CCSizeMake(300, 52);
    if (CCSprite* plate = CCSprite::createWithSpriteFrameName("ui_global_totals_background.png")) {
        const CCSize ps = plate->getContentSize();
        plate->setScaleX(size.width / ps.width);
        plate->setScaleY(size.height / ps.height);
        m_banner->addChild(plate);
    }
    if (CCSprite* star = CCSprite::createWithSpriteFrameName("ui_global_totalstar.png")) {
        const CCSize ss = star->getContentSize();
        star->setScale(size.height * 0.62f / std::max(1.0f, ss.height));
        star->setPosition(ccp(-size.width * 0.5f + size.height * 0.5f + 4, 0));
        m_banner->addChild(star);
    }
    const float textX = size.height * 0.5f;
    if (CCLabelBMFont* head = createLabel("ACHIEVEMENT EARNED", "font-UIGlobalsSmall", 0, kCCTextAlignmentCenter)) {
        head->setScale(0.75f);
        head->setPosition(ccp(textX * 0.5f, size.height * 0.2f));
        m_banner->addChild(head);
    }
    if (CCLabelBMFont* title = createLabel(a.title.c_str(), "font-UIGlobals", 0, kCCTextAlignmentCenter)) {
        const float w = title->getContentSize().width;
        const float room = size.width - size.height - 16;
        if (w > room) title->setScale(room / w);
        title->setPosition(ccp(textX * 0.5f, -size.height * 0.17f));
        m_banner->addChild(title);
    }
    m_banner->setContentSize(size);
    addChild(m_banner);
    const CCSize win = director->getWinSize();
    m_banner->setPosition(ccp(win.width * 0.5f, win.height + size.height));
    director->setNotificationNode(this);
    m_time = 0;
    SoundMgr::sharedSoundMgr()->playSound("upgrade_confirm", false);
    log("achievement banner %s \"%s\"", a.id.c_str(), a.title.c_str());
}

void AchievementPopup::tick(float dt) {
    CCDirector* director = CCDirector::sharedDirector();
    if (m_time < 0) {
        if (!m_queue.empty()) startNext();
        return;
    }
    // (a movie took the notification node meanwhile: the banner gives way)
    if (director->getNotificationNode() != this) {
        if (!director->getNotificationNode()) director->setNotificationNode(this);
        else return;
    }
    m_time += dt;
    const CCSize win = director->getWinSize();
    const float h = m_banner ? m_banner->getContentSize().height : 0;
    const float shown = win.height - h * 0.5f - 3, hidden = win.height + h;
    float y = shown;
    if (m_time < kSlide) {
        const float t = m_time / kSlide;
        y = hidden + (shown - hidden) * (1 - (1 - t) * (1 - t));  // eased out
    } else if (m_time > kSlide + kHold) {
        const float t = std::min(1.0f, (m_time - kSlide - kHold) / kSlide);
        y = shown + (hidden - shown) * t * t;  // eased in
        if (t >= 1) return finish();
    }
    // (across the middle of the screen as it is now: it may change size under
    // the banner, at a screen change)
    if (m_banner) m_banner->setPosition(ccp(win.width * 0.5f, y));
}

void AchievementPopup::finish() {
    if (m_banner) m_banner->removeFromParent();
    m_banner = nullptr;
    m_time = -1;
    CCDirector* director = CCDirector::sharedDirector();
    if (director->getNotificationNode() == this) director->setNotificationNode(nullptr);
    startNext();
}

}  // namespace mt
