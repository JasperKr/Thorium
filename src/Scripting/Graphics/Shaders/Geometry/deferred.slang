import camera;
import material;
import utils;
import Lighting.pixel;
import Lighting.physicallyBased;
import constants;

import Lighting.Lights.directionalLight;

import transform;
struct PushConstantBuffer {
  uint MaterialBufferIndex;
  uint ModelTransformIndex;
};

[[vk::push_constant]]
PushConstantBuffer PushConstants;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 tex_coord : TEXCOORDS;
  float4 color : COLOR;
  float2 motion : MOTION;
};

struct VertexInput {
  float3 Position : POSITION;
  float2 TexCoords : TEXCOORD;
  uint Normal : NORMAL;
  uint Tangent : TANGENT;
  float4 Color : COLOR;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
  VertexStageOutput out;

  Transform transform = GetModelTransform(PushConstants.ModelTransformIndex);

  float4 worldPos = mul(float4(input.Position, 1.0), transform.ModelMatrix);
  out.sv_position = mul(camera.ViewProjectionMatrix, worldPos);
  float4 previousClip = mul(previousCamera.ViewProjectionMatrix, worldPos);

  float3x3 NormalMatrix = float3x3(transform.NormalMatrix);

  out.tex_coord = input.TexCoords;
  float3 normal = UnpackNormal(input.Normal).xyz;
  out.normal = normalize(mul(NormalMatrix, normal).xyz);

  float4 tangent = UnpackTangent(input.Tangent);
  out.tangent.xyz = normalize(mul(NormalMatrix, tangent.xyz));
  out.tangent.w = tangent.w;

  out.color = input.Color;

  float2 currentUV = out.sv_position.xy / out.sv_position.w;
  float2 previousUV = previousClip.xy / previousClip.w;
  currentUV = currentUV * 0.5 + 0.5;
  previousUV = previousUV * 0.5 + 0.5;

  out.motion = currentUV - previousUV;

  return out;
}

struct FragmentInput {
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 tex_coord : TEXCOORDS;
  float4 color : COLOR;
  float2 motion : MOTION;
  bool isFrontFace : SV_IsFrontFace;
};

Sampler2D AlbedoTexture;
Sampler2D MetallicRoughnessTexture;
Sampler2D NormalTexture;
Sampler2D EmissiveTexture;
Sampler2D ReflectanceTexture;

struct FragmentOutput {
  float4 Albedo : SV_Target0;
  float4 Normal : SV_Target1;
  float4 Material : SV_Target2;
  float4 Emissive : SV_Target3;
  float2 Motion : SV_Target4;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
  float3 normal = normalize(input.normal);
  float4 tangent = float4(normalize(input.tangent.xyz), input.tangent.w);

  if (!input.isFrontFace) {
    normal = -normal;
    tangent.xyz = -tangent.xyz;
  }

  float4 texColor = AlbedoTexture.Sample(input.tex_coord);
  MaterialData matData = MaterialBuffer[PushConstants.MaterialBufferIndex];

  float3 albedo = texColor.rgb * matData.albedoFactor.rgb;

  float2 metallicRoughness =
      MetallicRoughnessTexture.Sample(input.tex_coord).bg;

  float3 normalMap =
      normalize(NormalTexture.Sample(input.tex_coord).xyz * 2.0f - 1.0f);

  float3 bitangent = cross(normal, tangent.xyz) * -tangent.w;
  float3x3 TBN = float3x3(tangent.xyz, bitangent, normal);
  TBN = transpose(TBN);
  normal = mul(TBN, normalMap);

  float3 emissiveSample = EmissiveTexture.Sample(input.tex_coord).rgb;
  float reflectanceSample = ReflectanceTexture.Sample(input.tex_coord).r;

  float metallic = metallicRoughness.x * matData.metallicFactor;
  float perceptualRoughness = metallicRoughness.y * matData.roughnessFactor;
  float reflectance = reflectanceSample * matData.reflectanceFactor;

  MaterialChannel material =
      MaterialChannel(metallic, perceptualRoughness, reflectance, 1.0F);

  float3 emissive = emissiveSample * matData.emissiveFactor;

  return FragmentOutput(
      float4(albedo, 0.0), float4(normalize(normal) * 0.5 + 0.5, 0.0),
      material.packed, float4(emissive, 1.0), PackMotionVector(input.motion));
}
