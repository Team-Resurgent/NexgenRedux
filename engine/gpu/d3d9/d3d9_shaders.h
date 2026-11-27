#pragma once

// The Xbox 360's shaders: HLSL for shader model 3, compiled when the device starts. The constants are laid out as
// ViewUniforms (c0 to c51) and the object's (c52 on), in gpu/mesh_uniforms.h, so one upload serves every shader.
namespace nx::gpu::shaders360
{
    // 2D: a coloured, textured triangle.
    inline const char kFlat[] = R"(
float4x4 transform : register(c0);
sampler2D image : register(s0);

struct FlatIn
{
    float3 position : POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct FlatOut
{
    float4 position : POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct FlatPixel
{
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

FlatOut flatVs(FlatIn input)
{
    FlatOut output;
    output.position = mul(transform, float4(input.position, 1.0));
    output.color = input.color.wzyx;  // the bytes of each colour arrive the other way round
    output.uv = input.uv;
    return output;
}

float4 flatPs(FlatPixel input) : COLOR0
{
    return tex2D(image, input.uv) * input.color;
}
)";

    // What meshes, the sky and effects on things share.
    inline const char kMeshCommon[] = R"(
float4x4 viewProjection : register(c0);
float4x4 shadowMatrix : register(c4);   // world to the shadow map: 0 to 1 across, down and deep
float4 eye : register(c8);              // w: time
float4 sunDirection : register(c9);     // towards the sun; w: how dark shadows are, 0 for none
float4 sunColor : register(c10);
float4 ambientSky : register(c11);
float4 ambientGround : register(c12);
float4 fogColor : register(c13);
float4 fogRange : register(c14);        // start, 1 / length
float4 cameraRight : register(c15);     // half the width of the picture at distance 1
float4 cameraUp : register(c16);
float4 cameraForward : register(c17);
float4 skyTop : register(c18);
float4 skyHorizon : register(c19);

float4x4 world : register(c52);
float4 tint : register(c56);
float4 surface : register(c57);         // glow, shine, lit, fogged
float4 wind : register(c59);            // bend along x and z for each metre of v; cutout
float4 volume : register(c60);          // offset of the volume, and its scale; 0 for none
float4 objectLightPlace[4] : register(c61);  // this mesh's lights; w: 1 / range, 0 for none
float4 objectLightColor[4] : register(c65);
float4 shadowInfo : register(c69);      // whether there is a shadow map, and the size of a texel of it

sampler2D image : register(s0);
sampler2D shadowMap : register(s1);
sampler3D volumeImage : register(s2);

struct MeshIn
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct MeshPixel
{
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float3 place : TEXCOORD1;
    float3 normal : TEXCOORD2;
};

struct MeshOut
{
    float4 position : POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float3 place : TEXCOORD1;
    float3 normal : TEXCOORD2;
};

MeshOut meshVsWith(MeshIn input)
{
    MeshOut output;
    float4 place = mul(world, float4(input.position, 1.0));
    // Bent by the wind, more the higher the vertex is: v says how high, in metres. Gusts roll along the way it blows.
    if (wind.x != 0.0 || wind.y != 0.0)
    {
        float along = dot(place.xz, normalize(wind.xy)) * 0.12;
        float gust = sin(eye.w * 1.3 - along) * 0.55 + sin(eye.w * 2.9 - along * 1.7 + place.x * 0.3) * 0.2 + 0.35;
        place.xz += wind.xy * (input.uv.y * gust);
    }
    output.position = mul(viewProjection, place);
    output.place = place.xyz;
    output.normal = mul((float3x3)world, input.normal);
    output.color = input.color.wzyx * tint;  // the bytes of each colour arrive the other way round
    output.uv = input.uv;
    return output;
}

float shadowAt(float3 place, float facing)
{
    if (sunDirection.w <= 0.0 || shadowInfo.x <= 0.0)
    {
        return 1.0;
    }
    float3 at = mul(shadowMatrix, float4(place, 1.0)).xyz;
    if (any(at < 0.0) || any(at > 1.0))
    {
        return 1.0;
    }
    float sum = 0.0;
    float bias = 0.0012 + 0.003 * (1.0 - facing);  // a coarser map than elsewhere
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            sum += tex2Dlod(shadowMap, float4(at.xy + float2(x, y) * shadowInfo.y, 0.0, 0.0)).r >= at.z - bias ? 1.0 : 0.0;
        }
    }
    // Fading out at the edge of the map, so its end isn't a line.
    float2 edge = abs(at.xy * 2.0 - 1.0);
    float fade = saturate((max(edge.x, edge.y) - 0.85) / 0.15);
    return lerp(1.0 - sunDirection.w, 1.0, max(sum / 9.0, fade));
}

float3 shade(float3 albedo, float3 normal, float3 place, float shine, float shadow)
{
    float3 toEye = normalize(eye.xyz - place);
    float3 light = lerp(ambientGround.rgb, ambientSky.rgb, normal.y * 0.5 + 0.5);
    float facing = saturate(dot(normal, sunDirection.xyz));
    light += sunColor.rgb * (facing * shadow);
    float3 result = albedo * light;
    if (shine > 0.0)
    {
        float3 half3 = normalize(sunDirection.xyz + toEye);
        result += sunColor.rgb * (pow(saturate(dot(normal, half3)), 8.0 + shine * 120.0) * shine * shadow * step(0.001, facing));
    }
    return result;
}

float3 sky(float3 direction)
{
    float up = max(direction.y + 0.08, 0.0);
    float3 color = lerp(skyHorizon.rgb, skyTop.rgb, 1.0 - exp(-3.6 * up * up / (up + 0.2)));
    float sun = saturate(dot(direction, sunDirection.xyz));
    color += sunColor.rgb * (pow(sun, 900.0) * 2.0 + pow(sun, 24.0) * 0.18);
    return color;
}

float4 finish(MeshPixel input, float4 base, float glow)
{
    float3 color = base.rgb;
    if (surface.z > 0.5)
    {
        float3 normal = normalize(input.normal);
        float shadow = shadowAt(input.place, saturate(dot(normal, sunDirection.xyz)));
        shadow = surface.z > 1.5 ? 0.0 : shadow;
        color = shade(base.rgb, normal, input.place, surface.y, shadow);
        for (int i = 0; i < 4; i++)
        {
            float4 lightPlace = objectLightPlace[i];
            if (lightPlace.w > 0.0)
            {
                float3 way = lightPlace.xyz - input.place;
                float far = length(way);
                float reach = saturate(1.0 - far * lightPlace.w);
                color += base.rgb * objectLightColor[i].rgb * (saturate(dot(normal, way / max(far, 0.0001))) * reach * reach);
            }
        }
    }
    color = lerp(color, base.rgb, glow);
    if (surface.w > 0.5)
    {
        float fog = saturate((distance(eye.xyz, input.place) - fogRange.x) * fogRange.y);
        color = lerp(color, fogColor.rgb, fog);
    }
    return float4(color, base.a);
}
)";

    inline const char kMeshMain[] = R"(
