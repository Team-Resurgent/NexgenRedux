// Bloom: only what is bright, smaller and smaller, and then larger again and added up.

cbuffer EffectValues : register(b3)
{
    float4 values[4];            // what to do, threshold, strength; the size of a step
    float4 effectArea;
    float4 effectInfo;
};

Texture2D image : register(t0);
SamplerState imageSampler : register(s0);

float4 blurPs(float2 at : TEXCOORD0) : SV_Target
{
    float2 uv = float2(at.x * 0.5 + 0.5, 0.5 - at.y * 0.5);
    float2 along = values[1].xy;
    if (values[0].x < 1.5)
    {
        float3 color = image.SampleLevel(imageSampler, uv, 0).rgb * 4.0 + image.SampleLevel(imageSampler, uv + along, 0).rgb + image.SampleLevel(imageSampler, uv - along, 0).rgb +
            image.SampleLevel(imageSampler, uv + float2(along.x, -along.y), 0).rgb + image.SampleLevel(imageSampler, uv + float2(-along.x, along.y), 0).rgb;
        color /= 8.0;
        if (values[0].x < 0.5)
        {
            float bright = max(color.r, max(color.g, color.b));
            color *= smoothstep(values[0].y, values[0].y + 0.3, bright);
        }
        return float4(color, 1.0);
    }
    float3 sum = image.SampleLevel(imageSampler, uv + float2(-along.x * 2.0, 0.0), 0).rgb + image.SampleLevel(imageSampler, uv + float2(along.x * 2.0, 0.0), 0).rgb +
        image.SampleLevel(imageSampler, uv + float2(0.0, along.y * 2.0), 0).rgb + image.SampleLevel(imageSampler, uv + float2(0.0, -along.y * 2.0), 0).rgb;
    sum += (image.SampleLevel(imageSampler, uv + along, 0).rgb + image.SampleLevel(imageSampler, uv - along, 0).rgb +
        image.SampleLevel(imageSampler, uv + float2(along.x, -along.y), 0).rgb + image.SampleLevel(imageSampler, uv + float2(-along.x, along.y), 0).rgb) * 2.0;
    return float4(sum / 12.0 * values[0].z, 1.0);
}
