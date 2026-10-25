// Minimal Objective-C style object model for the port.
//
// The original game is Objective-C with manual retain/release. Mirroring that
// ownership model 1:1 keeps the translation of every decompiled method
// mechanical: `[x retain]` -> retainRef(x), `[x release]` -> releaseRef(x).
// Messages to nil are no-ops in Objective-C, so the free helpers accept null.
#pragma once

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

class Object {
public:
    Object() { ++live_; }
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
    virtual ~Object() { --live_; }
    // How many objects exist now (debug: leaks).
    static long live() { return live_; }

    void retain() { ++refs_; }
    void release() {
        if (--refs_ == 0) delete this;
    }
    int retainCount() const { return refs_; }
    // Unique for the whole run, unlike addresses, which get reused: tells
    // the draws of different objects apart across frames (DrawList.h).
    unsigned long long serial() const { return serial_; }
    // A serial for something drawn that is not an Object (a particle).
    static unsigned long long newSerial() { return ++nextSerial_; }

private:
    int refs_ = 1;  // alloc/init hands back one reference, like Objective-C
    inline static long live_ = 0;
    inline static unsigned long long nextSerial_ = 0;
    unsigned long long serial_ = ++nextSerial_;
};

template <class T>
T* retainRef(T* o) {
    if (o) o->retain();
    return o;
}

inline void releaseRef(Object* o) {
    if (o) o->release();
}

// NSMutableArray of objects: retains what it holds, releases on removal.
template <class T>
class Array {
public:
    Array() = default;
    Array(const Array&) = delete;
    Array& operator=(const Array&) = delete;
    // Moving hands over the references.
    Array(Array&& other) noexcept : items_(std::move(other.items_)) { other.items_.clear(); }
    Array& operator=(Array&& other) noexcept {
        if (this != &other) {
            removeAllObjects();
            items_ = std::move(other.items_);
            other.items_.clear();
        }
        return *this;
    }
    ~Array() { removeAllObjects(); }

    int count() const { return static_cast<int>(items_.size()); }
    bool empty() const { return items_.empty(); }

    // Out-of-range access is a programming error, as NSRangeException was.
    T* objectAtIndex(int i) const {
        if (i < 0 || i >= count()) throw std::out_of_range("Array::objectAtIndex");
        return items_[i];
    }
    T* operator[](int i) const { return objectAtIndex(i); }

    void addObject(T* o) {
        o->retain();
        items_.push_back(o);
    }
    void insertObject(T* o, int index) {
        o->retain();
        items_.insert(items_.begin() + index, o);
    }
    void replaceObjectAtIndex(int i, T* o) {
        T* old = objectAtIndex(i);
        o->retain();
        items_[i] = o;
        old->release();
    }
    void removeObjectAtIndex(int i) {
        T* o = objectAtIndex(i);
        items_.erase(items_.begin() + i);
        o->release();
    }
    // Removes every occurrence, like -[NSMutableArray removeObject:].
    void removeObject(T* o) {
        auto it = std::remove(items_.begin(), items_.end(), o);
        int n = static_cast<int>(items_.end() - it);
        items_.erase(it, items_.end());
        for (int k = 0; k < n; ++k) o->release();
    }
    void removeAllObjects() {
        std::vector<T*> old;
        old.swap(items_);
        for (T* o : old) o->release();
    }
    bool containsObject(const T* o) const {
        return std::find(items_.begin(), items_.end(), o) != items_.end();
    }
    int indexOfObject(const T* o) const {
        auto it = std::find(items_.begin(), items_.end(), o);
        return it == items_.end() ? -1 : static_cast<int>(it - items_.begin());
    }

    typename std::vector<T*>::const_iterator begin() const { return items_.begin(); }
    typename std::vector<T*>::const_iterator end() const { return items_.end(); }

private:
    std::vector<T*> items_;
};

struct Point {
    float x = 0, y = 0;
};

struct Size {
    float width = 0, height = 0;
};

struct Rect {
    Point origin;
    Size size;
    bool contains(Point p) const {
        return p.x >= origin.x && p.y >= origin.y && p.x < origin.x + size.width &&
               p.y < origin.y + size.height;
    }
};
