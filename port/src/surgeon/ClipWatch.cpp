#include "surgeon/ClipWatch.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "core/Json.h"
#include "platform/Assets.h"
#include "platform/Platform.h"
#include "surgeon/ArtNode.h"

ClipWatch::ClipWatch() {
    if (const char* names = std::getenv("AS_WATCH")) {
        std::istringstream list(names);
        std::string name;
        while (std::getline(list, name, ','))
            if (!name.empty()) prefixes.push_back(name);
        return;
    }
    std::string text;
    json::Value data;
    if (!assets::readText("data/gamedata.json", text) || !json::parse(text, data)) return;
    for (const json::Value& t : data["injuryTypes"].items)
        if (!t["linkage"].str().empty()) prefixes.push_back(t["linkage"].str());
}

// A watched prefix and then only digits (its depth), or the name itself.
bool ClipWatch::watched(const std::string& name) const {
    for (const std::string& p : prefixes) {
        if (name.compare(0, p.size(), p) != 0) continue;
        bool digits = true;
        for (size_t i = p.size(); i < name.size() && digits; ++i)
            digits = std::isdigit(static_cast<unsigned char>(name[i])) != 0;
        if (digits) return true;
    }
    return false;
}

void ClipWatch::update(ArtNode* stage, long pass, float mouseX, float mouseY) {
    if (prefixes.empty() || !stage) return;
    seen.clear();
    for (auto& [depth, child] : stage->children) visit(child, "_root." + child->name, pass, mouseX, mouseY);
    for (auto it = last.begin(); it != last.end();) {
        if (seen.count(it->first)) {
            ++it;
            continue;
        }
        platform::debugTrace("watch pass %ld %s: gone (was %s)", pass, it->first.c_str(), it->second.c_str());
        it = last.erase(it);
    }
}

void ClipWatch::visit(ArtNode* node, const std::string& path, long pass, float mouseX, float mouseY) {
    if (node->removed) return;
    if (watched(node->name)) {
        seen.insert(path);
        float sx = 0, sy = 0;
        node->localToGlobal(sx, sy);
        flash::Rect b = node->bounds();
        char head[256];
        std::snprintf(head, sizeof head, "frame %d %s at (%.1f,%.1f) stage (%.1f,%.1f) a %.0f", node->frame(),
                      node->visible ? "shown" : "hidden", node->x, node->y, sx, sy, node->alpha);
        std::string line = head;
        if (b.empty()) {
            line += " drawn nothing";
        } else {
            char drawn[96];
            std::snprintf(drawn, sizeof drawn, " drawn (%.0f,%.0f)-(%.0f,%.0f)", b.xMin, b.yMin, b.xMax, b.yMax);
            line += drawn;
        }
        // Its inner clips, and those of the clips in it made nodes (a script
        // setting a property on one: AS1's brain holder), by their paths.
        std::vector<VectorArt::ClipVisit> inner;
        node->innerClips(inner);
        std::vector<std::pair<ArtNode*, std::string>> in{{node, std::string()}};
        for (size_t k = 0; k < in.size(); ++k)
            for (auto& [depth, child] : in[k].first->children) {
                if (!child->timeline || child->removed) continue;
                std::string at = in[k].second.empty() ? child->name : in[k].second + "." + child->name;
                inner.push_back(VectorArt::ClipVisit{at, child->symbol(), child->frame(), 0});
                std::vector<VectorArt::ClipVisit> under;
                child->innerClips(under);
                for (VectorArt::ClipVisit& v : under) {
                    v.path = at + "." + v.path;
                    inner.push_back(v);
                }
                in.emplace_back(child, at);
            }
        if (!inner.empty()) {
            line += "; inner";
            for (const VectorArt::ClipVisit& v : inner) {
                if (line.size() > 1700) {
                    line += " ...";
                    break;
                }
                // (Not an unnamed clip two levels in: the pieces of its points.)
                size_t last = v.path.rfind('.');
                if (last != std::string::npos && v.path[last + 1] == '#' && std::count(v.path.begin(), v.path.end(), '.') >= 2)
                    continue;
                line += " " + v.path + "=" + std::to_string(v.frame);
                std::string rest = v.path;
                ArtNode* owner = node->through(rest);
                if (rest.empty() ? !owner->visible : owner->partHidden(rest)) line += "(hidden)";
            }
        }
        std::string& was = last[path];
        if (line != was) {
            platform::debugTrace("watch pass %ld mouse (%.0f,%.0f) %s: %s%s", pass, mouseX, mouseY, path.c_str(),
                                 was.empty() ? "new, " : "", line.c_str());
            was = line;
        }
    }
    for (auto& [depth, child] : node->children) visit(child, path + "." + child->name, pass, mouseX, mouseY);
}
