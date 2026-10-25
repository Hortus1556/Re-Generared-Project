#include "as3/Creature.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "as3/Bat.h"
#include "as3/BlackFrog.h"
#include "as3/FireMaggot.h"
#include "as3/Flea.h"
#include "as3/Ghost.h"
#include "as3/GiantWorm.h"
#include "as3/MiniWorm.h"
#include "as3/IceAnt.h"
#include "as3/Leech.h"
#include "as3/Jellyfish.h"
#include "as3/Locust.h"
#include "as3/MiniBat.h"
#include "as3/MiniFrog.h"
#include "as3/PoisonFrog.h"
#include "as3/Pygmy.h"
#include "as3/Log.h"
#include "as3/ResolutionMgr.h"
#include "as3/RobotScorpion.h"
#include "as3/Snake.h"
#include "as3/Spider.h"
#include "engine/cocos/CCDirector.h"
#include "platform/Platform.h"

namespace mt {
namespace creature {

using namespace cocos2d;

namespace {
constexpr float kPi = 3.14159265358979f;
}

InjuryState* make(int base) {
    switch (base) {
    case kBat:
        return new Bat;
    case kMiniBat:
        return new MiniBat;
    case kFlea:
        return new Flea;
    case kLeech:
        return new Leech;
    case kSnake:
        return new Snake;
    case kIceAnt:
        return new IceAnt;
    case kMiniFrog:
        return new MiniFrog;
    case kBlackFrog:
        return new BlackFrog;
    case kPoisonFrog:
        return new PoisonFrog;
    case kFireMaggot:
        return new FireMaggot;
    case kRobotScorpion:
        return new RobotScorpion;
    case kMiniWorm: return new MiniWorm;
    case kGiantWorm: return new GiantWorm;
    case kSpider: return new Spider;  // (Android 1.43: InjuryCommon::createInjuryStateForEnum)
    case kGhost: return new Ghost;    // (Android 1.43: the same, 0x1d)
    case kLocust: return new Locust;        // (iOS 1.45 0xb8838: 0xc)
    case kJellyfish: return new Jellyfish;  // (iOS 1.45: 0xd)
    case kPygmy: return new Pygmy;          // (iOS 1.45: 0x27)
    default:
        return nullptr;
    }
}

int randInt(int lo, int hi) {
    if (hi < lo) return lo;
    return lo + std::rand() % (hi - lo + 1);
}

float randFloat(float lo, float hi) {
    return lo + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) / (hi - lo));
}

bool touchInRect(const InjuryState& s, const CCPoint& touch) { return touchInRect(s, touch, s.rect); }

bool touchInRect(const InjuryState& s, const CCPoint& touch, const CCRect& r) {
    // (0x799f0) the touch's offset from the injury (whole points) turned back
    // by its turn, inside its rect (strictly)
    const float a = static_cast<float>(static_cast<int>(s.rotation)) * kPi / 180.0f;
    float dx = static_cast<float>(static_cast<int>(touch.x - s.position.x));
    float dy = static_cast<float>(static_cast<int>(touch.y - s.position.y));
    float x = s.position.x + (dx * std::cos(a) - dy * std::sin(a));
    float y = s.position.y + (dx * std::sin(a) + dy * std::cos(a));
    return r.origin.x < x && x < r.origin.x + r.size.width && r.origin.y < y && y < r.origin.y + r.size.height;
}

bool touchInPlainRect(const InjuryState& s, const CCPoint& touch) {
    const CCRect& r = s.rect;
    return r.origin.x < touch.x && touch.x < r.origin.x + r.size.width && r.origin.y < touch.y &&
           touch.y < r.origin.y + r.size.height;
}

