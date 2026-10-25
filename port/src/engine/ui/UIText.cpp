// Text views: text laid out and rasterised (plutovg) at the display's
// resolution with the system font that stands in for the iOS one.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "engine/ui/UIKit.h"
#include "platform/DrawList.h"
#include "platform/Platform.h"
#include "plutovg.h"

namespace ui {

std::string fontKind(const std::string& ios) {
    // The system's face of that kind stands in (platform::systemFont): no
    // system here has Helvetica or Marker Felt.
    std::string n = ios;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
    bool bold = n.find("bold") != std::string::npos;
    if (n.find("marker") != std::string::npos) return n.find("wide") != std::string::npos ? "hand-wide" : "hand";
    if (n.find("courier") != std::string::npos) return bold ? "mono-bold" : "mono";
    if (n.find("georgia") != std::string::npos) return bold ? "serif-bold" : "serif";
    return bold ? "sans-bold" : "sans";
}

namespace {

plutovg_font_face_t* face(const std::string& ios) {
    static std::map<std::string, plutovg_font_face_t*> faces;
    std::string kind = fontKind(ios);
    auto it = faces.find(kind);
    if (it != faces.end()) return it->second;
    std::string path = platform::systemFont(kind);
    plutovg_font_face_t* f = path.empty() ? nullptr : plutovg_font_face_load_from_file(path.c_str(), 0);
    if (!f && kind != "sans") f = face("Helvetica");
    faces[kind] = f;
    return f;
}

float advance(plutovg_font_face_t* f, float size, const std::string& s) {
    if (!f || s.empty()) return 0;
    return plutovg_font_face_text_extents(f, size, s.data(), static_cast<int>(s.size()), PLUTOVG_TEXT_ENCODING_UTF8,
                                          nullptr);
}

// Lines of the text wrapped at word boundaries to `width` (0: no wrapping),
// as UIKit's UILineBreakModeWordWrap.
std::vector<std::string> wrap(plutovg_font_face_t* f, float size, const std::string& text, float width) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        std::string para = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (width <= 0) {
            lines.push_back(para);
        } else {
            std::string line;
            size_t p = 0;
            while (p < para.size()) {
                size_t sp = para.find(' ', p);
                std::string word = para.substr(p, sp == std::string::npos ? std::string::npos : sp - p);
                std::string candidate = line.empty() ? word : line + " " + word;
                if (!line.empty() && advance(f, size, candidate) > width) {
                    lines.push_back(line);
                    line = word;
                } else {
                    line = candidate;
                }
                if (sp == std::string::npos) break;
                p = sp + 1;
            }
            lines.push_back(line);
        }
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return lines;
}

void metrics(plutovg_font_face_t* f, float size, float& ascent, float& lineHeight) {
    float a = size * 0.9f, d = -size * 0.2f, gap = 0;
    if (f) plutovg_font_face_get_metrics(f, size, &a, &d, &gap, nullptr);
    ascent = a;
    lineHeight = a - d + gap;
}

}  // namespace

Size textSize(const std::string& text, const std::string& font, float size, float width) {
    plutovg_font_face_t* f = face(font);
    std::vector<std::string> lines = wrap(f, size, text, width);
    float ascent = 0, lh = 0;
    metrics(f, size, ascent, lh);
    float w = 0;
    for (const std::string& l : lines) w = std::max(w, advance(f, size, l));
    return {w, lh * lines.size()};
}

struct TextView::Rendered {
    std::string key;
    GLuint texture = 0;
    float width = 0, height = 0;  // points
};

TextView::~TextView() {
    if (rendered_) {
        if (rendered_->texture) platform::releaseTexture(rendered_->texture);
        delete rendered_;
    }
}

void TextView::drawContent(float al) {
    if (text.empty() || bounds.width <= 0 || bounds.height <= 0) return;
    float scale = std::max(1.0f, platform::displayScale());
    char buf[160];
    std::snprintf(buf, sizeof buf, "|%s|%.2f|%.3f,%.3f,%.3f,%.3f|%d|%.1f|%.1f|%.2f|%.2f", fontName.c_str(), fontSize,
                  textColor.r, textColor.g, textColor.b, textColor.a, textAlignment, bounds.width, bounds.height,
                  inset, scale);
    std::string key = text + buf;
    if (!rendered_) rendered_ = new Rendered;
    if (rendered_->key != key) {
        if (rendered_->texture) platform::releaseTexture(rendered_->texture);
        rendered_->texture = 0;
        rendered_->key = key;
        int w = static_cast<int>(std::ceil(bounds.width * scale)), h = static_cast<int>(std::ceil(bounds.height * scale));
        plutovg_font_face_t* f = face(fontName);
        plutovg_surface_t* s = plutovg_surface_create(w, h);
        if (s && f) {
            plutovg_canvas_t* c = plutovg_canvas_create(s);
            plutovg_canvas_scale(c, scale, scale);
            float size = fontSize;
            float ascent = 0, lh = 0;
            float avail = bounds.width - 2 * inset;
            std::vector<std::string> lines = wrap(f, size, text, avail);
            metrics(f, size, ascent, lh);
            // The handwritten face standing in for Marker Felt is wider:
            // where the text would run out of its box it is set smaller
            // (to at most 65%) so it all shows, as it did on the device.
            bool standIn = fontKind(fontName).rfind("hand", 0) == 0;
            while (standIn && size > fontSize * 0.65f && inset * 2 + lh * static_cast<float>(lines.size()) > bounds.height) {
                size -= 0.5f;
                lines = wrap(f, size, text, avail);
                metrics(f, size, ascent, lh);
            }
            plutovg_canvas_set_font(c, f, size);
            plutovg_canvas_set_rgba(c, textColor.r, textColor.g, textColor.b, textColor.a);
            float y = inset + ascent;
            for (const std::string& l : lines) {
                float lw = advance(f, size, l);
                float x = textAlignment == kCenterAlign  ? inset + (avail - lw) / 2
                          : textAlignment == kRightAlign ? bounds.width - inset - lw
                                                         : inset;
                plutovg_canvas_fill_text(c, l.data(), static_cast<int>(l.size()), PLUTOVG_TEXT_ENCODING_UTF8, x, y);
                y += lh;
            }
            plutovg_canvas_destroy(c);
            // Premultiplied ARGB words (BGRA bytes) to the texture.
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            // (A surface's rows are its width: its stride is w * 4.)
            platform::toTexelOrder(plutovg_surface_get_data(s), static_cast<size_t>(w) * h);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, platform::kTexelFormat, GL_UNSIGNED_BYTE,
                         plutovg_surface_get_data(s));
            rendered_->texture = tex;
        }
        if (s) plutovg_surface_destroy(s);
        rendered_->width = bounds.width;
        rendered_->height = bounds.height;
    }
    if (!rendered_->texture) return;
    platform::Sprite sp;
    float xy[8] = {0, 0, rendered_->width, 0, 0, rendered_->height, rendered_->width, rendered_->height};
    std::copy(xy, xy + 8, sp.xy);
    float uv[8] = {0, 0, 1, 0, 0, 1, 1, 1};
    float rgba[4] = {al, al, al, al};  // premultiplied: the fade scales all four
    platform::drawSprite(serial(), rendered_->texture, sp, uv, rgba, GL_ONE);
}

}  // namespace ui
