#include "as3/UI.h"

#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "core/Json.h"
#include "engine/cocos/CCFileUtils.h"
#include "engine/cocos/CCSprite.h"
#include "engine/cocos/CCSpriteFrameCache.h"

namespace mt {

using namespace cocos2d;

namespace {

int lookup(const std::map<std::string, int>& m, const std::string& key, int fallback) {
    auto it = m.find(key);
    return it == m.end() ? fallback : it->second;
}

const std::map<std::string, int> kTypes = {
    {"button", kUIButton}, {"toggle", kUIToggle}, {"submenu", kUISubmenu}, {"front", kUIFront}, {"other", kUIOther}};
const std::map<std::string, int> kModes = {{"Absolute", kUIAbsolute}, {"TopLeft", kUITopLeft},     {"Top", kUITop},
                                           {"TopRight", kUITopRight}, {"Right", kUIRight},         {"BottomRight", kUIBottomRight},
                                           {"Bottom", kUIBottom},     {"BottomLeft", kUIBottomLeft}, {"Left", kUILeft}};
const std::map<std::string, int> kAlignH = {{"left", 0}, {"centre", 1}, {"right", 2}};
const std::map<std::string, int> kAlignV = {{"bottom", 0}, {"middle", 1}, {"top", 2}};
const float kAlign[3] = {0.0f, 0.5f, 1.0f};  // 0x3f2a6c

// Json::Value::asInt / asFloat as the original read them
int asInt(const json::Value& v) { return v.type == json::Value::kNumber ? static_cast<int>(v.number) : 0; }

}  // namespace

const UITextDef* UIDefinition::text(const std::string& id) const {
    for (const UITextDef& t : texts)
        if (t.textId == id) return &t;
    return nullptr;
}

UIDefinition loadUIDefinition(const char* path) {
    UIDefinition def;
    std::string text;
    json::Value root;
    if (!CCFileUtils::sharedFileUtils()->readText(path, text) || !json::parse(text, root)) {
        logError("Cannot read layout %s", path);
        return def;
    }
    def.name = root["name"].str();
    const json::Value& items = root["items"];
    if (items.size() <= 192) {
        for (const json::Value& it : items.items) {
            UIItemDef d;
            d.name = it["name"].str();
            d.spritePrefix = it["spritePrefix"].str();
            d.type = lookup(kTypes, it["type"].str(), kUIOther);
            d.x = asInt(it["x"]);
            d.y = asInt(it["y"]);
            d.mode = lookup(kModes, it["mode"].str(), kUIAbsolute);
            def.items.push_back(d);
        }
        const json::Value& texts = root["texts"];
        if (texts.size() <= 64) {
            for (const json::Value& t : texts.items) {
                UITextDef d;
                d.textId = t["textId"].str();
                d.fontName = t["fontName"].str();
                std::string h = t["alignH"].str(), v = t["alignV"].str();
                if (!h.empty()) {
                    if (!kAlignH.count(h)) logError("Invalid horizontal align in text %s: %s", d.textId.c_str(), h.c_str());
                    d.alignH = lookup(kAlignH, h, 0);
                }
                if (!v.empty()) {
                    if (!kAlignV.count(v)) logError("Invalid vertical align in text %s: %s", d.textId.c_str(), v.c_str());
                    d.alignV = lookup(kAlignV, v, 0);
                }
                const json::Value& c = t["colour"];
                for (size_t k = 0; k < c.size() && k < 4; ++k) d.colour[k] = static_cast<unsigned char>(asInt(c[k]));
                d.rotation = static_cast<float>(asInt(t["rotationInDegrees"]));
                d.layoutMode = lookup(kModes, t["layoutMode"].str(), kUIAbsolute);
                const json::Value& r = t["rect"];
                d.x = static_cast<float>(asInt(r["x"]));
                d.y = static_cast<float>(asInt(r["y"]));
                d.width = static_cast<float>(asInt(r["width"]));
                d.height = static_cast<float>(asInt(r["height"]));
                def.texts.push_back(d);
            }
        }
    }
    return def;
}

CCPoint anchorOnCanvas(int mode, float x, float y) {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    // the edges of the part of the canvas the screen shows
    float left = (640.0f - r->width / r->canvasScale) * 0.5f;
    float bottom = (384.0f - r->height / r->canvasScale) * 0.5f;
    float right = 640.0f - left, top = 384.0f - bottom;
    const CCPoint offsets[9] = {ccp(0, 0),      ccp(left, top), ccp(0, top),       ccp(right, top), ccp(right, 0),
                                ccp(right, bottom), ccp(0, bottom), ccp(left, bottom), ccp(left, 0)};
    CCPoint o = mode >= 0 && mode < 9 ? offsets[mode] : ccp(0, 0);
    return ccp(o.x + x, o.y + y);
}

CCLabelBMFont* createLabel(const char* text, const char* font, float width, CCTextAlignment alignment) {
    ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    std::string fnt = r->artPath(font, "fnt");
    return CCLabelBMFont::create(text, fnt.c_str(), width * r->canvasScale, alignment);
}

CCLabelBMFont* createTextLabel(const UITextDef& t) {
    std::string font = "font-" + t.fontName;
    CCLabelBMFont* label = createLabel("placeholder", font.c_str(), t.width, kCCTextAlignmentLeft);
    if (!label) return nullptr;
    float ax = kAlign[t.alignH < 3 ? t.alignH : 0], ay = kAlign[t.alignV < 3 ? t.alignV : 0];
    label->setAnchorPoint(ccp(ax, ay));
    CCPoint p = anchorOnCanvas(t.layoutMode, t.x, t.y);
    setPositionOnCanvas(label, p.x + t.width * ax, p.y + t.height * ay);
    label->setRotation(-t.rotation);
    label->setColor(ccc3(t.colour[0], t.colour[1], t.colour[2]));
    return label;
}

static CCSpriteFrame* frame(const std::string& name) {
    return CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(name.c_str());
}

void buildUI(UIInstance& ui, CCNode* parent, const UIDefinition& def, CCObject* target, SEL_MenuHandler selector) {
    ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    ui = UIInstance();
    ui.menuA = CCMenu::create();
    ui.menuA->setContentSize(CCSizeMake(r->width, r->height));
    ui.menuA->setAnchorPoint(CCPointZero);
    ui.menuB = CCMenu::create();
    ui.menuB->setContentSize(ui.menuA->getContentSize());
    ui.menuB->setAnchorPoint(ui.menuA->getAnchorPoint());
    ui.menuA->setPosition(CCPointZero);
    ui.menuB->setPosition(CCPointZero);
    for (size_t i = 0; i < def.items.size(); ++i) {
        const UIItemDef& d = def.items[i];
        CCMenuItem* item;
        if (d.type != kUIToggle && d.type < 5) {
            CCMenuItemImage* img = CCMenuItemImage::create();
            CCSpriteFrame* normal = frame(d.spritePrefix + "-normal.png");
            if (!normal) normal = frame(d.spritePrefix + ".png");
            if (normal) img->setNormalSpriteFrame(normal);
            if (d.type == kUIButton || d.type == kUISubmenu) {
                if (CCSpriteFrame* selected = frame(d.spritePrefix + "-selected.png")) img->setSelectedSpriteFrame(selected);
                if (d.type == kUISubmenu) img->setVisible(false);
            }
            item = img;
        } else {
            // a toggle of three items: normal, selected, disabled
            CCMenuItemImage* on = CCMenuItemImage::create();
            on->setNormalSpriteFrame(frame(d.spritePrefix + "-normal.png"));
            on->setSelectedSpriteFrame(frame(d.spritePrefix + "-normal.png"));
            CCMenuItemImage* selected = CCMenuItemImage::create();
            selected->setNormalSpriteFrame(frame(d.spritePrefix + "-selected.png"));
            selected->setSelectedSpriteFrame(frame(d.spritePrefix + "-selected.png"));
            CCMenuItemImage* disabled = CCMenuItemImage::create();
            disabled->setNormalSpriteFrame(frame(d.spritePrefix + "-disabled.png"));
            disabled->setSelectedSpriteFrame(frame(d.spritePrefix + "-disabled.png"));
            item = CCMenuItemToggle::createWithTarget(target, selector, on, selected, disabled, nullptr);
        }
        item->setAnchorPoint(CCPointZero);  // by its bottom-left corner (0x429624)
        CCPoint p = anchorOnCanvas(d.mode, static_cast<float>(d.x), static_cast<float>(d.y));
        setPositionOnCanvas(item, p.x, p.y);
        if (!target || !selector || d.type == kUIFront || d.type == kUIOther) item->setEnabled(false);
        else item->setTarget(target, selector);
        ui.indexOf[item] = static_cast<int>(i);
        ui.nodes.push_back(item);
        ui.byName[d.name] = item;
        (d.type == kUIOther ? ui.menuB : ui.menuA)->addChild(item);
    }
    CCNode* texts = CCNode::create();
    for (size_t j = 0; j < def.texts.size(); ++j) {
        CCLabelBMFont* label = createTextLabel(def.texts[j]);
        if (!label) continue;
        ui.indexOf[label] = static_cast<int>(j);  // (the original: the text's own index)
        ui.nodes.push_back(label);
        ui.byName[def.texts[j].textId] = label;
        texts->addChild(label);
    }
    parent->addChild(ui.menuB);
    parent->addChild(ui.menuA);
    parent->addChild(texts);
    ui.count = static_cast<int>(def.items.size() + def.texts.size());
}

CCNode* UIInstance::node(const std::string& name) {
    auto it = byName.find(name);
    return it == byName.end() ? nullptr : it->second;
}

CCMenuItemImage* UIInstance::item(const std::string& name) { return dynamic_cast<CCMenuItemImage*>(node(name)); }

CCLabelBMFont* UIInstance::label(const std::string& name) { return dynamic_cast<CCLabelBMFont*>(node(name)); }

void UIInstance::setVisible(const std::string& name, bool visible) {
    if (CCNode* n = node(name)) n->setVisible(visible);
}

void UIInstance::setText(const std::string& name, const std::string& text) {
    CCLabelBMFont* l = label(name);
    if (!l) return;
    std::string padded = text;
    int spaces = 0;
    for (unsigned char c : text)
        if (c < 0x20 && spaces < 32) {
            padded += ' ';
            ++spaces;
        }
    l->setString(padded.c_str());
}

void UIInstance::setItemImage(const UIDefinition& def, const std::string& name, const std::string& suffix) {
    CCMenuItemImage* it = item(name);
    if (!it) return;
    auto idx = indexOf.find(it);
    if (idx == indexOf.end()) return;
    std::string f = def.items[idx->second].spritePrefix + "-" + suffix + ".png";
    it->setNormalSpriteFrame(frame(f));
}

std::string UIInstance::nameOf(const UIDefinition& def, CCObject* sender) {
    auto it = indexOf.find(dynamic_cast<CCNode*>(sender));
    if (it == indexOf.end() || it->second >= static_cast<int>(def.items.size())) return "";
    return def.items[it->second].name;
}

void loadSheets(CCNode* scene, const std::vector<std::string>& names) {
    ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    for (const std::string& name : names) {
        std::string texture = r->artPath(name.c_str(), "pvr.ccz");
        if (!CCFileUtils::sharedFileUtils()->isFileExist(texture)) texture = r->artPath(name.c_str(), "png");
        // (the port's order: the frames first, whose sheet gives the
        // texture's size, so it is decoded on worker threads; the original
        // made the batch node first)
        std::string plist = r->artPath(name.c_str(), "plist");
        CCSpriteFrameCache::sharedSpriteFrameCache()->addSpriteFramesWithFile(plist.c_str());
        scene->addChild(CCSpriteBatchNode::create(texture.c_str(), 29));
        // (the port's) the frames the Android build added to this sheet, from
        // their own sheet in another art folder (ResolutionMgr::supplementPath)
        std::string extra = r->supplementPath(name.c_str(), "plist");
        if (!extra.empty() && extra != plist) {
            CCSpriteFrameCache::sharedSpriteFrameCache()->addSpriteFramesWithFile(extra.c_str());
            scene->addChild(CCSpriteBatchNode::create(r->supplementPath(name.c_str(), "png").c_str(), 29));
        }
    }
}

void unloadSheets(const std::vector<std::string>& names) {
    ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    for (const std::string& name : names) {
        CCSpriteFrameCache::sharedSpriteFrameCache()->removeSpriteFramesFromFile(r->artPath(name.c_str(), "plist").c_str());
        CCTextureCache::sharedTextureCache()->removeTextureForKey(r->artPath(name.c_str(), "png").c_str());
        CCTextureCache::sharedTextureCache()->removeTextureForKey(r->artPath(name.c_str(), "pvr.ccz").c_str());
        std::string extra = r->supplementPath(name.c_str(), "plist");
        if (!extra.empty() && extra != r->artPath(name.c_str(), "plist")) {
            CCSpriteFrameCache::sharedSpriteFrameCache()->removeSpriteFramesFromFile(extra.c_str());
            CCTextureCache::sharedTextureCache()->removeTextureForKey(r->supplementPath(name.c_str(), "png").c_str());
        }
    }
}

}  // namespace mt
