#pragma once
#include "gfx.h"

namespace fishgfx {
bool load(const std::string& dataDir);
// species index: 0..20 regular, 900..906 magic, 999 baby
// k = screen px per world unit; (x,y) = screen centre of the fish body; facingRight mirrors the art (art faces left)
void draw(int bodyType, int finType, int frame, float x, float y, float k, bool facingRight, float time, Color c = Color(), bool mouthOpen = false);
// approximate half extents of the fish in world units (for hit testing / layout)
void extents(int bodyType, float& hw, float& hh);
std::string speciesName(int type);
}
