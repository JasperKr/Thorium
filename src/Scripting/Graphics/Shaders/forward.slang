module Forward;

import camera;
import utils;

struct VertexStageOutput
{
    float4 sv_position : SV_Position;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float4x2 tex_coord : TEXCOORDS;
    float4 color : COLOR;
};

struct VertexInput
{
    float4 Position : POSITION;
    uint2 TexCoords : TEXCOORD;
    uint Normal : NORMAL;
    uint Tangent : TANGENT;
    float4 Color  : COLOR;
};

uniform float4x4 DefaultProjectionMatrix;

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input)
{
    VertexStageOutput out;
    out.sv_position = mul(camera.ViewProjectionMatrix, float4(input.Position.xyz, 1.0));

    float2 uv0 = unpackHalf2x16ToFloat(input.TexCoords.x);
    float2 uv1 = unpackHalf2x16ToFloat(input.TexCoords.x >> 16);
    float2 uv2 = unpackHalf2x16ToFloat(input.TexCoords.y);
    float2 uv3 = unpackHalf2x16ToFloat(input.TexCoords.y >> 16);

    out.tex_coord = float4x2(uv0, uv1, uv2, uv3);
    out.normal = UnpackNormal(input.Normal).xyz;
    out.tangent = UnpackTangent(input.Tangent);

    out.color = input.Color;
    return out;
}

struct FragmentInput
{
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float4x2 tex_coord : TEXCOORDS;
    float4 color : COLOR;
};

Sampler2D MainTexture;

[shader("fragment")]
float4 fragmentMain(FragmentInput input)
{
  return float4(float3(1.0), MainTexture .Sample(float2(input.tex_coord._m00, input.tex_coord._m10)) .r) * input.color;
}
