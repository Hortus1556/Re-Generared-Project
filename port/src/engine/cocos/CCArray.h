// cocos2d-x 2.1.1's CCArray (cocoa/CCArray), ported to Bleed Engine: an array
// of objects that retains what it holds. MIT: LICENSE-cocos2d-x.txt.
#pragma once

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "engine/cocos/CCObject.h"

namespace cocos2d {

#define CC_INVALID_INDEX 0xffffffff

class CCArray : public CCObject {
public:
    ~CCArray() override { removeAllObjects(); }
    static CCArray* create() {
        CCArray* a = new CCArray;
        a->autorelease();
        return a;
    }
    static CCArray* createWithCapacity(unsigned int capacity) {
        CCArray* a = create();
        a->data.reserve(capacity);
        return a;
    }
    static CCArray* createWithArray(CCArray* other) {
        CCArray* a = create();
        a->addObjectsFromArray(other);
        return a;
    }
    bool init() { return true; }
    bool initWithCapacity(unsigned int capacity) { data.reserve(capacity); return true; }

    unsigned int count() const { return static_cast<unsigned int>(data.size()); }
    unsigned int capacity() const { return static_cast<unsigned int>(data.capacity()); }
    unsigned int indexOfObject(CCObject* o) const {
        auto it = std::find(data.begin(), data.end(), o);
        return it == data.end() ? CC_INVALID_INDEX : static_cast<unsigned int>(it - data.begin());
    }
    CCObject* objectAtIndex(unsigned int i) const { return data[i]; }
    CCObject* lastObject() const { return data.empty() ? nullptr : data.back(); }
    CCObject* randomObject() const {
        if (data.empty()) return nullptr;
        float r = std::rand() / (RAND_MAX + 1.0f);  // CCRANDOM_0_1
        return data[static_cast<size_t>(r * data.size())];
    }
    bool containsObject(CCObject* o) const { return indexOfObject(o) != CC_INVALID_INDEX; }

    void addObject(CCObject* o) {
        o->retain();
        data.push_back(o);
    }
    void addObjectsFromArray(CCArray* other) {
        if (other)
            for (CCObject* o : other->data) addObject(o);
    }
    void insertObject(CCObject* o, unsigned int index) {
        o->retain();
        data.insert(data.begin() + index, o);
    }
    void removeLastObject(bool releaseObj = true) {
        if (data.empty()) return;
        CCObject* o = data.back();
        data.pop_back();
        if (releaseObj) o->release();
    }
    void removeObject(CCObject* o, bool releaseObj = true) {
        unsigned int i = indexOfObject(o);
        if (i != CC_INVALID_INDEX) removeObjectAtIndex(i, releaseObj);
    }
    void removeObjectAtIndex(unsigned int i, bool releaseObj = true) {
        CCObject* o = data[i];
        data.erase(data.begin() + i);
        if (releaseObj) o->release();
    }
    void removeObjectsInArray(CCArray* other) {
        if (other)
            for (CCObject* o : other->data) removeObject(o);
    }
    void removeAllObjects() {
        std::vector<CCObject*> old;
        old.swap(data);
        for (CCObject* o : old) o->release();
    }
    void fastRemoveObject(CCObject* o) { removeObject(o); }
    void fastRemoveObjectAtIndex(unsigned int i) { removeObjectAtIndex(i); }
    void exchangeObjectAtIndex(unsigned int i, unsigned int j) { std::swap(data[i], data[j]); }
    void replaceObjectAtIndex(unsigned int i, CCObject* o, bool releaseObj = true) {
        o->retain();
        CCObject* old = data[i];
        data[i] = o;
        if (releaseObj) old->release();
    }
    void reverseObjects() { std::reverse(data.begin(), data.end()); }

    std::vector<CCObject*> data;  // 2.1.1's ccArray: what CCARRAY_FOREACH walks
};

#define CCARRAY_FOREACH(__array__, __object__)                                                         \
    if ((__array__) && (__array__)->data.size() > 0)                                                  \
        for (size_t __i_##__object__ = 0;                                                             \
             __i_##__object__ < (__array__)->data.size() &&                                           \
             (((__object__) = (__array__)->data[__i_##__object__]) != nullptr || true);               \
             ++__i_##__object__)
#define CCARRAY_FOREACH_REVERSE(__array__, __object__)                                                 \
    if ((__array__) && (__array__)->data.size() > 0)                                                  \
        for (size_t __i_##__object__ = (__array__)->data.size();                                      \
             __i_##__object__-- > 0 && (((__object__) = (__array__)->data[__i_##__object__]) != nullptr || true);)

}  // namespace cocos2d
