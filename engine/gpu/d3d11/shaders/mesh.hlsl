// Lit meshes, the shadows they cast, the sky, and shapes drawn by marching along rays.
#pragma pack_matrix(column_major)

static const int kLights = 16;
static const int kShapes = 24;

cbuffer View : register(b0)
{
    float4x4 viewProjection;
    float4x4 shadowMatrix;       // world to the shadow map: 0 to 1 across, down and deep
    float4 eye;                  // w: time
    float4 sunDirection;         // towards the sun; w: how dark shadows are, 0 for none
    float4 sunColor;
    float4 ambientSky;
    float4 ambientGround;
    float4 fogColor;
    float4 fogRange;             // start, 1 / length
    float4 cameraRight;          // half the width of the picture at distance 1
    float4 cameraUp;
    float4 cameraForward;
    float4 skyTop;
    float4 skyHorizon;
    float4 lightPlace[kLights];  // w: 1 / range
    float4 lightColor[kLights];
};

cbuffer Object : register(b1)
{
    float4x4 world;
    float4 tint;
    float4 surface;              // glow, shine, lit, fogged
    int4 lights;                 // indices, -1 for none
};

cbuffer Field : register(b2)
{
    float4 shapePlace[kShapes];  // w: kind + 8 * operation
    float4 shapeSize[kShapes];   // w: how softly it joins
    float4 shapeColor[kShapes];
    float4 shapeMore[kShapes];   // the cosine and sine of its turn, and the radius of a ball around it
    float4 fieldInfo;            // shapes, steps, farthest, repeated every (0: not)
    float4 fieldInfo2;           // shine, reflection
};

Texture2D image : register(t0);
Texture2D shadowMap : register(t1);
SamplerState imageSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

struct MeshIn
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct MeshOut
{
    float4 color : COLOR;
    float3 normal : NORMAL;
    float3 place : TEXCOORD1;
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};

MeshOut meshVs(MeshIn input)
{
    MeshOut output;
    float4 place = mul(world, float4(input.position, 1.0));
    output.position = mul(viewProjection, place);
    output.place = place.xyz;
    output.normal = mul((float3x3)world, input.normal);
    output.color = input.color * tint;
    output.uv = input.uv;
    return output;
}

float4 shadowVs(MeshIn input) : SV_Position
{
    return mul(viewProjection, mul(world, float4(input.position, 1.0)));
}

