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

    out.sv_position = mul(float4(Position, 1.0), camera.ViewProjectionMatrix);

    out.tex_coord = input.TexCoords;
    float3 normal = UnpackNormal(input.Normal).xyz;
    out.normal = Rotate(normal, transform.Rotation);

    float4 tangent = UnpackTangent(input.Tangent);
    out.tangent = float4(Rotate(tangent.xyz, transform.Rotation), tangent.w);

    out.color = input.Color; //unpackUnorm4x8ToFloat(input.Color);
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

  return float4((1.0 + texColor.rgb * input.color.rgb) * NoL, texColor.a * input.color.a);
}
