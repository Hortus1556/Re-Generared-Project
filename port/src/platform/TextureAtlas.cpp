#include "platform/TextureAtlas.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "platform/DrawList.h"

namespace platform {
namespace {

// Art over 128 texels high (the big art's tiles, 256 a side, 258 with the
// ring, its edge tiles, big pieces) has pages of its own, 4 x 4 cells of
// 258, any cell let go taken again as it is: most of the texels are tiles,
// and among art of other sizes they left holes nothing else fitted, or
// shelves of their height nothing else could share (pages half empty).
constexpr int kCell = kAtlasLargest + 2;
constexpr int kCellsSide = 4;
constexpr int kTilePage = kCell * kCellsSide;  // 1032
// Other art goes in smaller pages, cut into shelves (rows of one height) as
// slots ask, a shelf's free stretches kept sorted. A shelf with nothing
// left in it is a free band: bands next to each other merge, and a new
// shelf of any height is cut from one (best fit) before the page's top
// grows. Shelves are a power of two high (8 to 128 texels and the ring), a
// slot going in the lowest that holds it: with a shelf for each of the
// art's own heights (a few an octave) most shelves were nearly empty.
constexpr int kArtPage = 1040;  // (8 shelves of the highest, 130)

struct Span {
    int x, width;
};
struct Shelf {
    int y, height;
    std::vector<Span> free;  // (a free band: one span, the whole width)
    long long used = 0;      // texels in slots
};
struct Page {
    GLuint id = 0;
    int side = 0;
    bool cells = false;
    std::vector<int> freeCells;  // (cells: by number, row by row)
    std::vector<Shelf> shelves;  // (else: by y, covering [0, top))
    int top = 0;
    long long used = 0;
};
std::vector<std::unique_ptr<Page>> gPages;

int shelfHeight(int h) {
    int s = 10;  // (8 and the ring)
    while (s < h) s = (s - 2) * 2 + 2;
    return s;
}
bool empty(const Shelf& s) { return s.used == 0; }
bool isTile(int w, int h) { return w <= kCell && h > kAtlasLargest / 2 + 2 && h <= kCell; }

Page* newPage(bool cells) {
    auto page = std::make_unique<Page>();
    page->cells = cells;
    page->side = cells ? kTilePage : kArtPage;
    if (cells)
        for (int i = kCellsSide * kCellsSide - 1; i >= 0; --i) page->freeCells.push_back(i);
    page->id = newTexture();
    // (transparent: what no slot holds is never drawn, but a ring texel of a
    // slot next to it may be filtered in)
    setTexels(page->id, page->side, page->side, true,
              std::vector<unsigned char>(static_cast<size_t>(page->side) * page->side * 4, 0));
    gPages.push_back(std::move(page));
    return gPages.back().get();
}

// A new shelf `h` high: from the free band that fits best, else on top.
Shelf* newShelf(Page& p, int h) {
    size_t best = p.shelves.size();
    for (size_t i = 0; i < p.shelves.size(); ++i)
        if (empty(p.shelves[i]) && p.shelves[i].height >= h &&
            (best == p.shelves.size() || p.shelves[i].height < p.shelves[best].height))
            best = i;
    if (best < p.shelves.size()) {
        Shelf& band = p.shelves[best];
        if (band.height - h >= 10) {  // (the rest stays a band)
            Shelf rest{band.y + h, band.height - h, {{0, p.side}}};
            band.height = h;
            p.shelves.insert(p.shelves.begin() + static_cast<long>(best) + 1, rest);
        }
        return &p.shelves[best];
    }
    if (p.top + h > p.side) return nullptr;
    p.shelves.push_back({p.top, h, {{0, p.side}}});
    p.top += h;
    return &p.shelves.back();
}

// A slot in the page: in a free cell, or in a shelf of its height with
// room (with `newShelf`, in a new shelf too).
bool take(Page& p, int w, int h, bool mayCutShelf, AtlasSlot& slot) {
    if (p.cells != isTile(w, h)) return false;
    if (p.cells) {
        if (p.freeCells.empty()) return false;
        const int cell = p.freeCells.back();
        p.freeCells.pop_back();
        slot.x = cell % kCellsSide * kCell;
        slot.y = cell / kCellsSide * kCell;
    } else {
        Shelf* best = nullptr;
        size_t bestSpan = 0;
        for (Shelf& s : p.shelves) {
            if (empty(s) || s.height != shelfHeight(h)) continue;
            for (size_t i = 0; i < s.free.size() && !best; ++i)
                if (s.free[i].width >= w) {
                    best = &s;
                    bestSpan = i;
                }
            if (best) break;
        }
        if (!best) {
            if (!mayCutShelf) return false;
            best = newShelf(p, shelfHeight(h));
            if (!best) return false;
            bestSpan = 0;
        }
        Span& span = best->free[bestSpan];
        slot.x = span.x;
        slot.y = best->y;
        span.x += w;
        span.width -= w;
        if (span.width == 0) best->free.erase(best->free.begin() + static_cast<long>(bestSpan));
        best->used += static_cast<long long>(w) * h;
    }
    slot.page = p.id;
    slot.pageSide = p.side;
    p.used += static_cast<long long>(w) * h;
    return true;
}

// An empty page goes, unless it is the only empty one of its kind: one is
// kept for what comes (a page made and deleted again and again is memory
// the driver keeps).
void dropIfEmpty(std::vector<std::unique_ptr<Page>>::iterator it) {
    Page& p = **it;
    if (p.used != 0) return;
    const bool cells = p.cells;
    if (std::count_if(gPages.begin(), gPages.end(), [&](const auto& q) { return q->cells == cells && q->used == 0; }) <= 1)
        return;
    releaseTexture(p.id);
    gPages.erase(it);
}

}  // namespace

bool atlasPut(int width, int height, const std::vector<unsigned char>& texels, bool drawnNow, AtlasSlot& slot) {
    if (width <= 0 || height <= 0 || width > kAtlasLargest || height > kAtlasLargest ||
        texels.size() < static_cast<size_t>(width) * height * 4)
        return false;
    const int w = width + 2, h = height + 2;  // (its transparent ring)
    slot = AtlasSlot{};
    // The fullest pages first: art comes and goes, and the pages it leaves
    // emptying out can go.
    std::vector<Page*> order;
    for (auto& p : gPages) order.push_back(p.get());
    std::sort(order.begin(), order.end(), [](const Page* a, const Page* b) {
        return a->used * b->side * b->side > b->used * a->side * a->side;
    });
    // (a shelf with room anywhere before a new shelf anywhere: else every
    // page grew shelves of each height, nearly empty)
    for (bool cut : {false, true})
        for (Page* p : order)
            if (!slot.page && take(*p, w, h, cut, slot)) break;
    if (!slot.page && !take(*newPage(isTile(w, h)), w, h, true, slot)) return false;
    slot.width = width;
    slot.height = height;
    std::vector<unsigned char> block(static_cast<size_t>(w) * h * 4, 0);
    for (int y = 0; y < height; ++y)
        std::memcpy(&block[(static_cast<size_t>(y + 1) * w + 1) * 4], &texels[static_cast<size_t>(y) * width * 4],
                    static_cast<size_t>(width) * 4);
    setTexelsAt(slot.page, slot.x, slot.y, w, h, std::move(block), drawnNow);
    return true;
}

void releaseSlot(const AtlasSlot& slot) {
    if (slot.page) releaseAtlasSlot(slot);
}

void atlasFree(const AtlasSlot& slot) {
    auto it = std::find_if(gPages.begin(), gPages.end(), [&](const auto& p) { return p->id == slot.page; });
    if (it == gPages.end()) return;
    Page& p = **it;
    const int w = slot.width + 2, h = slot.height + 2;
    p.used -= static_cast<long long>(w) * h;
    if (p.cells) {
        p.freeCells.push_back(slot.y / kCell * kCellsSide + slot.x / kCell);
        dropIfEmpty(it);
        return;
    }
    auto s = std::find_if(p.shelves.begin(), p.shelves.end(), [&](const Shelf& sh) { return sh.y == slot.y; });
    if (s == p.shelves.end()) return;
    auto at = std::lower_bound(s->free.begin(), s->free.end(), slot.x, [](const Span& a, int x) { return a.x < x; });
    at = s->free.insert(at, {slot.x, w});
    // (merged with the stretches either side)
    if (at + 1 != s->free.end() && at->x + at->width == (at + 1)->x) {
        at->width += (at + 1)->width;
        s->free.erase(at + 1);
    }
    if (at != s->free.begin() && (at - 1)->x + (at - 1)->width == at->x) {
        (at - 1)->width += at->width;
        s->free.erase(at);
    }
    s->used -= static_cast<long long>(w) * h;
    if (empty(*s)) {
        // A free band, merged with the bands next to it; at the top, the
        // page's rows given back.
        size_t i = static_cast<size_t>(s - p.shelves.begin());
        if (i + 1 < p.shelves.size() && empty(p.shelves[i + 1])) {
            p.shelves[i].height += p.shelves[i + 1].height;
            p.shelves.erase(p.shelves.begin() + static_cast<long>(i) + 1);
        }
        if (i > 0 && empty(p.shelves[i - 1])) {
            p.shelves[i - 1].height += p.shelves[i].height;
            p.shelves.erase(p.shelves.begin() + static_cast<long>(i));
        }
        while (!p.shelves.empty() && empty(p.shelves.back())) {
            p.top -= p.shelves.back().height;
            p.shelves.pop_back();
        }
    }
    dropIfEmpty(it);
}

float atlasPageFill(GLuint page) {
    for (const auto& p : gPages)
        if (p->id == page) return static_cast<float>(p->used) / (static_cast<float>(p->side) * p->side);
    return 1;
}

size_t atlasBytes() {
    size_t n = 0;
    for (const auto& p : gPages) n += static_cast<size_t>(p->side) * p->side * 4;
    return n;
}

size_t atlasUnusedBytes() {
    long long used = 0;
    for (const auto& p : gPages) used += p->used;
    return atlasBytes() - static_cast<size_t>(used) * 4;
}

std::string atlasStats() {
    long long used[2] = {0, 0}, area[2] = {0, 0};
    int count[2] = {0, 0}, shelves = 0, bands = 0;
    for (const auto& p : gPages) {
        used[p->cells] += p->used;
        area[p->cells] += static_cast<long long>(p->side) * p->side;
        ++count[p->cells];
        for (const Shelf& sh : p->shelves) (empty(sh) ? bands : shelves)++;
    }
    auto share = [&](int k) { return area[k] ? static_cast<int>(used[k] * 100 / area[k]) : 0; };
    return std::to_string(gPages.size()) + " atlas pages (" + std::to_string(atlasBytes() >> 20) + " MB; " +
           std::to_string(count[1]) + " of tiles " + std::to_string(share(1)) + "% in slots, " + std::to_string(count[0]) +
           " of other art " + std::to_string(share(0)) + "%, " + std::to_string(shelves) + " shelves, " +
           std::to_string(bands) + " free bands)";
}

}  // namespace platform
