#include "engine/Spritex.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include "platform/Assets.h"
#include "platform/DrawList.h"
#include "platform/GL.h"
#include "platform/Platform.h"

namespace {

// -[NSString componentsSeparatedByCharactersInSet:] for one separator:
// keeps empty fields.
std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string::size_type begin = 0;
    while (true) {
        std::string::size_type end = s.find(sep, begin);
        out.push_back(s.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
        if (end == std::string::npos) return out;
        begin = end + 1;
    }
}

int intValue(const std::string& s) { return std::atoi(s.c_str()); }

std::vector<int> ints(const std::string& record) {
    std::vector<int> v;
    for (const std::string& f : split(record, 'a')) v.push_back(intValue(f));
    return v;
}

std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : text) {
        if (c == '\n' || c == '\r') {  // newlineCharacterSet: \r\n yields an empty line between
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

double nowMs() { return platform::passTime() * 1000.0; }

}  // namespace

// ------------------------------------------------------ ResourceSpritexData

ResourceSpritexData::ResourceSpritexData(int id, const std::string& filename) : Resource(id, filename, kSpritex) {
    std::string text;
    if (path_.empty() || !assets::readText(filename, text)) return;
    status_ = 1;
    createSpritexData(text);
    // The iPad street scene's data places its lamp light's three pieces
    // where its image no longer has them (one region is empty, the others
    // 15 pixels off), so the light came out as a box over the wall; these
    // are where the pieces are in ipad_sidestreet.png.
    if (filename == "ipad_sidestreet.txt" && pieces.size() > 3) {
        const int at[4][2] = {{0, 0}, {4, 786}, {634, 922}, {340, 785}};
        for (int i = 1; i <= 3; ++i)
            if (pieces[i].size() >= 2) {
                pieces[i][0] = at[i][0];
                pieces[i][1] = at[i][1];
            }
        // The middle piece and the bottom one share a row; drawn twice it
        // showed as a bright line.
        if (pieces[3].size() >= 4) pieces[3][3] -= 1;
    }
}

void ResourceSpritexData::createSpritexData(const std::string& text) {
    std::vector<std::string> l = lines(text);
    while (l.size() < 4) l.emplace_back();

    for (const std::string& rec : split(l[0], 'b')) pieces.push_back(ints(rec));

    for (const std::string& group : split(l[1], 'c')) {
        std::vector<std::vector<int>> frame;
        for (const std::string& rec : split(group, 'b'))
            if (!rec.empty()) frame.push_back(ints(rec));  // an empty group is a blank frame
        frames.push_back(frame);
    }

    for (const std::string& group : split(l[3], 'c')) {
        std::vector<std::vector<int>> action;
        for (const std::string& rec : split(group, 'b')) action.push_back(ints(rec));
        actions.push_back(action);
    }

    // Per-frame bounds. As in the original, the derived extents start at
    // (100, -100, 100, -100) once and keep growing across frames.
    std::vector<std::string> boundGroups = split(l[2], 'c');
    int minX = 100, maxX = -100, minY = 100, maxY = -100;
    for (size_t f = 0; f < frames.size(); ++f) {
        std::vector<std::string> fields = f < boundGroups.size() ? split(boundGroups[f], 'a')
                                                                 : std::vector<std::string>{""};
        std::vector<int> b;
        if (fields.size() < 2) {
            for (const std::vector<int>& part : frames[f]) {
                if (part.empty() || part[0] < 0 || part[0] >= static_cast<int>(pieces.size())) continue;
                const std::vector<int>& p = pieces[part[0]];
                if (p.size() < 4) continue;
                minX = std::min(minX, p[0]);
                maxX = std::max(maxX, p[0] + p[2]);
                minY = std::min(minY, p[1]);
                maxY = std::max(maxY, p[1] + p[3]);
            }
            b = {minX, maxX, minY, maxY};
        } else {
            int x = intValue(fields[0]), y = intValue(fields[1]);
            int w = fields.size() > 2 ? intValue(fields[2]) : 0, h = fields.size() > 3 ? intValue(fields[3]) : 0;
            b = {x, x + w, y, y + h};
        }
        for (int& v : b) v = std::abs(v);
        if (fields.size() < 5) {
            b.push_back(0);
            b.push_back(0);
        } else {
            b.push_back(intValue(fields[4]));
            b.push_back(fields.size() > 5 ? intValue(fields[5]) : 0);
        }
        frameBounds.push_back(b);
    }

    // Source images: the last bounds group, or one 1024 image by default.
    if (split(l[2], 'a').size() < 2) {
        images.push_back({0, 0, 1024, 1024});
    } else {
        for (const std::string& rec : split(boundGroups.back(), 'b')) images.push_back(ints(rec));
    }

    for (size_t i = 0; i < pieces.size(); ++i) pieces[i].push_back(getImageIndex(static_cast<int>(i)));
}

// Which stacked image a piece lies in; rebases the piece's y onto it.
int ResourceSpritexData::getImageIndex(int piece) {
    std::vector<int>& p = pieces[piece];
    if (p.size() < 2) p.resize(2, 0);
    int bottom = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        int h = images[i].size() > 1 ? images[i][1] : 0;
        bottom += h;
        if (p[1] <= bottom) {
            p[1] -= bottom - h;
            return static_cast<int>(i);
        }
    }
    return 0;
}

