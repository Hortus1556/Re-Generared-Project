#include "core/Json.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace json {

namespace {

const Value kNullValue;

struct Parser {
    const std::string& s;
    size_t i = 0;
    std::string error;

    void space() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    }
    bool fail(const char* what) {
        if (error.empty()) error = std::string(what) + " at " + std::to_string(i);
        return false;
    }
    bool literal(const char* word) {
        size_t n = std::strlen(word);
        if (s.compare(i, n, word) != 0) return false;
        i += n;
        return true;
    }
    static void utf8(std::string& out, unsigned c) {
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    bool string(std::string& out) {
        if (i >= s.size() || s[i] != '"') return fail("string expected");
        ++i;
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i >= s.size()) return fail("bad escape");
            char e = s[i++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                if (i + 4 > s.size()) return fail("bad \\u");
                unsigned c1 = static_cast<unsigned>(std::strtoul(s.substr(i, 4).c_str(), nullptr, 16));
                i += 4;
                if (c1 >= 0xD800 && c1 < 0xDC00 && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                    unsigned c2 = static_cast<unsigned>(std::strtoul(s.substr(i + 2, 4).c_str(), nullptr, 16));
                    i += 6;
                    c1 = 0x10000 + ((c1 - 0xD800) << 10) + (c2 - 0xDC00);
                }
                utf8(out, c1);
                break;
            }
            default: return fail("bad escape");
            }
        }
        if (i >= s.size()) return fail("unterminated string");
        ++i;
        return true;
    }
    bool value(Value& v) {
        space();
        if (i >= s.size()) return fail("value expected");
        char c = s[i];
        if (c == '{') {
            v.type = Value::kObject;
            ++i;
            space();
            if (i < s.size() && s[i] == '}') {
                ++i;
                return true;
            }
            for (;;) {
                space();
                // A stray bare word before a key (AS2's cutscene_start_24 has
                // one, which the game's own parser passed over): skipped.
                while (i < s.size() && s[i] != '"' && s[i] != '}') ++i;
                if (i < s.size() && s[i] == '}') {
                    ++i;
                    return true;
                }
                std::string k;
                if (!string(k)) return false;
                space();
                if (i >= s.size() || s[i] != ':') return fail("':' expected");
                ++i;
                // A key given twice: the last value wins, in the first one's
                // place (JsonCpp's, which AS3's data was written for: some
                // surgery files set "scream" twice, "" then "male")
                Value* slot = nullptr;
                for (auto& m : v.members)
                    if (m.first == k) slot = &m.second;
                if (slot) {
                    *slot = Value{};
                } else {
                    v.members.emplace_back(std::move(k), Value{});
                    slot = &v.members.back().second;
                }
                if (!value(*slot)) return false;
                space();
                if (i < s.size() && s[i] == ',') {
                    ++i;
                    continue;
                }
                if (i < s.size() && s[i] == '}') {
                    ++i;
                    return true;
                }
                return fail("',' or '}' expected");
            }
        }
        if (c == '[') {
            v.type = Value::kArray;
            ++i;
            space();
            if (i < s.size() && s[i] == ']') {
                ++i;
                return true;
            }
            for (;;) {
                v.items.emplace_back();
                if (!value(v.items.back())) return false;
                space();
                if (i < s.size() && s[i] == ',') {
                    ++i;
                    continue;
                }
                if (i < s.size() && s[i] == ']') {
                    ++i;
                    return true;
                }
                return fail("',' or ']' expected");
            }
        }
        if (c == '"') {
            v.type = Value::kString;
            return string(v.string);
        }
        if (literal("true")) {
            v.type = Value::kBool;
            v.boolean = true;
            return true;
        }
        if (literal("false")) {
            v.type = Value::kBool;
            return true;
        }
        if (literal("null")) return true;
        char* end = nullptr;
        v.number = std::strtod(s.c_str() + i, &end);
        if (end == s.c_str() + i) return fail("value expected");
        v.type = Value::kNumber;
        i = static_cast<size_t>(end - s.c_str());
        return true;
    }
};

}  // namespace

bool Value::has(const std::string& key) const {
    if (type != kObject) return false;
    for (const auto& [k, v] : members)
        if (k == key) return true;
    return false;
}

const Value& Value::operator[](const std::string& key) const {
    if (type != kObject) return kNullValue;
    for (const auto& [k, v] : members)
        if (k == key) return v;
    return kNullValue;
}

const Value& Value::operator[](size_t i) const { return type == kArray && i < items.size() ? items[i] : kNullValue; }

bool parse(const std::string& text, Value& out, std::string* error) {
    Parser p{text};
    out = Value{};
    bool ok = p.value(out);
    if (ok) {
        p.space();
        if (p.i != text.size()) ok = p.fail("text after the value");
    }
    if (!ok && error) *error = p.error;
    return ok;
}

std::string escape(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char b[8];
                std::snprintf(b, sizeof b, "\\u%04x", c);
                out += b;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    return out + "\"";
}

void Writer::newline() {
    if (!pretty) return;
    out += '\n';
    out.append(first.size() * 2, ' ');
}

void Writer::before() {
    if (afterKey) {
        afterKey = false;
        return;
    }
    if (first.empty()) return;
    if (!first.back()) out += ',';
    first.back() = false;
    newline();
}

Writer& Writer::beginObject() {
    before();
    out += '{';
    first.push_back(true);
    return *this;
}

Writer& Writer::endObject() {
    bool empty = first.back();
    first.pop_back();
    if (!empty) newline();
    out += '}';
    return *this;
}

Writer& Writer::beginArray() {
    before();
    out += '[';
    first.push_back(true);
    return *this;
}

Writer& Writer::endArray() {
    bool empty = first.back();
    first.pop_back();
    if (!empty) newline();
    out += ']';
    return *this;
}

Writer& Writer::key(const std::string& k) {
    before();
    out += escape(k);
    out += pretty ? ": " : ":";
    afterKey = true;
    return *this;
}

Writer& Writer::value(const std::string& s) {
    before();
    out += escape(s);
    return *this;
}

Writer& Writer::value(double d) {
    before();
    char b[32];
    std::snprintf(b, sizeof b, "%.17g", d);
    out += b;
    return *this;
}

Writer& Writer::value(float f) {
    before();
    char b[32];
    std::snprintf(b, sizeof b, "%.9g", static_cast<double>(f));
    out += b;
    return *this;
}

Writer& Writer::value(int i) { return value(static_cast<long long>(i)); }

Writer& Writer::value(long long i) {
    before();
    out += std::to_string(i);
    return *this;
}

Writer& Writer::value(bool b) {
    before();
    out += b ? "true" : "false";
    return *this;
}

Writer& Writer::null() {
    before();
    out += "null";
    return *this;
}

}  // namespace json
