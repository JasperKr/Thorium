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

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
  VertexStageOutput out;

  float4 worldPos = mul(float4(input.Position, 1.0), ModelMatrix);
  out.sv_position = mul(camera.ViewProjectionMatrix, worldPos);

  out.tex_coord = input.TexCoords;
  float3 normal = UnpackNormal(input.Normal).xyz;
  out.normal = normalize(mul(NormalMatrix, normal).xyz);

  float4 tangent = UnpackTangent(input.Tangent);
  out.tangent.xyz = normalize(mul(NormalMatrix, tangent.xyz));
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
Sampler2D NormalTexture;
Sampler2D EmissiveTexture;
Sampler2D ReflectanceTexture;
StructuredBuffer<MaterialData> MaterialBuffer;

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  float3 normal = normalize(input.normal);
  float4 tangent = float4(normalize(input.tangent.xyz), input.tangent.w);

  // For some reason normals are flipped so we remove the ! here ig.
  if (!input.isFrontFace) {
    normal = -normal;
    tangent.xyz = -tangent.xyz;
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

  float3 normalMap = NormalTexture.Sample(input.tex_coord).xyz * 2.0f - 1.0f;

  float3 bitangent = cross(normal, tangent.xyz) * tangent.w;
  float3x3 TBN = float3x3(tangent.xyz, bitangent, normal);
  TBN = transpose(TBN);
  normal = mul(TBN, normalMap);

  float3 emissiveSample = EmissiveTexture.Sample(input.tex_coord).rgb;
  float reflectanceSample = ReflectanceTexture.Sample(input.tex_coord).r;

  float metallic = metallicRoughness.x * matData.metallicFactor;
  float roughness = metallicRoughness.y * matData.roughnessFactor;
  float reflectance = reflectanceSample * matData.reflectanceFactor;
  float3 emissive = emissiveSample * matData.emissiveFactor;

  Pixel pixel;
  pixel.Normal = normal;
  pixel.Albedo = albedo;
  pixel.Metallic = metallic;
  pixel.Roughness = roughness * roughness;
  pixel.PerceptualRoughness = roughness;
  pixel.Fresnel0 = ComputeFresnel0(albedo, metallic, reflectance);

  float3 viewDir = normalize(camera.Position - input.WorldPosition);
  float NoV = ClampNoV(dot(normal, viewDir));
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

  float3 color = direct + irradiance + radiance + emissive;

  return float4(color, 1.0);
}
