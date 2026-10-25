#include "as3/Animation.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "as3/Content.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"

namespace mt {

using namespace cocos2d;

namespace {

constexpr float kCanvasHeight = 384;  // (0x5a648)

// A frame's centre on the canvas (y up), from where the stage (y down) has
// its top-left corner, its size in cocos points (0x5a5bc).
CCPoint framePosition(int x, int y, int width, int height) {
    float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    return ccp(static_cast<float>(x) + static_cast<float>(width) / s * 0.5f,
               kCanvasHeight - static_cast<float>(y) - static_cast<float>(height) / s * 0.5f);
}

// A list of points one a frame, from entries given for some frames ("frame",
// 1 the first): the frames between repeat the last given ((0, 0) before
// any), as many as `frameCount` at least. JsonCpp's asInt: the numbers cut
// to whole points; y turned up.
std::vector<CCPoint> framePoints(const json::Value& entries, int frameCount) {
    std::vector<CCPoint> out;
    auto fillTo = [&out](int n) {
        while (static_cast<int>(out.size()) < n) out.push_back(out.empty() ? CCPointZero : out.back());
    };
    for (const json::Value& e : entries.items) {
        fillTo(e["frame"].integer() - 1);
        out.push_back(ccp(static_cast<float>(e["x"].integer()), static_cast<float>(-e["y"].integer())));
    }
    fillTo(frameCount);
    return out;
}

// JsonCpp keeps an object's members by key (a std::map): the original went
// through the clips in that order.
std::vector<const std::pair<std::string, json::Value>*> sortedMembers(const json::Value& object) {
    std::vector<const std::pair<std::string, json::Value>*> out;
    for (const auto& m : object.members) out.push_back(&m);
    std::sort(out.begin(), out.end(), [](const auto* a, const auto* b) { return a->first < b->first; });
    return out;
}

}  // namespace

Animation* Animation::create(const char* file, int prepare) {
    Animation* a = new Animation;
    a->init();
    a->initWithFile(file);  // (the original kept it whatever the reading gave)
    if (prepare == 1) a->prepare();
    a->autorelease();
    return a;
}

Animation::~Animation() {
    stop();
    for (auto& c : m_clips) {
        for (CCSpriteFrame* f : c->frames) CC_SAFE_RELEASE(f);
        CC_SAFE_RELEASE(c->sprite);
        for (CCNode* n : c->controlNodes) n->release();
    }
}

bool Animation::initWithFile(const char* file) {
    m_prepared = false;
    m_current = nullptr;
    m_queueTail = -1;
    m_queueHead = -1;
    m_addPending = true;
    m_flipX = false;
    m_paused = false;
    json::Value root;
    if (!readJson(file, root)) return true;  // (the original went on with an empty file too)
    m_frameRate = root["frame_rate"].integer();
    for (const auto* member : sortedMembers(root["clips"])) {
        const json::Value& def = member->second;
        auto clip = std::make_unique<Clip>();
        clip->name = member->first;
        clip->depth = def["depth"].integer();
        clip->rotation = def["rotation"].integer();
        const json::Value& frames = def["frames"];
        clip->frameCount = frames["frames"].integer();
        clip->offsets = framePoints(frames["offset"], clip->frameCount);
        for (const json::Value& cp : def["cp"].items) {
            ControlPoint p;
            p.name = cp["name"].type == json::Value::kString ? cp["name"].string : "";
            p.pos = framePoints(cp["pos"], clip->frameCount);
            clip->controlPoints.push_back(p);
        }
        for (const json::Value& prop : def["props"].items) {
            std::string name = prop["name"].type == json::Value::kString ? prop["name"].string : "";
            addTimeline(clip.get(), name, prop["timeline"]);
        }
        clip->x = def["x"].integer();
        clip->y = def["y"].integer();
        m_clips.push_back(std::move(clip));
    }
    return true;
}

void Animation::addTimeline(Clip* clip, const std::string& name, const json::Value& timeline) {
    Timeline t;
    t.clip = clip;
    int total = 0;
    for (const json::Value& e : timeline.items) {
        Segment s;
        s.from = e["from"].integer();
        s.to = e["to"].integer();
        s.count = e["count"].integer();
        s.length = std::abs(s.to - s.from) + 1;
        s.total = s.count != -1 ? s.count * s.length : s.length;
        t.segments.push_back(s);
        total += s.total;
        if (std::max(s.from, s.to) > clip->frameCount)
            logError("The %s timeline from %s has more frames than the movieclip", name.c_str(), clip->name.c_str());
    }
    Prop& p = m_props[name];
    p.name = name;
    p.length = std::max(p.length, total);
    p.timelines.push_back(t);
}

void Animation::prepare() {
    CCSpriteFrameCache* cache = CCSpriteFrameCache::sharedSpriteFrameCache();
    for (auto& c : m_clips) {
        char name[128];
        for (int i = 1; i <= c->frameCount; ++i) {
            // (the original printed "file <name>" to the console for each)
            std::snprintf(name, sizeof name, "%s_%d.png", c->name.c_str(), i);
            CCSpriteFrame* f = cache->spriteFrameByName(name);
            CC_SAFE_RETAIN(f);
            c->frames.push_back(f);
        }
        // (the original threw, ending the game, when a clip had no frames)
        if (c->frames.empty() || !c->frames[0]) {
            logError("Animation: no frame %s_1.png", c->name.c_str());
            c->sprite = CCSprite::create();
        } else {
            c->sprite = CCSprite::createWithSpriteFrame(c->frames[0]);
        }
        c->sprite->retain();
        // where the clip's first frame has its centre, on the canvas
        CCSize size = c->sprite->boundingBox().size;
        CCPoint p = framePosition(c->x, c->y, static_cast<int>(size.width), static_cast<int>(size.height));
        c->x = static_cast<int>(p.x);
        c->y = static_cast<int>(p.y);
        for (size_t i = 0; i < c->controlPoints.size(); ++i) {
            CCNode* n = CCNode::create();
            n->retain();
            c->controlNodes.push_back(n);
        }
    }
    m_prepared = true;
}

void Animation::stop() {
    if (!m_current) return;
    if (m_paused) return;  // (as the original: paused, it stays)
    unschedule(schedule_selector(Animation::update));
    m_current = nullptr;
    m_paused = false;
}

void Animation::detachAll() {
    for (Attachment& a : m_attachments)
        if (a.node->getParent()) a.node->removeFromParentAndCleanup(false);
}

void Animation::showFrame(const char* name, int frame) {
    stop();
    removeAllChildrenWithCleanup(false);
    detachAll();
    if (!m_props.count(name)) logError("Animation: no animation %s", name);
    m_current = &m_props[name];  // (an unknown name made an empty one, as std::map's [] did)
    m_frame = frame;
    m_addPending = true;
    update(0);
}

void Animation::play(const char* name, int loops, bool reverse, int startFrame) {
    if (m_paused) resume();
    if (m_current && !m_paused) {
        stop();
        m_queueHead = m_queueTail;  // the queue emptied
    }
    removeAllChildrenWithCleanup(false);
    detachAll();
    if (!m_props.count(name)) logError("Animation: no animation %s", name);
    m_current = &m_props[name];
    // the counter one before the first frame shown (update steps it first).
    // Backwards it starts at length, so a backwards pass shows length-1..1
    // and never its last frame (the original's, kept: one frame of the one
    // backwards use, a creature's "out")
    if (reverse) m_frame = startFrame != -1 ? m_current->length - startFrame - 1 : m_current->length;
    else m_frame = startFrame != -1 ? startFrame - 1 : 0;
    m_reverse = reverse;
    m_loopsLeft = loops != -1 ? loops - 1 : -1;
    m_addPending = true;
    update(0);
    schedule(schedule_selector(Animation::update), 1.0f / static_cast<float>(m_frameRate));
}

void Animation::resume() {
    if (!m_paused) return;
    m_paused = false;
    schedule(schedule_selector(Animation::update), 1.0f / static_cast<float>(m_frameRate));
}

bool Animation::isPlaying() { return m_current && !m_paused; }

void Animation::queue(const char* name, int loops, bool reverse) {
    int next = m_queueTail + 1;
    if (next > kQueueSize - 1) next = 0;
    if (next == m_queueHead) {
        logError("The animation queue is full. Consider incrementing AnimationConstants::QueueMaxSize");
        return;
    }
    // what plays for ever ends at its end, for this one
    if (m_loopsLeft == -1) m_loopsLeft = 0;
    m_queue[next].name = name;
    m_queue[next].loops = loops;
    m_queue[next].reverse = reverse;
    m_queueTail = next;
}

void Animation::pause() {
    if (!m_current || m_paused) return;
    m_paused = true;
    unschedule(schedule_selector(Animation::update));
}

const char* Animation::currentName() { return m_current ? m_current->name.c_str() : nullptr; }

int Animation::lengthOf(const char* name) {
    const Prop& p = m_props[name];
    int longest = 0;
    for (const Timeline& t : p.timelines) {
        int total = 0;
        for (const Segment& s : t.segments) total += s.total;
        longest = std::max(longest, total);
    }
    return longest;
}

CCSize Animation::getSpriteSize() {
    // (the original read the first clip's sprite unchecked)
    if (m_clips.empty() || !m_clips[0]->sprite) return CCSizeZero;
    return m_clips[0]->sprite->getContentSize();
}

void Animation::attach(const char* controlPoint, CCNode* node) {
    m_attachments.push_back(Attachment{controlPoint, node});
    if (!m_current) return;
    for (Timeline& t : m_current->timelines) {
        Clip* c = t.clip;
        for (size_t i = 0; i < c->controlPoints.size() && i < c->controlNodes.size(); ++i)
            // (the original added it again if two clips showing had that point)
            if (c->controlPoints[i].name == controlPoint && !node->getParent()) c->controlNodes[i]->addChild(node);
    }
}

void Animation::setColor(const ccColor3B& color) {
    for (auto& c : m_clips)
        if (c->sprite) c->sprite->setColor(color);
}

void Animation::setOpacity(float opacity) {
    for (auto& c : m_clips)
        if (c->sprite) c->sprite->setOpacity(static_cast<GLubyte>(static_cast<int>(opacity)));
}

void Animation::setFlipX(bool flip) {
    if (m_flipX == flip) return;
    m_flipX = flip;
    for (auto& c : m_clips)
        if (c->sprite) c->sprite->setFlipX(flip);
}

void Animation::update(float) {
    if (!m_current) return;
    m_frame += m_reverse ? -1 : 1;
    if ((m_reverse && m_frame < 1) || (!m_reverse && m_frame > m_current->length)) {
        // the end of a pass: again, the queued one, or stop
        if (m_loopsLeft >= 1) {
            --m_loopsLeft;
        } else if (m_loopsLeft == 0) {
            if (m_queueHead == m_queueTail) {
                stop();
                return;
            }
            int next = m_queueHead + 1;
            if (next > kQueueSize - 1) next = 0;
            Queued q = m_queue[next];
            m_queueHead = next;
            stop();
            play(q.name.c_str(), q.loops, q.reverse, -1);
            return;
        }
        m_frame = m_reverse ? m_current->length - 1 : 1;
    }
    const float s = ResolutionMgr::sharedResolutionMgr()->canvasScale;
    for (Timeline& t : m_current->timelines) {
        Clip* c = t.clip;
        if (!c->sprite) continue;  // (not prepared: the original failed)
        if (m_addPending) {
            // the clips of this animation shown, their control points above
            // them, the attached nodes on their points
            int z = c->depth * 10;
            if (!c->sprite->getParent()) addChild(c->sprite, z);
            for (size_t i = 0; i < c->controlNodes.size(); ++i) {
                if (!c->controlNodes[i]->getParent()) addChild(c->controlNodes[i], z | 1);
                for (Attachment& a : m_attachments)
                    if (a.controlPoint == c->controlPoints[i].name && !a.node->getParent())
                        c->controlNodes[i]->addChild(a.node);
            }
        }
        // the segment the counter is in (one played for ever takes the rest)
        int start = 0;
        for (const Segment& seg : t.segments) {
            if (m_frame - start <= seg.total || seg.count == -1) {
                int i = (m_frame - start - 1) % seg.length;
                if (seg.to < seg.from) i = -i;
                int f = seg.from + i - 1;
                // (the original threw on a frame the clip has not)
                if (f < 0 || f >= static_cast<int>(c->frames.size()) || f >= static_cast<int>(c->offsets.size()))
                    break;
                if (c->frames[f]) c->sprite->setDisplayFrame(c->frames[f]);
                CCPoint pos = ccp((static_cast<float>(c->x) + c->offsets[f].x) * s,
                                  (static_cast<float>(c->y) + c->offsets[f].y) * s);
                c->sprite->setPosition(pos);
                for (size_t k = 0; k < c->controlNodes.size(); ++k) {
                    const std::vector<CCPoint>& cp = c->controlPoints[k].pos;
                    if (f >= static_cast<int>(cp.size())) continue;
                    c->controlNodes[k]->setPosition(ccp(pos.x + cp[f].x * s, pos.y + cp[f].y * s));
                }
                break;
            }
            start += seg.total;
        }
    }
    m_addPending = false;
}

}  // namespace mt
