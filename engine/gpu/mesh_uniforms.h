#pragma once

#include "gpu/gpu.h"

#include <math.h>
#include <string.h>

// The layout of the uniform blocks in mesh.hlsl and in the OpenGL shaders. Only vectors of four and
// matrices, so that both languages lay them out the same.
namespace nx::gpu
{
    struct ViewUniforms
    {
        Mat4 viewProjection, shadowMatrix;
        float eye[4], sunDirection[4], sunColor[4], ambientSky[4], ambientGround[4], fogColor[4], fogRange[4];
        float cameraRight[4], cameraUp[4], cameraForward[4], skyTop[4], skyHorizon[4];
        float lightPlace[kLights][4], lightColor[kLights][4];
    };

    struct ObjectUniforms
    {
        Mat4 world;
        float tint[4], surface[4];
        i32 lights[4];
        float wind[4];   // bend along x and z, cutout
    };

    struct FieldUniforms
    {
        float place[kFieldShapes][4], size[kFieldShapes][4], color[kFieldShapes][4], more[kFieldShapes][4];
        float info[4], info2[4];
    };

    inline void put(float (&out)[4], const Vec3& v, float w)
    {
        out[0] = v.x;
        out[1] = v.y;
        out[2] = v.z;
        out[3] = w;
    }

    inline void put(float (&out)[4], const Color& c, float w)
    {
        out[0] = c.r;
        out[1] = c.g;
        out[2] = c.b;
        out[3] = w;
    }

    // Where a place falls in the shadow map: from clip space to 0 to 1. Rows go down in D3D and up in
    // OpenGL, whose depth also starts at -1 until the shader's end.
    inline void fillView(ViewUniforms& u, const View& v, bool rowsGoUp)
    {
        memset(static_cast<void*>(&u), 0, sizeof(u));
        u.viewProjection = v.projection * v.view;
        Mat4 half;
        half.m[0] = 0.5f;
        half.m[5] = rowsGoUp ? 0.5f : -0.5f;
        half.m[12] = half.m[13] = 0.5f;
        u.shadowMatrix = half * v.shadows;
        put(u.eye, v.eye, v.time);
        put(u.sunDirection, v.sunDirection, v.shadowStrength);
        put(u.sunColor, v.sunColor, 1);
        put(u.ambientSky, v.ambientSky, 1);
        put(u.ambientGround, v.ambientGround, 1);
        put(u.fogColor, v.fogColor, 1);
        u.fogRange[0] = v.fogStart;
        u.fogRange[1] = v.fogEnd > v.fogStart ? 1 / (v.fogEnd - v.fogStart) : 0;
        put(u.cameraRight, v.right, 0);
        put(u.cameraUp, v.up, 0);
        put(u.cameraForward, v.forward, 0);
        put(u.skyTop, v.skyTop, 1);
        put(u.skyHorizon, v.skyHorizon, 1);
        for (u32 i = 0; i < v.lightCount && i < kLights; i++)
        {
            put(u.lightPlace[i], v.lights[i].place, v.lights[i].range > 0 ? 1 / v.lights[i].range : 1);
            put(u.lightColor[i], v.lights[i].color, 1);
        }
    }

    inline void fillObject(ObjectUniforms& u, const MeshDraw& d)
    {
        u.world = d.world;
        put(u.tint, d.color, d.color.a);
        u.surface[0] = d.glow;
        u.surface[1] = d.shine;
        u.surface[2] = d.lit ? d.indoors ? 2.0f : 1.0f : 0.0f;
        u.surface[3] = d.fogged ? 1.0f : 0.0f;
        u.wind[0] = d.bendX;
        u.wind[1] = d.bendZ;
        u.wind[2] = d.cutout;
        u.wind[3] = 0;
        for (u32 i = 0; i < 4; i++)
        {
            u.lights[i] = i < d.lightCount ? d.lights[i] : -1;
        }
    }

    inline void fillField(FieldUniforms& u, const Field& f)
    {
        memset(&u, 0, sizeof(u));
        const u32 count = f.count < kFieldShapes ? f.count : kFieldShapes;
        for (u32 i = 0; i < count; i++)
        {
            const FieldShape& s = f.shapes[i];
            put(u.place[i], s.place, static_cast<float>(static_cast<u32>(s.kind) + 8 * static_cast<u32>(s.operation)));
            put(u.size[i], s.size, s.soft);
            put(u.color[i], s.color, 1);
            u.more[i][0] = cosf(s.turn);
            u.more[i][1] = sinf(s.turn);
            // Of a ball around it, and around where it melts.
            float radius = s.size.x;
            switch (s.kind)
            {
            case FieldKind::Box: radius = s.size.length() + 0.03f; break;
            case FieldKind::Torus:
            case FieldKind::Capsule: radius = s.size.x + s.size.y; break;
            case FieldKind::Cylinder: radius = sqrtf(s.size.x * s.size.x + s.size.y * s.size.y); break;
            default: break;
            }
            u.more[i][2] = radius + s.soft + 0.02f;
        }
        u.info[0] = static_cast<float>(count);
        u.info[1] = static_cast<float>(f.steps < 1 ? 1 : f.steps > 160 ? 160 : f.steps);
        u.info[2] = f.farthest;
        u.info[3] = f.repeat;
        u.info2[0] = f.shine;
        u.info2[1] = f.reflection;
    }
}
