#pragma once

#include "base/grow_array.h"
#include "base/math.h"
#include "gpu/gpu.h"

namespace nx
{
    // Triangles being made, on the CPU.
    struct MeshData : NonCopyable
    {
        GrowArray<gpu::MeshVertex, MemTag::General> vertices;
        GrowArray<u16, MemTag::General> indices;

        void clear()
        {
            vertices.clear();
            indices.clear();
        }

        // False once there is no room: a mesh has up to 65536 vertices.
        bool vertex(const Vec3& place, const Vec3& normal, const gpu::Color& color, float u, float v);
        bool triangle(u32 a, u32 b, u32 c);
        // Four corners, counter-clockwise seen from the front.
        bool quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const gpu::Color& color);

        void box(const Vec3& middle, const Vec3& half, const gpu::Color& color);
        void sphere(const Vec3& middle, float radius, u32 around, const gpu::Color& color);
        // From bottom to top; a radius of 0 makes a cone.
        void cylinder(const Vec3& bottom, float height, float bottomRadius, float topRadius, u32 around, const gpu::Color& color);
        void capsule(float radius, float halfHeight, u32 around, const gpu::Color& color);
        void torus(float ring, float tube, u32 around, const gpu::Color& color);
        // Flat, facing up, in squares.
        void plane(float halfWidth, float halfDepth, u32 squares, const gpu::Color& color);

        // Moves and turns everything from a vertex on.
        void transform(u32 firstVertex, const Mat4& by);
        // Smooth over shared vertices.
        void computeNormals();
        void bounds(Vec3& middle, float& radius) const;
    };

    // A simpler copy: vertices in the same cell of a grid become one. The grid has this many cells along
    // the longest side.
    void simplify(const gpu::MeshVertex* vertices, u32 vertexCount, const u16* indices, u32 indexCount, u32 cells, MeshData& out);

    gpu::MeshVertex meshVertex(const Vec3& place, const Vec3& normal, const gpu::Color& color, float u, float v);
}
