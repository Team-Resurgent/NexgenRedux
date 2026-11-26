#pragma once

// The same as mesh.hlsl, for OpenGL. Change them together.
namespace nx::gpu::shaders
{
    inline const char kMeshCommon[] = R"(#version 330 core
layout(std140) uniform View
{
    mat4 viewProjection;
    mat4 shadowMatrix;
    vec4 eye;
    vec4 sunDirection;
    vec4 sunColor;
    vec4 ambientSky;
    vec4 ambientGround;
    vec4 fogColor;
    vec4 fogRange;
    vec4 cameraRight;
    vec4 cameraUp;
    vec4 cameraForward;
    vec4 skyTop;
    vec4 skyHorizon;
    vec4 lightPlace[16];
    vec4 lightColor[16];
};
layout(std140) uniform Object
{
    mat4 world;
    vec4 tint;
    vec4 surface;
    ivec4 lights;
    vec4 wind;
    vec4 volume;
};
)";

    inline const char kMeshVertex[] = R"(
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 uv;
out vec4 vertexColor;
out vec3 vertexNormal;
out vec3 vertexPlace;
out vec2 vertexUv;
void main()
{
    vec4 place = world * vec4(position, 1.0);
    // Bent by the wind, more the higher the vertex is: v says how high, in metres. Gusts roll along the way it blows.
    if (wind.x != 0.0 || wind.y != 0.0)
    {
        float along = dot(place.xz, normalize(wind.xy)) * 0.12;
        float gust = sin(eye.w * 1.3 - along) * 0.55 + sin(eye.w * 2.9 - along * 1.7 + place.x * 0.3) * 0.2 + 0.35;
        place.xz += wind.xy * (uv.y * gust);
    }
    gl_Position = viewProjection * place;
    gl_Position.z = gl_Position.z * 2.0 - gl_Position.w;
    vertexPlace = place.xyz;
    vertexNormal = mat3(world) * normal;
    vertexColor = color * tint;
    vertexUv = uv;
}
)";

    inline const char kShadowVertex[] = R"(
layout(location = 0) in vec3 position;
void main()
{
    gl_Position = viewProjection * world * vec4(position, 1.0);
    gl_Position.z = gl_Position.z * 2.0 - gl_Position.w;
}
)";

    inline const char kShadowFragment[] = R"(
void main() {}
)";

    inline const char kScreenVertex[] = R"(
out vec2 at;
void main()
{
    at = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    gl_Position = vec4(at, 1.0, 1.0);
}
)";

    inline const char kShade[] = R"(
uniform sampler2D image;
uniform sampler2DShadow shadowMap;
uniform sampler3D volumeImage;
layout(location = 0) out vec4 result;

float shadowAt(vec3 place, float facing)
{
    if (sunDirection.w <= 0.0)
    {
        return 1.0;
    }
    vec3 at = (shadowMatrix * vec4(place, 1.0)).xyz;
    if (any(lessThan(at, vec3(0.0))) || any(greaterThan(at, vec3(1.0))))
    {
        return 1.0;
    }
    float sum = 0.0;
    float bias = 0.0006 + 0.0016 * (1.0 - facing);
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            sum += texture(shadowMap, vec3(at.xy + vec2(x, y) / 2048.0, at.z - bias));
        }
    }
    vec2 edge = abs(at.xy * 2.0 - 1.0);
    float fade = clamp((max(edge.x, edge.y) - 0.85) / 0.15, 0.0, 1.0);
    return mix(1.0 - sunDirection.w, 1.0, max(sum / 9.0, fade));
}

vec3 shade(vec3 albedo, vec3 normal, vec3 place, float shine, float shadow)
{
    vec3 toEye = normalize(eye.xyz - place);
    vec3 light = mix(ambientGround.rgb, ambientSky.rgb, normal.y * 0.5 + 0.5);
    float facing = clamp(dot(normal, sunDirection.xyz), 0.0, 1.0);
    light += sunColor.rgb * (facing * shadow);
    vec3 color = albedo * light;
    if (shine > 0.0)
    {
        vec3 middle = normalize(sunDirection.xyz + toEye);
        color += sunColor.rgb * (pow(clamp(dot(normal, middle), 0.0, 1.0), 8.0 + shine * 120.0) * shine * shadow * step(0.001, facing));
    }
    return color;
}

vec3 sky(vec3 direction)
{
    float up = max(direction.y + 0.08, 0.0);
    vec3 color = mix(skyHorizon.rgb, skyTop.rgb, 1.0 - exp(-3.6 * up * up / (up + 0.2)));
    float sun = clamp(dot(direction, sunDirection.xyz), 0.0, 1.0);
    color += sunColor.rgb * (pow(sun, 900.0) * 2.0 + pow(sun, 24.0) * 0.18);
    return color;
}

