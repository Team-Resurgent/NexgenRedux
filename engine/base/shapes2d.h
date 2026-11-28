#pragma once

#include "base/math.h"
#include "base/types.h"

// Shapes on the plane, worked out rather than drawn: polygons cut into triangles, and whether they bulge outwards.
namespace nx
{
    // Cuts a polygon, either way round and concave too, into count - 2 triangles: three corner indices each, into
    // out, which has room for 3 * (count - 2). What is left of one whose sides cross is a fan. Returns how many.
    u32 triangulate(const Vec2* points, u32 count, u16* out);
    // Whether every corner turns the same way, so that a line between any two places inside stays inside.
    bool isConvex(const Vec2* points, u32 count);
}
