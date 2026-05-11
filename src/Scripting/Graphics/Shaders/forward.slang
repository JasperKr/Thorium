module Forward;

import camera;
import material;
import utils;
import Lighting.pixel;
import Lighting.physicallyBased;
import constants;

import Lighting.Lights.directionalLight;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float3 WorldPosition : WORLDPOSITION;
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
  out.WorldPosition = worldPos.xyz;
  return out;
}

struct FragmentInput {
  float3 WorldPosition : WORLDPOSITION;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 tex_coord : TEXCOORDS;
  float4 color : COLOR;
  bool isFrontFace : SV_IsFrontFace;
};

Sampler2D AlbedoTexture;
Sampler2D MetallicRoughnessTexture;
StructuredBuffer<MaterialData> MaterialBuffer;

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  input.normal = normalize(input.normal);

  // For some reason normals are flipped so we remove the ! here ig.
  if (input.isFrontFace) {
    input.normal = -input.normal;
    input.tangent.xyz = -input.tangent.xyz;
  }

  float4 texColor = AlbedoTexture.Sample(input.tex_coord);
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

  float2 metallicRoughness =
      MetallicRoughnessTexture.Sample(input.tex_coord).bg;
  float metallic = metallicRoughness.x * matData.metallicFactor;
  float roughness = metallicRoughness.y * matData.roughnessFactor;

  Pixel pixel;
  pixel.Normal = input.normal;
  pixel.Albedo = albedo;
  pixel.Metallic = metallic;
  pixel.Roughness = roughness * roughness;
  pixel.PerceptualRoughness = roughness;
  pixel.Fresnel0 = ComputeFresnel0(albedo, metallic, matData.reflectanceFactor);

  float3 viewDir = normalize(camera.Position - input.WorldPosition);
  float NoV = ClampNoV(dot(input.normal, viewDir));
  float lambdaV = ComputeLambdaV(pixel.Roughness, NoV);

  float3 direct = DirectionalLight::SampleAll(input.WorldPosition, pixel,
                                              viewDir, NoV, lambdaV);
  float3 E = prefilteredDFG(pixel.PerceptualRoughness, pixel.Fresnel0, NoV);
  float3 diffuse = ComputeDiffuseColor(pixel.Albedo, pixel.Metallic);

  float3 irradiance = float3(1, 1, 1); // Some ambient lighting
  float3 radiance = irradiance;        // Specular IBL contribution
  irradiance *= INV_PI;
  irradiance *= (1.0f.xxx - E) * diffuse;
  radiance *= E;

  float3 color = direct + irradiance + radiance;

  return float4(color, alpha);
}