MeshOut meshVs(MeshIn input)
{
    return meshVsWith(input);
}

float4 meshPs(MeshPixel input) : COLOR0
{
    float4 base = volume.w > 0.0 ? tex3D(volumeImage, input.place * volume.w + volume.xyz) * input.color : tex2D(image, input.uv) * input.color;
    clip(base.a - wind.z);
    return finish(input, base, surface.x);
}

// What casts shadows: only how far it is from the sun.
struct ShadowOut
{
    float4 position : POSITION;
    float2 depth : TEXCOORD0;
};

ShadowOut shadowVs(MeshIn input)
{
    ShadowOut output;
    output.position = mul(viewProjection, mul(world, float4(input.position, 1.0)));
    output.depth = output.position.zw;
    return output;
}

float4 shadowPs(float2 depth : TEXCOORD0) : COLOR0
{
    return float4(depth.x / depth.y, 0.0, 0.0, 1.0);
}

// A triangle over the whole screen, as far away as can be: the corners say where it is, -1 to 1.
struct ScreenOut
{
    float4 position : POSITION;
    float2 at : TEXCOORD0;
};

ScreenOut screenVs(float2 corner : POSITION)
{
    ScreenOut output;
    output.position = float4(corner, 1.0, 1.0);
    output.at = corner;
    return output;
}

float3 rayOf(float2 at)
{
    return normalize(cameraForward.xyz + cameraRight.xyz * at.x + cameraUp.xyz * at.y);
}

float4 skyPs(float2 at : TEXCOORD0) : COLOR0
{
    return float4(sky(rayOf(at)), 1.0);
}
)";
    // Over the picture: what was drawn, made brighter, blurred, added, graded. The corners say where, -1 to 1.
    inline const char kPost[] = R"(
float4 post[4] : register(c0);
sampler2D source : register(s0);
sampler3D table : register(s2);

struct PostOut
{
    float4 position : POSITION;
    float2 at : TEXCOORD0;
};

PostOut postVs(float2 corner : POSITION)
{
    PostOut output;
    output.position = float4(corner, 0.0, 1.0);
    output.at = corner;
    return output;
}

