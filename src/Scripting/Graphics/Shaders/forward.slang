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

Sampler2D MainTexture;
StructuredBuffer<MaterialData> MaterialBuffer;

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  input.normal = normalize(input.normal);
  if (!input.isFrontFace) {
    input.normal = -input.normal;
    input.tangent.xyz = -input.tangent.xyz;
  }

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

  Pixel pixel;
  pixel.Normal = input.normal;
  pixel.Albedo = albedo;
  pixel.Metallic = matData.metallicFactor;
  pixel.Roughness = matData.roughnessFactor * matData.roughnessFactor;
  pixel.PerceptualRoughness = matData.roughnessFactor;
  pixel.Fresnel0 = ComputeFresnel0(albedo, matData.metallicFactor,
                                   matData.reflectanceFactor);

  float3 viewDir = camera.Position - input.WorldPosition;
  viewDir = normalize(viewDir);
  float NoV = saturate(dot(input.normal, viewDir));
  float lambdaV = ComputeLambdaV(pixel.Roughness, NoV);

  float3 radiance = DirectionalLight::SampleAll(input.WorldPosition, pixel,
                                                viewDir, NoV, lambdaV);
  float3 irradiance = float3(0.5, 0.5, 0.5); // Some ambient lighting
  float3 E = prefilteredDFG(pixel.PerceptualRoughness, pixel.Fresnel0, NoV);
  float3 diffuse = ComputeDiffuseColor(pixel.Albedo, pixel.Metallic);

  irradiance *= INV_PI;
  irradiance *= (1.0f.xxx - E) * diffuse;

  float3 color = radiance + irradiance;

  return float4(albedo + color, alpha);
}