vec3 rayOf(vec2 at)
{
    return normalize(cameraForward.xyz + cameraRight.xyz * at.x + cameraUp.xyz * at.y);
}
)";

    inline const char kMeshFinish[] = R"(
in vec4 vertexColor;
in vec3 vertexNormal;
in vec3 vertexPlace;
in vec2 vertexUv;
vec4 finish(vec4 base, float glow)
{
    vec3 color = base.rgb;
    if (surface.z > 0.5)
    {
        vec3 normal = normalize(vertexNormal);
        float shadow = shadowAt(vertexPlace, clamp(dot(normal, sunDirection.xyz), 0.0, 1.0));
        shadow = surface.z > 1.5 ? 0.0 : shadow;
        color = shade(base.rgb, normal, vertexPlace, surface.y, shadow);
        for (int i = 0; i < 4; i++)
        {
            int index = lights[i];
            if (index >= 0)
            {
                vec3 way = lightPlace[index].xyz - vertexPlace;
                float far = length(way);
                float reach = clamp(1.0 - far * lightPlace[index].w, 0.0, 1.0);
                color += base.rgb * lightColor[index].rgb * (clamp(dot(normal, way / max(far, 0.0001)), 0.0, 1.0) * reach * reach);
            }
        }
    }
    color = mix(color, base.rgb, glow);
    if (surface.w > 0.5)
    {
        float fog = clamp((distance(eye.xyz, vertexPlace) - fogRange.x) * fogRange.y, 0.0, 1.0);
        color = mix(color, fogColor.rgb, fog);
    }
    return vec4(color, base.a);
}
)";

    inline const char kMeshFragment[] = R"(
void main()
{
    vec4 base = volume.w > 0.0 ? texture(volumeImage, vertexPlace * volume.w + volume.xyz) * vertexColor : texture(image, vertexUv) * vertexColor;
    if (base.a < wind.z)
    {
        discard;
    }
    result = finish(base, surface.x);
}
)";

    // Effects: what others write goes between.
    inline const char kEffectVertexMain[] = R"(
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 uv;
out vec4 vertexColor;
out vec3 vertexNormal;
out vec3 vertexPlace;
out vec2 vertexUv;
void main()
{
#ifdef MOVES
    vec4 place = world * vec4(move(position, normal, uv), 1.0);
#else
    vec4 place = world * vec4(position, 1.0);
#endif
    gl_Position = viewProjection * place;
    gl_Position.z = gl_Position.z * 2.0 - gl_Position.w;
    vertexPlace = place.xyz;
    vertexNormal = mat3(world) * normal;
    vertexColor = color * tint;
    vertexUv = uv;
}
)";

    inline const char kEffectVertexStart[] = R"(
vec4 picture(vec2 uv) { return vec4(1.0); }
)";

    inline const char kEffectSurfaceStart[] = R"(
vec4 picture(vec2 uv) { return texture(image, uv); }
)";

    inline const char kEffectSurfaceMain[] = R"(
void main()
{
    float glow = surface.x;
    vec4 base = paint(texture(image, vertexUv) * vertexColor, vertexUv, vertexPlace, normalize(vertexNormal), glow);
    if (base.a < 0.004)
    {
        discard;
    }
    result = finish(base, glow);
}
)";

    inline const char kEffectPictureStart[] = R"(
uniform sampler2D image;
layout(location = 0) out vec4 result;
in vec2 at;
vec4 picture(vec2 uv)
{
    vec2 p = effectArea.xy + clamp(uv, 0.0, 1.0) * effectArea.zw;
    return textureLod(image, vec2(p.x, 1.0 - p.y), 0.0);
}
)";

    inline const char kEffectPictureMain[] = R"(
void main()
{
    vec2 uv = vec2(at.x * 0.5 + 0.5, 0.5 - at.y * 0.5);
    result = vec4(effect(uv, picture(uv)).rgb, 1.0);
}
)";

    // Bloom: only what is bright, smaller and smaller, and then larger again and added up.
    inline const char kBlurFragment[] = R"(
