#include "as2/menus/FlashStage.h"

#include <cstdio>
#include <map>

#include "as2/GameController.h"
#include "platform/Platform.h"

namespace as2::flashstage {

namespace {

constexpr float kStageW = 600, kStageH = 460;

std::map<std::string, VectorArt*>& loaded() {
    static std::map<std::string, VectorArt*> m;
    return m;
}

}  // namespace

VectorArt& art(const std::string& package) {
    VectorArt*& a = loaded()[package];
    if (!a) a = &VectorArt::library(package + "/library.json");
    return *a;
}

void preload() {
    for (const char* p : {"flash", "flash-shell", "flash-preloader", "flash-cutscene"})
        VectorArt::preload(std::string(p) + "/library.json");
}

bool gHand = false;

void wantHand() { gHand = true; }

void endFrame() {
    for (auto& kv : loaded()) kv.second->endFrame();
    platform::setHandCursor(gHand);
    gHand = false;
}

float scale() { return gameController->ActualScreenSize.height / kStageH; }

float left() { return (gameController->ActualScreenSize.width - kStageW * scale()) * 0.5f; }

VectorTransform at(float x, float y, float turn) {
    VectorTransform t;
    t.x = left() + x * scale();
    t.y = y * scale();
    t.scaleX = t.scaleY = scale();
    t.rotation = turn;
    return t;
}

Point toStage(Point screen) { return {(screen.x - left()) / scale(), screen.y / scale()}; }

Point stageToWorld(float x, float y) { return {(left() + x * scale()) / 2, y * scale() / 2}; }

float worldPerStage() { return scale() / 2; }

std::string prettyScore(long long v) {
    std::string s = std::to_string(v), out;
    int n = static_cast<int>(s.size());
    for (int i = 0; i < n; ++i) {
        if (i && (n - i) % 3 == 0 && s[0] != '-') out += ',';
        out += s[static_cast<size_t>(i)];
    }
    return out;
}

std::string formatTime(int seconds) {
    char b[16];
    std::snprintf(b, sizeof b, "%d:%02d", seconds / 60, seconds % 60);
    return b;
}

}  // namespace as2::flashstage