float shadowAt(float3 place, float facing)
{
    if (sunDirection.w <= 0.0)
    {
        return 1.0;
    }
    float3 at = mul(shadowMatrix, float4(place, 1.0)).xyz;
    if (any(at < 0.0) || any(at > 1.0))
    {
        return 1.0;
    }
    float sum = 0.0;
    float bias = 0.0006 + 0.0016 * (1.0 - facing);
    [unroll] for (int y = -1; y <= 1; y++)
    {
        [unroll] for (int x = -1; x <= 1; x++)
        {
            sum += shadowMap.SampleCmpLevelZero(shadowSampler, at.xy + float2(x, y) / 2048.0, at.z - bias);
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

float4 finish(MeshOut input, float4 base, float glow)
{
    float3 color = base.rgb;
    if (surface.z > 0.5)
    {
        float3 normal = normalize(input.normal);
        float shadow = shadowAt(input.place, saturate(dot(normal, sunDirection.xyz)));
        shadow = surface.z > 1.5 ? 0.0 : shadow;
        color = shade(base.rgb, normal, input.place, surface.y, shadow);
        [unroll] for (int i = 0; i < 4; i++)
        {
            int index = lights[i];
            if (index >= 0)
            {
                float3 way = lightPlace[index].xyz - input.place;
                float far = length(way);
                float reach = saturate(1.0 - far * lightPlace[index].w);
                color += base.rgb * lightColor[index].rgb * (saturate(dot(normal, way / max(far, 0.0001))) * reach * reach);
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

float4 meshPs(MeshOut input) : SV_Target
{
    return finish(input, image.Sample(imageSampler, input.uv) * input.color, surface.x);
}

struct ScreenOut
{
    float2 at : TEXCOORD0;
    float4 position : SV_Position;
};

// A triangle that covers the screen, as far away as can be.
ScreenOut screenVs(uint id : SV_VertexID)
{
    ScreenOut output;
    float2 corner = float2(id == 1 ? 3.0 : -1.0, id == 2 ? 3.0 : -1.0);
    output.at = corner;
    output.position = float4(corner, 1.0, 1.0);
    return output;
}

float3 rayOf(float2 at)
{
    return normalize(cameraForward.xyz + cameraRight.xyz * at.x + cameraUp.xyz * at.y);
}

float4 skyPs(ScreenOut input) : SV_Target
{
    return float4(sky(rayOf(input.at)), 1.0);
}

// --- Shapes by their distance ---

// How far it is to a shape.
float shapeDistance(int i, float3 p)
{
    uint kind = uint(shapePlace[i].w) % 8u;
    float3 d = p - shapePlace[i].xyz;
    float2 turn = shapeMore[i].xy;
    d.xz = float2(d.x * turn.x - d.z * turn.y, d.x * turn.y + d.z * turn.x);
    float3 size = shapeSize[i].xyz;
    if (kind == 0)
    {
        return length(d) - size.x;
    }
    if (kind == 1)
    {
        float3 q = abs(d) - size;
        return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - 0.03;
    }
    if (kind == 2)
    {
        return length(float2(length(d.xz) - size.x, d.y)) - size.y;
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
    float2 w = abs(float2(length(d.xz), d.y)) - size.xy;
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

Near nearTo(float3 from, float3 way, float far)
{
    Near found;
    found.mask = 0u;
    found.first = far;
    found.last = 0.0;
    found.plane = -1.0;
    int count = int(fieldInfo.x);
    bool everywhere = fieldInfo.w > 0.0;
    [loop] for (int i = 0; i < count; i++)
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
            float3 d = shapePlace[i].xyz - from;
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

float fieldDistance(float3 p, uint mask)
{
    float again = fieldInfo.w;
    if (again > 0.0)
    {
        p.xz -= again * floor(p.xz / again + 0.5);
    }
    float found = 500.0;
    int count = int(fieldInfo.x);
    [loop] for (int i = 0; i < count; i++)
    {
        if ((mask & (1u << uint(i))) == 0u)
        {
            continue;
        }
        float far = shapeDistance(i, p);
        float soft = max(shapeSize[i].w, 0.0001);
        uint operation = uint(shapePlace[i].w) / 8u;
        far = operation == 1 ? -far : far;
        // Joined is the nearest of the two; cut and kept are the farthest.
        float way_ = operation == 0 ? 1.0 : -1.0;
        float h = clamp(0.5 + 0.5 * way_ * (found - far) / soft, 0.0, 1.0);
        found = lerp(found, far, h) - way_ * soft * h * (1.0 - h);
    }
    return found;
}

float3 fieldAlbedo(float3 p, uint mask)
{
    float again = fieldInfo.w;
    if (again > 0.0)
    {
        p.xz -= again * floor(p.xz / again + 0.5);
    }
    float found = 500.0;
    float3 color = float3(0.0, 0.0, 0.0);
    int count = int(fieldInfo.x);
    [loop] for (int i = 0; i < count; i++)
    {
        if ((mask & (1u << uint(i))) == 0u)
        {
            continue;
        }
        float far = shapeDistance(i, p);
        float soft = max(shapeSize[i].w, 0.0001);
        uint operation = uint(shapePlace[i].w) / 8u;
        far = operation == 1 ? -far : far;
        float way_ = operation == 0 ? 1.0 : -1.0;
        float h = clamp(0.5 + 0.5 * way_ * (found - far) / soft, 0.0, 1.0);
        found = lerp(found, far, h) - way_ * soft * h * (1.0 - h);
        color = operation == 2 ? color : lerp(color, shapeColor[i].rgb, h);
    }
    return color;
}

float march(float3 from, float3 way, float far, int steps, Near close)
{
    float t = max(0.02, close.first);
    float last = close.last;
    [loop] for (int i = 0; i < steps; i++)
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

float3 fieldNormal(float3 p, uint mask)
{
    float2 e = float2(0.0015, -0.0015);
    return normalize(e.xyy * fieldDistance(p + e.xyy, mask) + e.yyx * fieldDistance(p + e.yyx, mask) + e.yxy * fieldDistance(p + e.yxy, mask) + e.xxx * fieldDistance(p + e.xxx, mask));
}

float fieldShadow(float3 from, float3 way)
{
    Near close = nearTo(from, way, 9.0);
    float lit = 1.0, t = max(0.06, close.first);
    [loop] for (int i = 0; i < 18; i++)
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

float3 fieldColor(float3 place, float3 way, Near close, out float3 normal)
{
    normal = fieldNormal(place, close.mask);
    float shadow = fieldShadow(place + normal * 0.02, sunDirection.xyz);
    // Darker where other surfaces are close.
    float open = clamp(fieldDistance(place + normal * 0.25, close.mask) / 0.25, 0.0, 1.0) * 0.5 + 0.5;
    return shade(fieldAlbedo(place, close.mask), normal, place, fieldInfo2.x, shadow) * open;
}

struct FieldOut
{
    float4 color : SV_Target;
    float depth : SV_Depth;
};

FieldOut fieldPs(ScreenOut input)
{
    FieldOut output;
    float3 way = rayOf(input.at);
    Near close = nearTo(eye.xyz, way, fieldInfo.z);
    float t = march(eye.xyz, way, fieldInfo.z, int(fieldInfo.y), close);
    if (t < 0.0)
    {
        discard;
    }
    float3 place = eye.xyz + way * t;
    float3 normal;
    float3 color = fieldColor(place, way, close, normal);
    if (fieldInfo2.y > 0.0)
    {
        float3 bounce = reflect(way, normal);
        float3 start = place + normal * 0.03;
        Near there = nearTo(start, bounce, fieldInfo.z * 0.5);
        float t2 = march(start, bounce, fieldInfo.z * 0.5, 28, there);
        float3 seen = sky(bounce);
        [branch] if (t2 > 0.0)
        {
            float3 n2;
            seen = fieldColor(start + bounce * t2, bounce, there, n2);
        }
        float edge = pow(1.0 - saturate(dot(normal, -way)), 3.0);
        color = lerp(color, seen, fieldInfo2.y * (0.25 + 0.75 * edge));
    }
    float fog = saturate((t - fogRange.x) * fogRange.y);
    output.color = float4(lerp(color, fogColor.rgb, fog), 1.0);
    float4 clip = mul(viewProjection, float4(place, 1.0));
    output.depth = saturate(clip.z / clip.w);
    return output;
}