void tracePosition(const InjuryState& s) {
    static const bool on = platform::debugMode() && std::getenv("AS_TRACE_CREATURES") != nullptr;
    if (!on) return;
    // (every 5th frame, counted by the director: the first creature traced
    // may be gone or on a stage not shown)
    // (its turn as drawn: the animation's, in the tongs' sprite once taken)
    if (cocos2d::CCDirector::sharedDirector()->getTotalFrames() % 5 == 0) {
        float turn = 0;
        if (Animation* a = anim(const_cast<InjuryState&>(s))) {
            turn = a->getRotation();
            if (!s.pieces.empty() && s.pieces[0].sprite && a->getParent() == s.pieces[0].sprite)
                turn += s.pieces[0].sprite->getRotation();
        }
        log("creature #%d %s at %.0f,%.0f rot %.0f", s.soundChannel, s.def->name.c_str(), s.position.x, s.position.y,
            turn);
    }
}

void moveTo(InjuryState& s, const CCPoint& p) {
    // (0x84e20; its rect as 0x84e44)
    s.position = p;
    s.rect = CCRectMake(p.x - s.def->size.width * 0.5f, p.y - s.def->size.height * 0.5f, s.def->size.width,
                        s.def->size.height);
    s.placeHitPoints();
    s.placeNodes();
}

Animation* anim(InjuryState& s) {
    if (!s.animations.empty()) return dynamic_cast<Animation*>(s.animations[0]);
    // (taken into its piece's sprite for the tongs: grabPiece)
    if (!s.pieces.empty() && s.pieces[0].sprite && s.pieces[0].sprite->getChildrenCount() > 0)
        return dynamic_cast<Animation*>(static_cast<CCNode*>(s.pieces[0].sprite->getChildren()->objectAtIndex(0)));
    return nullptr;
}

CCPoint animPlace(InjuryState& s, Animation* a) {
    if (!s.pieces.empty() && s.pieces[0].sprite && a && a->getParent() == s.pieces[0].sprite)
        return s.pieces[0].sprite->getPosition();
    return a ? a->getPosition() : s.position;
}

std::string playing(Animation* a) {
    if (!a) return std::string();
    const char* n = a->currentName();
    return n ? std::string(n) : std::string();
}

CCPoint toCanvas(const CCPoint& screen) { return canvasPosition(screen); }

CCPoint fromCanvas(const CCPoint& c) {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    return ccp((c.x - (kCanvasW - r->width / s) * 0.5f) * s, (c.y - (kCanvasH - r->height / s) * 0.5f) * s);
}

CCPoint pointOnPlayAreaSide(int& lastSide) {
    // (0x184250) a side other than the last: 0 the top, 1 the bottom, 2 the
    // left, 3 the right, at a random place along it (whole points)
    int side;
    do {
        side = randInt(0, 3);
    } while (side == lastSide);
    lastSide = side;
    const CCRect& a = ResolutionMgr::sharedResolutionMgr()->rect;
    const float l = a.origin.x, b = a.origin.y, w = a.size.width, h = a.size.height;
    switch (side) {
    case 0:
        return ccp(static_cast<float>(randInt(static_cast<int>(l), static_cast<int>(l + w))), b + h);
    case 1:
        return ccp(static_cast<float>(randInt(static_cast<int>(l), static_cast<int>(l + w))), b);
    case 2:
        return ccp(l, static_cast<float>(randInt(static_cast<int>(b), static_cast<int>(b + h))));
    default:
        return ccp(l + w, static_cast<float>(randInt(static_cast<int>(b), static_cast<int>(b + h))));
    }
}

