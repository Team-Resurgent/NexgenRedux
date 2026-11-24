cbuffer Uniforms : register(b0)
{
    column_major float4x4 transform;
};

Texture2D image : register(t0);
SamplerState imageSampler : register(s0);

struct VertexIn
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct VertexOut
{
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    float4 position : SV_Position;
};

VertexOut vsMain(VertexIn input)
{
    VertexOut output;
    output.position = mul(transform, float4(input.position, 1.0));
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

float4 psMain(VertexOut input) : SV_Target
{
    return image.Sample(imageSampler, input.uv) * input.color;
}