uniform sampler2D image;
layout(location = 0) out vec4 result;
in vec2 at;
layout(std140) uniform EffectValues
{
    vec4 values[4];
    vec4 effectArea;
    vec4 effectInfo;
};
void main()
{
    vec2 uv = at * 0.5 + 0.5;
    vec2 step = values[1].xy;
    if (values[0].x < 1.5)
    {
        vec4 sum = texture(image, uv) * 4.0 + texture(image, uv + step) + texture(image, uv - step) + texture(image, uv + vec2(step.x, -step.y)) + texture(image, uv + vec2(-step.x, step.y));
        vec3 color = sum.rgb / 8.0;
        if (values[0].x < 0.5)
        {
            float bright = max(color.r, max(color.g, color.b));
            color *= smoothstep(values[0].y, values[0].y + 0.3, bright);
        }
        result = vec4(color, 1.0);
        return;
    }
    vec3 sum = texture(image, uv + vec2(-step.x * 2.0, 0.0)).rgb + texture(image, uv + vec2(step.x * 2.0, 0.0)).rgb + texture(image, uv + vec2(0.0, step.y * 2.0)).rgb + texture(image, uv + vec2(0.0, -step.y * 2.0)).rgb;
    sum += (texture(image, uv + step).rgb + texture(image, uv - step).rgb + texture(image, uv + vec2(step.x, -step.y)).rgb + texture(image, uv + vec2(-step.x, step.y)).rgb) * 2.0;
    result = vec4(sum / 12.0 * values[0].z, 1.0);
}
)";

    inline const char kSkyFragment[] = R"(
in vec2 at;
void main()
{
    result = vec4(sky(rayOf(at)), 1.0);
}
)";

    inline const char kFieldFragment[] = R"(
in vec2 at;
layout(std140) uniform Field
{
    vec4 shapePlace[24];
    vec4 shapeSize[24];
    vec4 shapeColor[24];
    vec4 shapeMore[24];
    vec4 fieldInfo;
    vec4 fieldInfo2;
};

// How far it is to a shape.
float shapeDistance(int i, vec3 p)
{
    int kind = int(shapePlace[i].w) % 8;
    vec3 d = p - shapePlace[i].xyz;
    vec2 turn = shapeMore[i].xy;
    d.xz = vec2(d.x * turn.x - d.z * turn.y, d.x * turn.y + d.z * turn.x);
    vec3 size = shapeSize[i].xyz;
    if (kind == 0)
    {
        return length(d) - size.x;
    }
    if (kind == 1)
    {
        vec3 q = abs(d) - size;
        return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - 0.03;
    }
    if (kind == 2)
    {
        return length(vec2(length(d.xz) - size.x, d.y)) - size.y;
    }
    if (kind == 3)
    {
        d.y -= clamp(d.y, -size.y, size.y);
        return length(d) - size.x;
    }
    if (kind == 4)
    {
        return d.y;
    }
    vec2 w = abs(vec2(length(d.xz), d.y)) - size.xy;
    return min(max(w.x, w.y), 0.0) + length(max(w, 0.0));
}

// Which shapes a line goes close, and between where and where. The rest needn't be looked at. Planes go
// on for ever: where there are no others, the line meets them without any marching.
struct Near
{
    uint mask;
    float first;
    float last;
    float plane;
};

Near nearTo(vec3 from, vec3 way, float far)
{
    Near found;
    found.mask = 0u;
    found.first = far;
    found.last = 0.0;
    found.plane = -1.0;
    int count = int(fieldInfo.x);
    bool everywhere = fieldInfo.w > 0.0;
    for (int i = 0; i < count; i++)
    {
        int code = int(shapePlace[i].w);
        if (everywhere || code >= 16)
        {
            found.mask |= 1u << uint(i);
        }
        else if (code == 4)
        {
            found.mask |= 1u << uint(i);
            float down = way.y < -0.0001 ? (shapePlace[i].y - from.y) / way.y : -1.0;
            found.plane = down > 0.0 && (found.plane < 0.0 || down < found.plane) ? down : found.plane;
        }
        else
        {
            vec3 d = shapePlace[i].xyz - from;
            float along = dot(d, way);
            float off = dot(d, d) - along * along;
            float radius = shapeMore[i].z;
            if (off < radius * radius)
            {
                float reach = sqrt(radius * radius - off);
                if (along + reach > 0.0)
                {
                    found.mask |= 1u << uint(i);
                    if (code < 8)
                    {
                        found.first = min(found.first, along - reach);
                        found.last = max(found.last, along + reach);
                    }
                }
            }
        }
    }
    if (everywhere)
    {
        found.first = 0.0;
        found.last = far;
    }
    found.first = max(found.first, 0.0);
    found.last = min(found.last, far);
    return found;
}

float fieldDistance(vec3 p, uint mask)
{
    float again = fieldInfo.w;
    if (again > 0.0)
    {
        p.xz -= again * floor(p.xz / again + 0.5);
    }
    float found = 500.0;
    int count = int(fieldInfo.x);
    for (int i = 0; i < count; i++)
    {
        if ((mask & (1u << uint(i))) == 0u)
        {
            continue;
        }
        float far = shapeDistance(i, p);
        float soft = max(shapeSize[i].w, 0.0001);
        int operation = int(shapePlace[i].w) / 8;
        far = operation == 1 ? -far : far;
        // Joined is the nearest of the two; cut and kept are the farthest.
        float way_ = operation == 0 ? 1.0 : -1.0;
        float h = clamp(0.5 + 0.5 * way_ * (found - far) / soft, 0.0, 1.0);
        found = mix(found, far, h) - way_ * soft * h * (1.0 - h);
    }
    return found;
}

