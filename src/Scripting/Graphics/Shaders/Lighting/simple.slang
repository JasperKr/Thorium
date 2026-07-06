// Simple deferred lighting shader that just for-loops over all lights and accumulates their contribution.

import camera;
import physicallyBased;
import utils;
import pixel;
import constants;

import Lights.directionalLight;
import Lights.pointLight;
import Lights.spotLight;

#include "fullscreen.slang"

struct FragmentOutput {
  float4 color : SV_Target0;
};

struct FragmentInput {
  float2 tex_coord : TEXCOORDS;
};

Sampler2D AlbedoTexture;
Sampler2D NormalTexture;
Sampler2D MaterialTexture;
Sampler2D DepthTexture;
Sampler2D EmissiveTexture;

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
  FragmentOutput out;

  float3 albedo = AlbedoTexture.Sample(input.tex_coord).rgb;
  float3 normalData = NormalTexture.Sample(input.tex_coord).xyz;
  float4 materialData = MaterialTexture.Sample(input.tex_coord);
  float3 emissive = EmissiveTexture.Sample(input.tex_coord).rgb;
  float depth = DepthTexture.Sample(input.tex_coord).r;

  if (depth == DepthFarPlane) {
    discard;
  }

  float3 normal = normalize(normalData.xyz * 2.0f - 1.0f);
  float metallic = materialData.r;
  float perceptualRoughness = ClampPerceptualRoughness(materialData.g);
  float reflectance = materialData.b;
  float ao = materialData.a;

  float3 incomingLight = emissive.rgb;

  float3 viewDir = -camera.GetRayDirection(input.tex_coord);
  float3 worldPosition = camera.GetWorldPosition(input.tex_coord, depth);

  Pixel pixel;
  pixel.Normal = normal;
  pixel.Albedo = albedo.rgb;
  pixel.Metallic = metallic;
  pixel.PerceptualRoughness = perceptualRoughness;
  pixel.Roughness = CalculateRoughness(perceptualRoughness);
  pixel.Fresnel0 = ComputeFresnel0(albedo, metallic, reflectance);

  float NoV = ClampNoV(dot(normal, viewDir));
  float LambdaV = ComputeLambdaV(pixel.Roughness, NoV);

  incomingLight +=
      DirectionalLight.SampleAll(worldPosition, pixel, viewDir, NoV, LambdaV);
  incomingLight +=
      PointLight.SampleAll(worldPosition, pixel, viewDir, NoV, LambdaV);
  incomingLight +=
      SpotLight.SampleAll(worldPosition, pixel, viewDir, NoV, LambdaV);

  float3 E = prefilteredDFG(pixel.PerceptualRoughness, pixel.Fresnel0, NoV);
  float3 diffuse = ComputeDiffuseColor(pixel.Albedo, pixel.Metallic);

  float3 irradiance = float3(0.0, 0.0, 0.0); // Some ambient lighting
  irradiance *= INV_PI;
  float3 radiance = irradiance; // Specular IBL contribution
  irradiance *= (1.0f.xxx - E) * diffuse;
  radiance *= E;

  incomingLight += irradiance + radiance;

  out.color = float4(incomingLight, 1.0f);

  return out;
}
