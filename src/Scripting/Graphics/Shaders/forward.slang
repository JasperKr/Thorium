module Forward;

import camera;
import utils;

struct VertexStageOutput
{
    float4 sv_position : SV_Position;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 tex_coord : TEXCOORDS;
    float4 color : COLOR;
};

struct VertexInput
{
    float3 Position : POSITION;
    float2 TexCoords : TEXCOORD;
    uint Normal : NORMAL;
    uint Tangent : TANGENT;
    float4 Color  : COLOR;
};

uniform float4x4 ModelMatrix;
uniform float3x3 NormalMatrix;
uniform float4x4 DefaultProjectionMatrix;

float3 Rotate(float3 v, float4 q)
{
  return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input)
{
    VertexStageOutput out;

    float4 worldPos = mul(ModelMatrix, float4(input.Position, 1.0));
    out.sv_position = mul(camera.ViewProjectionMatrix, worldPos);

    out.tex_coord = input.TexCoords;
    float3 normal = normalize(UnpackNormal(input.Normal).xyz);
    out.normal = mul(NormalMatrix, normal).xyz;

    float4 tangent = UnpackTangent(input.Tangent);
    out.tangent.xyz = mul(NormalMatrix, tangent.xyz);
    out.tangent.w = tangent.w;

    out.color = input.Color;
    return out;
}

struct FragmentInput
{
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 tex_coord : TEXCOORDS;
    float4 color : COLOR;
};

Sampler2D MainTexture;

[shader("fragment")]
float4 fragmentMain(FragmentInput input)
{
  float3 Incoming = normalize(float3(0.0, 1.0, 1.0));
  float NoL = dot(input.normal, Incoming) * 0.5 + 0.5;
  float4 texColor = MainTexture.Sample(input.tex_coord);

  return float4((texColor.rgb * input.normal.rgb) * NoL, texColor.a * input.color.a);
}
