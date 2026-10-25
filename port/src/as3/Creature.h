// What the creature injuries share (Bat, MiniBat, the frogs, Flea, IceAnt,
// Leech, Snake, FireMaggot, RobotScorpion, the worms). Each is its own
// InjuryState subclass, as in the original; these are the base class's
// helpers they call (their addresses given) and a few of the port's own.
//
// A creature is drawn by the usual injury pictures (0x77c4c): its stage's
// animation (`common/anim_creature_*_anim.json`, InjuryState::animations[0])
// at its place. Its vf3 (called when the pictures are built) starts it, and
// its vf4 (each frame, after the stage's timers) moves it: the creature's
// injury follows the animation node or the other way round.
#pragma once

#include <string>

#include "as3/Animation.h"
#include "as3/InjuryState.h"

namespace mt {
namespace creature {

// The creatures' states (the state enum's values, kStateNames' order).
enum CreatureStateKind {
    kBatHanging = 9, kBatFlying, kBatDied, kMiniBatFlying, kGrowing, kPoisonAreaIdle, kPoisonAreaEmitting,
    kSoakableParticleIdle, kPassiveParticleIdle, kSucking, kHidden, kFleaJumping, kFleaDead,
    kRobotScorpionEmerging, kRobotScorpionDashing, kRobotScorpionBurying, kRobotScorpionTurning,
    kRobotScorpionSubmerged, kRobotScorpionCharging, kRobotScorpionFiring, kRobotScorpionTazered,
    kRobotScorpionClamped, kRobotScorpionExploding, kRobotScorpionDead, kBroken, kStage1, kStage2, kStage3,
    kClamped, kSnakeInHole, kSnakeOut, kFireEmitting, kPoisonFrogIdle, kPoisonFrogDead, kPoisonFrogHurt,
    kPoisonFrogShocked, kPoisonFrogWalking, kPoisonFrogAngry, kPoisonFrogFiring, kPoisonFrogLooking,
    kFireMaggotCalmed, kFireMaggotAngry, kFireMaggotDead, kIceAntWalking, kIceAntDead, kIceAntBite, kIceAntTazer,
    kBlackFrogNormal, kBlackFrogAngry, kBlackFrogDead, kMiniFrogEggClosed, kMiniFrogEggClamped, kMiniFrogNormal,
    kMiniFrogAngry, kMiniFrogDead, kShut, kMiniWormAlive, kMiniWormBite, kMiniWormMove, kMiniWormLatched,
    kMiniWormDangling, kMiniWormShocked, kGiantWormAlive, kGiantWormDead, kGiantWormLookingAround,
    kGiantWormMoving, kGiantWormSpittingAtScreen, kGiantWormSpittingAtPatient, kGiantWormGoingUnder,
    kGiantWormUnderSkin, kGiantWormReact, kGiantWormWrapChest, kIncisionShut,
    // the Hospital's (Android 1.43: its own numbers 0x38.. differ)
    kSpiderWalking, kSpiderDead, kSpiderBite
};

// the canvas the game is laid out on (canvas points)
constexpr float kCanvasW = 640, kCanvasH = 384;

// The factory's (0x908c4) creature cases: the subclass of a creature base
// class with its tuning (its constructor), nullptr for any other base.
InjuryState* make(int base);

// lo..hi, both included (0x644b0)
int randInt(int lo, int hi);
// lo..hi (0x644cc)
float randFloat(float lo, float hi);

// the touch in the injury's rect turned with it (0x799f0)
bool touchInRect(const InjuryState& s, const cocos2d::CCPoint& touch);
// the same for another rect turned with the injury about its place
bool touchInRect(const InjuryState& s, const cocos2d::CCPoint& touch, const cocos2d::CCRect& r);
// the touch strictly inside the injury's rect, not turned (the flea's own test)
bool touchInPlainRect(const InjuryState& s, const cocos2d::CCPoint& touch);

// (debug: AS_TRACE_CREATURES=1) every tenth frame, "creature #<id> <class>
// at x,y" (cocos points), for test scripts to follow a moving creature
void tracePosition(const InjuryState& s);

// the injury moved (0x84e20): its rect, its hit points, its nodes (vf30)
void moveTo(InjuryState& s, const cocos2d::CCPoint& p);

// its stage's animation (InjuryState::animations[0]), or nullptr
Animation* anim(InjuryState& s);
// where its animation is on the screen: the piece's sprite's place once
// the tongs have had it (grabPiece; the original's node was the animation
// itself, so its place moved with the tongs)
cocos2d::CCPoint animPlace(InjuryState& s, Animation* a);
// the name it plays, "" if none (0x5abf0)
std::string playing(Animation* a);

// canvas points (640x384, as the creatures' paths are given) and the
// screen's cocos points
cocos2d::CCPoint toCanvas(const cocos2d::CCPoint& screen);
cocos2d::CCPoint fromCanvas(const cocos2d::CCPoint& canvas);
// a random point on a side of the play area (ResolutionMgr's rect, canvas
// points), not the side of last time (the bats' 0x184250, 0x184bdc)
cocos2d::CCPoint pointOnPlayAreaSide(int& lastSide);

// an injury of `className` made at (x, y) turned `rotation` (event 2)
// a place `distance` canvas points (give or take spread/2) from `from`
// (cocos points) in a direction that stays in the play area: sideways if
// there is room up and down, up if no room down, down if none up; left or
// right as there is room, at random if both (0x905f4: the frogs' hops)
cocos2d::CCPoint hopTarget(const cocos2d::CCPoint& from, int distance, int spread);

// the way from `from` to `to` (a unit vector into `dir`) and its turn in
// degrees, 0 up and clockwise (the frogs' 0x159598, 0x152d08)
float headingTo(const cocos2d::CCPoint& from, const cocos2d::CCPoint& to, cocos2d::CCPoint& dir);

InjuryEvent createEvent(const std::string& className, float x, float y, float rotation);
// the patient's damage (event 9)
InjuryEvent damageEvent(int damage);
// a completed action for the combo: its points, or a bonus for an injury
// made during the surgery (HitResult +0x15/+0x16)
void giveCombo(HitResult& r, const InjuryState& s);

// What the tongs take of a dead creature: its animation in a sprite of its
// own (pieces[0].sprite), made the first time it is asked. The original
// gave the screen the animation node itself as the piece's sprite; the
// port's pieces hold sprites, so the animation goes into an empty sprite
// at its place, the sprite turned as the animation was (the original's node
// kept its turn in the tongs).
PieceState* grabPiece(InjuryState& s);
// turns what shows the creature: its piece's sprite once the tongs have had
// it (grabPiece), else its animation
void setAnimRotation(InjuryState& s, float degrees);

// the creature's piece of the tongs (0xb bat, 0x12 leech, 0x15 flea...):
// the def's first piece slot written over (kind, rect at (100, 100) of the
// injury's size) and added to its pieces (the creatures' start)
void addCreaturePiece(InjuryState& s, int kind);

}  // namespace creature
}  // namespace mt