CCPoint hopTarget(const CCPoint& from, int distance, int spread) {
    const ResolutionMgr* r = ResolutionMgr::sharedResolutionMgr();
    const float s = r->canvasScale;
    const float d = static_cast<float>(static_cast<int>(static_cast<float>(distance) * s +
                                                        s * static_cast<float>(randInt(-(spread / 2), spread / 2))));
    const float mx = (kCanvasW - r->width / s) * 0.5f, my = (kCanvasH - r->height / s) * 0.5f;
    const CCRect& a = r->rect;
    const float top = s * (a.origin.y + a.size.height - my), bottom = s * (a.origin.y - my);
    const float left = s * (a.origin.x - mx), right = s * (a.origin.x + a.size.width - mx);
    const bool canLeft = left <= from.x - d, canRight = from.x + d <= right;
    int lo, hi;
    if (from.y + d <= top) {
        if (bottom <= from.y - d) {  // sideways
            if (canLeft && canRight) {
                if (randInt(0, 1) == 0) lo = 270, hi = 450;
                else lo = 90, hi = 270;
            } else if (canLeft) {
                lo = 90, hi = 270;
            } else {
                lo = 270, hi = 450;
            }
        } else if (canLeft && canRight) {  // up
            if (randInt(0, 1) == 0) lo = 90, hi = 180;
            else lo = 0, hi = 90;
        } else if (canLeft) {
            lo = 90, hi = 180;
        } else {
            lo = 0, hi = 90;
        }
    } else if (canLeft && canRight) {  // down
        if (randInt(0, 1) == 0) lo = 270, hi = 360;
        else lo = 180, hi = 270;
    } else if (canLeft) {
        lo = 180, hi = 270;
    } else {
        lo = 270, hi = 360;
    }
    const float ang = static_cast<float>(randInt(lo, hi)) * kPi / 180.0f;
    return ccp(from.x + d * std::cos(ang), from.y + d * std::sin(ang));
}

float headingTo(const CCPoint& from, const CCPoint& to, CCPoint& dir) {
    float dx = to.x - from.x, dy = to.y - from.y, len = std::sqrt(dx * dx + dy * dy);
    dir = len > 0 ? ccp(dx / len, dy / len) : ccp(0, 1);
    float a = std::acos(std::max(-1.0f, std::min(1.0f, dir.y))) * 180.0f / kPi;
    if (dir.x < 0 && a != 0) a = 360.0f - a;
    return a;
}

InjuryEvent createEvent(const std::string& className, float x, float y, float rotation) {
    InjuryEvent e;
    e.type = InjuryEvent::kCreateInjury;
    e.name = className;
    e.x = std::floor(x);
    e.y = std::floor(y);
    e.rotation = static_cast<float>(static_cast<int>(rotation));
    return e;
}

InjuryEvent damageEvent(int damage) {
    InjuryEvent e;
    e.type = InjuryEvent::kDamage;
    e.damage = damage;
    return e;
}

void giveCombo(HitResult& r, const InjuryState& s) {
    if (!s.createdLater) r.comboAction = true;
    else r.comboBonus = true;
}

PieceState* grabPiece(InjuryState& s) {
    if (s.pieces.empty()) return nullptr;
    PieceState& p = s.pieces[0];
    Animation* a = anim(s);
    if (!p.sprite && a) {
        // (the port's: see the header) the sprite at the animation's place
        // and turn, the animation at its origin in it, unturned. (Fixed: the
        // port's first version left the sprite unturned, so a creature
        // turned its own way, a spider, an ant, snapped upright as the tongs
        // lifted it.)
        CCSprite* holder = CCSprite::create();
        holder->setPosition(a->getPosition());
        holder->setRotation(a->getRotation());
        a->retain();
        CCNode* parent = a->getParent();
        int z = a->getZOrder();
        a->removeFromParentAndCleanup(false);
        a->setPosition(CCPointZero);
        a->setRotation(0);
        holder->addChild(a);
        a->release();
        if (parent) parent->addChild(holder, z);
        p.sprite = holder;
        p.grabOffset = CCPointZero;
        // (no longer one of the injury's nodes it places: vf30)
        s.animations.erase(s.animations.begin());
    }
    return &p;
}

void setAnimRotation(InjuryState& s, float degrees) {
    Animation* a = anim(s);
    if (!s.pieces.empty() && s.pieces[0].sprite) {
        s.pieces[0].sprite->setRotation(degrees);
        if (a) a->setRotation(0);
    } else if (a) {
        a->setRotation(degrees);
    }
}

void addCreaturePiece(InjuryState& s, int /*kind*/) {
    PieceState p;
    p.def = nullptr;
    p.rect = CCRectMake(100, 100, s.def->size.width, s.def->size.height);
    s.pieces.push_back(p);
}

}  // namespace creature
}  // namespace mt