vec3 fieldAlbedo(vec3 p, uint mask)
{
    float again = fieldInfo.w;
    if (again > 0.0)
    {
        p.xz -= again * floor(p.xz / again + 0.5);
    }
    float found = 500.0;
    vec3 color = vec3(0.0, 0.0, 0.0);
    int count = int(fieldInfo.x);
    for (int i = 0; i < count; i++)
    {
        if ((mask & (1u << uint(i))) == 0u)
        {
            continue;
        }
        float far = shapeDistance(i, p);
        float soft = max(shapeSize[i].w, 0.0001);
        int operation = int(shapePlace[i].w) / 8;
        far = operation == 1 ? -far : far;
        float way_ = operation == 0 ? 1.0 : -1.0;
        float h = clamp(0.5 + 0.5 * way_ * (found - far) / soft, 0.0, 1.0);
        found = mix(found, far, h) - way_ * soft * h * (1.0 - h);
        color = operation == 2 ? color : mix(color, shapeColor[i].rgb, h);
    }
    return color;
}

float march(vec3 from, vec3 way, float far, int steps, Near close)
{
    float t = max(0.02, close.first);
    float last = close.last;
    for (int i = 0; i < steps; i++)
    {
        if (t > last)
        {
            break;
        }
        float d = fieldDistance(from + way * t, close.mask);
        if (d < 0.0012 * t + 0.0004)
        {
            return t;
        }
        t += d;
    }
    // Past everything else.
    return close.plane > 0.0 && close.plane < far && fieldInfo.w <= 0.0 && (t > last || close.plane < t + 0.05) ? close.plane : -1.0;
}

vec3 fieldNormal(vec3 p, uint mask)
{
    vec2 e = vec2(0.0015, -0.0015);
    return normalize(e.xyy * fieldDistance(p + e.xyy, mask) + e.yyx * fieldDistance(p + e.yyx, mask) + e.yxy * fieldDistance(p + e.yxy, mask) + e.xxx * fieldDistance(p + e.xxx, mask));
}

float fieldShadow(vec3 from, vec3 way)
{
    Near close = nearTo(from, way, 9.0);
    float lit = 1.0, t = max(0.06, close.first);
    for (int i = 0; i < 18; i++)
    {
        if (t > close.last || lit < 0.01)
        {
            break;
        }
        float d = fieldDistance(from + way * t, close.mask);
        lit = min(lit, 10.0 * d / t);
        t += clamp(d, 0.04, 0.7);
    }
    return clamp(lit, 0.0, 1.0);
}

vec3 fieldColor(vec3 place, vec3 way, Near close, out vec3 normal)
{
    normal = fieldNormal(place, close.mask);
    float shadow = fieldShadow(place + normal * 0.02, sunDirection.xyz);
    // Darker where other surfaces are close.
    float open = clamp(fieldDistance(place + normal * 0.25, close.mask) / 0.25, 0.0, 1.0) * 0.5 + 0.5;
    return shade(fieldAlbedo(place, close.mask), normal, place, fieldInfo2.x, shadow) * open;
}

void main()
{
    vec3 way = rayOf(at);
    Near close = nearTo(eye.xyz, way, fieldInfo.z);
    float t = march(eye.xyz, way, fieldInfo.z, int(fieldInfo.y), close);
    if (t < 0.0)
    {
        discard;
    }
    vec3 place = eye.xyz + way * t;
    vec3 normal;
    vec3 color = fieldColor(place, way, close, normal);
    if (fieldInfo2.y > 0.0)
    {
        vec3 bounce = reflect(way, normal);
        vec3 start = place + normal * 0.03;
        Near there = nearTo(start, bounce, fieldInfo.z * 0.5);
        float t2 = march(start, bounce, fieldInfo.z * 0.5, 28, there);
        vec3 seen = sky(bounce);
        if (t2 > 0.0)
        {
            vec3 n2;
            seen = fieldColor(start + bounce * t2, bounce, there, n2);
        }
        float edge = pow(1.0 - clamp(dot(normal, -way), 0.0, 1.0), 3.0);
        color = mix(color, seen, fieldInfo2.y * (0.25 + 0.75 * edge));
    }
    float fog = clamp((t - fogRange.x) * fogRange.y, 0.0, 1.0);
    result = vec4(mix(color, fogColor.rgb, fog), 1.0);
    vec4 clip = viewProjection * vec4(place, 1.0);
    gl_FragDepth = clamp(clip.z / clip.w, 0.0, 1.0);
}
)";
}