float2 uvOf(float2 at)
{
    return float2(at.x * 0.5 + 0.5, 0.5 - at.y * 0.5);
}

// Smaller, and only what is brighter than post[0].z. post[0].xy: a pixel of what is read.
float4 brightPs(float2 at : TEXCOORD0) : COLOR0
{
    float2 uv = uvOf(at);
    float2 t = post[0].xy;
    float3 c = (tex2D(source, uv + t * float2(-1.0, -1.0)).rgb + tex2D(source, uv + t * float2(1.0, -1.0)).rgb +
                tex2D(source, uv + t * float2(-1.0, 1.0)).rgb + tex2D(source, uv + t * float2(1.0, 1.0)).rgb) * 0.25;
    float bright = max(c.r, max(c.g, c.b));
    return float4(c * smoothstep(post[0].z, post[0].z + 0.3, bright), 1.0);
}

// Spread along post[0].xy.
float4 blurPs(float2 at : TEXCOORD0) : COLOR0
{
    float2 uv = uvOf(at);
    float2 d = post[0].xy;
    float3 sum = tex2D(source, uv).rgb * 0.2270270270;
    sum += (tex2D(source, uv + d).rgb + tex2D(source, uv - d).rgb) * 0.1945945946;
    sum += (tex2D(source, uv + d * 2.0).rgb + tex2D(source, uv - d * 2.0).rgb) * 0.1216216216;
    sum += (tex2D(source, uv + d * 3.0).rgb + tex2D(source, uv - d * 3.0).rgb) * 0.0540540541;
    sum += (tex2D(source, uv + d * 4.0).rgb + tex2D(source, uv - d * 4.0).rgb) * 0.0162162162;
    return float4(sum, 1.0);
}

// Just what it was, to put the screen back after a picture was drawn in its place.
float4 copyPs(float2 at : TEXCOORD0) : COLOR0
{
    return float4(tex2D(source, uvOf(at)).rgb, 1.0);
}

// Added to what is there, post[0].x of it.
float4 addPs(float2 at : TEXCOORD0) : COLOR0
{
    return float4(tex2D(source, uvOf(at)).rgb * post[0].x, 1.0);
}

// Each colour looked up in a table post[0].y a side, and mixed with what it was by post[0].x.
float4 gradePs(float2 at : TEXCOORD0) : COLOR0
{
    float3 color = saturate(tex2D(source, uvOf(at)).rgb);
    float n = post[0].y;
    float3 graded = tex3D(table, color * ((n - 1.0) / n) + 0.5 / n).rgb;
    return float4(lerp(color, graded, post[0].x), 1.0);
}
)";

    // The words of an effect (see gpu/effect_source.h), here.
    inline const char kEffectWords[] = R"(
float4 values[4] : register(c80);
float4 effectArea : register(c84);
float4 effectInfo : register(c85);
#define size effectInfo.xy
#define time effectInfo.z
)";

    inline const char kPictureStart[] = R"(
sampler2D image : register(s0);
float4 picture(float2 uv) { return tex2Dlod(image, float4(effectArea.xy + saturate(uv) * effectArea.zw, 0.0, 0.0)); }
)";

    inline const char kPictureMain[] = R"(
struct EffectOut
{
    float4 position : POSITION;
    float2 at : TEXCOORD0;
};

EffectOut effectVs(float2 corner : POSITION)
{
    EffectOut output;
    output.position = float4(corner, 0.0, 1.0);
    output.at = corner;
    return output;
}

float4 effectPs(float2 at : TEXCOORD0) : COLOR0
{
    float2 uv = float2(at.x * 0.5 + 0.5, 0.5 - at.y * 0.5);
    return float4(effect(uv, picture(uv)).rgb, 1.0);
}
)";

    inline const char kSurfacePixelStart[] = R"(
float4 picture(float2 uv) { return tex2D(image, uv); }
)";

    inline const char kSurfaceVertexStart[] = R"(
float4 picture(float2 uv) { return float4(1.0, 1.0, 1.0, 1.0); }
)";

    inline const char kSurfacePixelMain[] = R"(
float4 effectPs(MeshPixel input) : COLOR0
{
    float glow = surface.x;
    float4 base = paint(tex2D(image, input.uv) * input.color, input.uv, input.place, normalize(input.normal), glow);
    clip(base.a - 0.004);
    return finish(input, base, glow);
}
)";

    inline const char kSurfaceVertexMain[] = R"(
MeshOut effectVs(MeshIn input)
{
#ifdef MOVES
    input.position = move(input.position, input.normal, input.uv);
#endif
    return meshVsWith(input);
}
)";
}
