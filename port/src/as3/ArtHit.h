// (the port's) Hit tests on an object's art: whether a sprite's picture (its
// texture's alpha, CCSprite::opaqueAt) lies near a touch. The extractions
// take the tongs only where their art is (AngledExtraction, SurfaceExtraction
// pieceContains): the original's touch area was the whole picture's rect
// (0x77c4c), so an object mostly buried, or with a picture much wider than
// what shows (the whale's shark: a fin showing over a buried body as wide as
// the screen's middle), took touches meant for objects beside it and over it
// (the idols, the puffin), which then could not be taken.
#pragma once

#include "engine/cocos/CCGeometry.h"

namespace cocos2d {
class CCSprite;
}

namespace mt {

// The sprite's art within `radius` points of `at`, a point of the sprite's
// parent's space. With `above` given, only art at or above that height of
// the parent's space counts (an embedded object: what is above the skin).
// A sprite whose texture keeps no alpha counts its whole picture.
bool artNear(cocos2d::CCSprite* sprite, const cocos2d::CCPoint& at, float radius, const float* above = nullptr);

}  // namespace mt
