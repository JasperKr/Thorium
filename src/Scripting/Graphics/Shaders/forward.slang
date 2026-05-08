module Forward;

import camera;
import material;
import utils;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 tex_coord : TEXCOORDS;
  float4 color : COLOR;
};

struct VertexInput {
  float3 Position : POSITION;
  float2 TexCoords : TEXCOORD;
  uint Normal : NORMAL;
  uint Tangent : TANGENT;
  float4 Color : COLOR;
};

uniform float4x4 ModelMatrix;
uniform float3x3 NormalMatrix;
uniform float4x4 DefaultProjectionMatrix;

float3 Rotate(float3 v, float4 q) {
  return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
  VertexStageOutput out;

  float4 worldPos = mul(float4(input.Position, 1.0), ModelMatrix);
  out.sv_position = mul(worldPos, camera.ViewProjectionMatrix);

  out.tex_coord = input.TexCoords;
  float3 normal = UnpackNormal(input.Normal).xyz;
  out.normal = normalize(mul(NormalMatrix, normal).xyz);

  float4 tangent = UnpackTangent(input.Tangent);
  out.tangent.xyz = mul(NormalMatrix, tangent.xyz);
  out.tangent.w = tangent.w;

  out.color = input.Color;
  return out;
}

struct FragmentInput {
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 tex_coord : TEXCOORDS;
  float4 color : COLOR;
  bool isFrontFace : SV_IsFrontFace;
};

Sampler2D MainTexture;
StructuredBuffer<MaterialData> MaterialBuffer;

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  input.normal = normalize(input.normal);
  if (!input.isFrontFace) {
    input.normal = -input.normal;
    input.tangent.xyz = -input.tangent.xyz;
  }

  float3 Incoming = normalize(float3(0.0, 1.0, 1.0));
  float NoL = dot(input.normal, Incoming) * 0.5 + 1.0;
  float4 texColor = MainTexture.Sample(input.tex_coord);
  MaterialData matData = MaterialBuffer[MaterialBufferIndex];

  float alpha = texColor.a * matData.albedoFactor.a;

  if (matData.alphaMode == AlphaMode::Mask) {
    if (alpha <= matData.alphaCutoff) {
      discard;
    }
  }

  if (matData.alphaMode != AlphaMode::Blend) {
    alpha = 1.0;
  }

  float3 albedo = texColor.rgb * matData.albedoFactor.rgb;

  return float4(albedo * NoL, alpha);
}
