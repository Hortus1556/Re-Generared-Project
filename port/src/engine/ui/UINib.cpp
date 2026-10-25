// Nibs: the views of a package's ui/<name>.json (port/tools/nib2json.py).
#include <map>

#include "core/Json.h"
#include "engine/ui/UIKit.h"
#include "platform/Assets.h"
#include "platform/Platform.h"

namespace ui {
namespace {

std::map<std::string, std::function<View*()>>& registry() {
    static std::map<std::string, std::function<View*()>> r;
    return r;
}

Color color(const json::Value& v, Color fallback) {
    if (v.type != json::Value::kArray || v.size() < 4) return fallback;
    return {static_cast<float>(v[0].num()), static_cast<float>(v[1].num()), static_cast<float>(v[2].num()),
            static_cast<float>(v[3].num())};
}

int stateOf(const std::string& name) {
    if (name == "highlighted") return Button::kHighlighted;
    if (name == "disabled") return Button::kDisabled;
    if (name == "selected") return Button::kSelected;
    return Button::kNormal;
}

View* make(const json::Value& v, std::map<int, View*>& byId) {
    const std::string& cls = v["class"].str();
    const std::string& base = v["uiClass"].str();
    View* view = nullptr;
    auto it = registry().find(cls);
    if (it != registry().end()) view = it->second();
    else if (cls == "EAGLView") view = new GLView;
    else if (base == "UIButton") view = new Button;
    else if (base == "UIImageView") view = new ImageView;
    else if (base == "UITextView" || base == "UILabel") view = new TextView;
    else view = new View;
    view->className = cls;
    const json::Value& f = v["frame"];
    if (f.size() == 4)
        view->setFrame({{static_cast<float>(f[0].num()), static_cast<float>(f[1].num())},
                        {static_cast<float>(f[2].num()), static_cast<float>(f[3].num())}});
    view->tag = v["tag"].integer(0);
    view->alpha = static_cast<float>(v["alpha"].num(1));
    view->hidden = v["hidden"].flag(false);
    view->userInteractionEnabled = v["userInteraction"].flag(true);
    view->contentMode = v["contentMode"].integer(kScaleToFill);
    view->clipsToBounds = v["clipsSubviews"].flag(false);
    if (v.has("backgroundColor") && v["backgroundColor"].type == json::Value::kArray)
        view->setBackgroundColor(color(v["backgroundColor"], {}));

    if (ImageView* iv = dynamic_cast<ImageView*>(view)) {
        if (v["image"].type == json::Value::kString) {
            Image* img = Image::named(v["image"].str());
            iv->setImage(img);
            releaseRef(img);
        }
    } else if (Button* b = dynamic_cast<Button*>(view)) {
        for (const auto& kv : v["states"].members) {
            int st = stateOf(kv.first);
            if (kv.second.has("image")) {
                Image* img = Image::named(kv.second["image"].str());
                b->setImage(img, st);
                releaseRef(img);
            }
            if (kv.second.has("background")) {
                Image* img = Image::named(kv.second["background"].str());
                b->setBackgroundImage(img, st);
                releaseRef(img);
            }
            if (kv.second.has("title")) b->setTitle(kv.second["title"].str(), st);
        }
        b->enabled = !v["disabled"].flag(false);
        b->selected = v["selected"].flag(false);
        b->adjustsImageWhenHighlighted = v["adjustsImageWhenHighlighted"].flag(true);
        b->adjustsImageWhenDisabled = v["adjustsImageWhenDisabled"].flag(true);
    } else if (TextView* t = dynamic_cast<TextView*>(view)) {
        t->text = v["text"].str();
        if (v["font"].size() == 2) {
            t->fontName = v["font"][0].str();
            t->fontSize = static_cast<float>(v["font"][1].num(17));
        }
        t->textColor = color(v["textColor"], t->textColor);
        t->textAlignment = v["textAlignment"].integer(0);
        t->inset = base == "UILabel" ? 0 : 8;
    }
    byId[v["id"].integer(-1)] = view;
    for (const json::Value& s : v["subviews"].items) {
        View* sub = make(s, byId);
        view->addSubview(sub);
        releaseRef(sub);
    }
    return view;
}

}  // namespace

void registerViewClass(const std::string& name, std::function<View*()> make) { registry()[name] = std::move(make); }

bool loadNib(const std::string& name, Nib& out) {
    std::string text;
    if (!assets::readText("ui/" + name + ".json", text)) return false;
    json::Value doc;
    if (!json::parse(text, doc) || !doc.has("views")) return false;
    std::map<int, View*> byId;
    for (const json::Value& v : doc["views"].items) out.views.push_back(make(v, byId));
    auto find = [&](const json::Value& ref) -> View* {
        if (ref.type != json::Value::kNumber) return nullptr;
        auto it = byId.find(ref.integer());
        return it == byId.end() ? nullptr : it->second;
    };
    for (const json::Value& o : doc["outlets"].items) {
        if (o["source"].str() != "owner") continue;
        if (View* v = find(o["destination"])) out.outlets[o["name"].str()] = v;
    }
    for (const json::Value& a : doc["actions"].items) {
        if (a["destination"].str() != "owner") continue;
        if (View* v = find(a["source"])) out.actions.push_back({v, a["selector"].str(), a["events"].integer(64)});
    }
    return true;
}

}  // namespace ui