// ------------------------------------------------------------------ Spritex

Spritex* Spritex::initWithData(const std::string& images, const std::string& data) {
    Spritex* s = new Spritex;
    ResourceManager& rm = ResourceManager::Instance();
    s->resID = rm.createResource<ResourceSpritexData>(data, Resource::kSpritex);
    s->res = s->resID == -1 ? nullptr : static_cast<ResourceSpritexData*>(rm.getResource(s->resID));
    s->imgName = images;
    s->loadSpritexImage(images);
    if (s->res) {
        s->setSpritexStateWithType(0, 0);
        s->currentTime = platform::passTime();  // seconds, as the original did (so the first step fires at once)
    }
    return s;
}

Spritex::~Spritex() {
    if (res) ResourceManager::Instance().deleteResource(resID);
    for (AdvancedImage* img : spritexImage) releaseRef(img);
}

void Spritex::loadSpritexImage(const std::string& images) {
    for (const std::string& name : split(images, ',')) {
        AdvancedImage* img = AdvancedImage::initWithPathRowColData(name, 1, 1, nullptr);
        if (!img) img = AdvancedImage::initWithPathRowColData("false.png", 1, 1, nullptr);
        spritexImage.push_back(img);  // may still be null if false.png is missing too
    }
}

void Spritex::setSpritexStateWithType(int state, int type) {
    if (!res) return;
    actionState = state;
    actionType = type;
    actionEnable = true;
    actionFrameState = 0;
    frameState = res->actions.at(actionState).at(0).at(0);
}

void Spritex::nextSpritexFrame() {
    if (!res || !actionEnable) return;
    const std::vector<std::vector<int>>& action = res->actions.at(actionState);
    double now = nowMs();
    if (now - currentTime <= std::max<double>(action.at(actionFrameState).at(1), minFrameMs - 1)) return;
    currentTime = now;
    ++actionFrameState;
    int last = static_cast<int>(action.size()) - 1;
    if (actionFrameState > last) {
        if (actionType == 1) {
            actionFrameState = last;
            actionEnable = false;
        } else if (actionType == 2) {
            actionFrameState = 0;
            actionEnable = false;
        } else if (actionType == 0) {
            actionFrameState = 0;
        }
    }
    frameState = action.at(actionFrameState).at(0);
}

void Spritex::drawSpritex() {
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(static_cast<float>(posX), static_cast<float>(posY), 0);
    if (!res) {
        glScalef(scaleX, scaleY, 1);
        glRotatef(static_cast<float>(rotateAngle), 0, 0, 1);
        if (!spritexImage.empty() && spritexImage[0]) {
            spritexImage[0]->setMirrorWith(0, mirror != 0);
            spritexImage[0]->draw();
        }
        glPopMatrix();
        return;
    }
    // What the keyframes place where steps with them; only the transform
    // above moves smoothly.
    platform::KeyframeGroup art(serial());
    const std::vector<int>& fb = res->frameBounds.at(frameState);
    int side = mirror != 0 ? 1 : 0;  // pivot on the right edge when mirrored
    glTranslatef(static_cast<float>(fb[side]), static_cast<float>(fb[2]), 0);
    glScalef(scaleX, scaleY, 1);
    glRotatef(static_cast<float>(rotateAngle), 0, 0, 1);
    glTranslatef(static_cast<float>(-fb[side]), static_cast<float>(-fb[2]), 0);
    for (const std::vector<int>& part : res->frames.at(frameState)) {
        const std::vector<int>& piece = res->pieces.at(part.at(0));
        AdvancedImage* img = spritexImage.at(piece.at(4));
        if (!img) continue;
        int x = mirror == 0 ? fb[0] + part.at(1) : -piece.at(2) - part.at(1) + fb[1];
        img->setPositionWithXY(x, fb[2] + part.at(2));
        img->setMirrorWith(0, mirror != (part.size() > 3 ? part[3] : 0));
        img->drawWithImageRegionWithXYWidthHeight(piece[0], piece[1], piece[2], piece[3]);
    }
    glPopMatrix();
}

// The original indexes frameBounds by actionState here (not frameState).
int Spritex::getSpritexFrameWidth() const {
    if (!res) return spritexImage.empty() || !spritexImage[0] ? 0 : static_cast<int>(spritexImage[0]->getFrameWidth());
    const std::vector<int>& fb = res->frameBounds.at(actionState);
    return fb[0] + fb[1];
}

int Spritex::getSpritexFrameHeight() const {
    if (!res) return spritexImage.empty() || !spritexImage[0] ? 0 : static_cast<int>(spritexImage[0]->getFrameHeight());
    const std::vector<int>& fb = res->frameBounds.at(actionState);
    return fb[2] + fb[3];
}
