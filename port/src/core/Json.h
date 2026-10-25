// Minimal JSON: a value tree read from text, and a writer that builds text.
// For the game packages (port/games/<game>/: library, symbols, manifest);
// no dependency beyond the standard library.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace json {

struct Value {
    enum Type { kNull, kBool, kNumber, kString, kArray, kObject } type = kNull;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<Value> items;                            // kArray
    std::vector<std::pair<std::string, Value>> members;  // kObject, in order

    bool isNull() const { return type == kNull; }
    bool has(const std::string& key) const;
    // A member (a null value if absent or not an object), an item.
    const Value& operator[](const std::string& key) const;
    const Value& operator[](size_t i) const;
    size_t size() const { return type == kArray ? items.size() : type == kObject ? members.size() : 0; }

    double num(double fallback = 0) const { return type == kNumber ? number : fallback; }
    int integer(int fallback = 0) const { return type == kNumber ? static_cast<int>(number) : fallback; }
    bool flag(bool fallback = false) const { return type == kBool ? boolean : fallback; }
    const std::string& str() const { return string; }
};

// False (and `error` set) if the text is not JSON.
bool parse(const std::string& text, Value& out, std::string* error = nullptr);

// Builds JSON text: begin/end objects and arrays, keys, values. Commas are
// put in for you. Floats are written with enough digits to read back the
// same float.
class Writer {
public:
    explicit Writer(bool pretty = false) : pretty(pretty) {}
    Writer& beginObject();
    Writer& endObject();
    Writer& beginArray();
    Writer& endArray();
    Writer& key(const std::string& k);
    Writer& value(const std::string& s);
    Writer& value(const char* s) { return value(std::string(s)); }
    Writer& value(double d);
    Writer& value(float f);
    Writer& value(int i);
    Writer& value(long long i);
    Writer& value(unsigned i) { return value(static_cast<long long>(i)); }
    Writer& value(bool b);
    Writer& null();
    // A key and a value at once.
    template <class T>
    Writer& field(const std::string& k, const T& v) {
        key(k);
        return value(v);
    }
    const std::string& text() const { return out; }

private:
    void before();
    void newline();
    std::string out;
    std::vector<bool> first;  // per open container: nothing in it yet
    bool afterKey = false;
    bool pretty;
};

std::string escape(const std::string& s);

}  // namespace json
