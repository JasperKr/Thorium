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

struct Transform {
  float3 Position;
  float4 Rotation;
  float3 Scale;
};

uniform Transform transform;
uniform float4x4 DefaultProjectionMatrix;

float3 Rotate(float3 v, float4 q)
{
  return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input)
{
    VertexStageOutput out;

    float3 Position = input.Position.xyz * transform.Scale;
    Position = Rotate(Position, transform.Rotation);
    Position += transform.Position;

    out.sv_position = mul(camera.ViewProjectionMatrix, float4(Position, 1.0));

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
