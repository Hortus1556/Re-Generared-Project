// A Flash shape as an SVG file (the game packages' vector/shapes/): valid
// SVG that any browser or editor shows, in a fixed dialect that reads back
// to exactly the same shape.
//
//   <svg viewBox (pixels) data-bounds="xMin yMin xMax yMax" (twips)>
//   <defs> one gradient per gradient style: userSpaceOnUse, the Flash
//          square -819.2..819.2 px, gradientTransform, spreadMethod, stops
//   <g data-layer="n">  one per style layer (styles changed mid-shape)
//     <path data-fill="i" fill=... fill-rule="evenodd" d=.../>   in drawing order
//     <path data-line="i" fill="none" stroke=... d=.../>
//   A style no path uses is kept as a path with an empty d.
//
// Path coordinates are pixels with two decimals: exactly the twips (1/20
// pixel) of the shape. Moves, lines and quadratic curves only (M, L, Q).
#pragma once

#include <string>

#include "flash/Swf.h"

namespace flash {

std::string shapeToSvg(const Shape& shape);
// False (and `error` set) if the text is not in the dialect.
bool shapeFromSvg(const std::string& svg, Shape& shape, std::string* error = nullptr);

}  // namespace flash
