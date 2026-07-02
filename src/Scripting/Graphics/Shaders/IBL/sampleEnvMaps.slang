import constants;
import utils;
import camera;
import helpers;
import physicallyBased;

Sampler2D DepthTexture;
Sampler2D NormalTexture;
Sampler2D AlbedoTexture;
Sampler2D MaterialTexture;

uniform bool SpecularEnabled;

#include "fullscreen.slang"

struct FragmentInput {
  float2 VertexCoords : TEXCOORD0;
};

struct FragmentOutput {
  float4 Radiance : SV_Target0;
  float4 Irradiance : SV_Target1;
};

struct LightProbe {
  float3 position;
  int index;

  float radius;
  float innerRadius;
};

float sdSphere(float3 center, float radius, float3 point) {
  return distance(center, point) - radius;
}

struct LightProbeSampleInfo {
  float3 radiance;
  float3 irradiance;
  float influence;
};

// TODO: Volume texture for the world, storing an index and count into
// a list of indices for probes that influence that cell, and then sample the probes in the shader
uniform int LightProbeCount;
StructuredBuffer<LightProbe> Probes;

Sampler2DArray SpecularTextures;
Sampler2DArray IrradianceTextures;

LightProbeSampleInfo SampleLightProbe(int probeIndex, float3 worldPosition,
                                      float3 normal, float3 reflectedRay,
                                      float perceptualRoughness) {
  float influence = 1.0;
  LightProbe probe = Probes[probeIndex];

  float signedDistMax = sdSphere(probe.position, probe.radius, worldPosition);
  if (signedDistMax > 0.0)
    return LightProbeSampleInfo(float3(0.0), float3(0.0), 0.0);

  float signedDistMin =
      sdSphere(probe.position, probe.innerRadius, worldPosition);
  if (signedDistMin > 0.0) {
    influence =
        1.0 - saturate(signedDistMin / (probe.radius - probe.innerRadius));
  }

  if (influence <= 0.0)
    return LightProbeSampleInfo(float3(0.0), float3(0.0), 0.0);

  float3 reflected = prefilteredRadiance(
      SpecularTextures, float4(reflectedRay, probe.index), perceptualRoughness);
  float3 irradiance =
      prefilteredIrradiance(IrradianceTextures, float4(normal, probe.index))
          .rgb;

  return LightProbeSampleInfo(reflected, irradiance, influence);
}

LightProbeSampleInfo SampleLightProbes(float3 worldPosition, float3 normal,
                                       float3 reflectedRay,
                                       float perceptualRoughness) {
  LightProbeSampleInfo totalInfo;
  totalInfo.radiance = float3(0.0);
  totalInfo.irradiance = float3(0.0);
  totalInfo.influence = 0.0;

  for (int i = 0; i < LightProbeCount; i++) {
    LightProbeSampleInfo info = SampleLightProbe(
        i, worldPosition, normal, reflectedRay, perceptualRoughness);

    totalInfo.radiance += info.radiance * info.influence;
    totalInfo.irradiance += info.irradiance * info.influence;
    totalInfo.influence += info.influence;
  }

  return totalInfo;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
  float depth = DepthTexture.Sample(input.VertexCoords).r;

  if (depth == DepthFarPlane) {
    // TODO: Or discard?
    FragmentOutput output;
    output.Radiance = float4(0.0, 0.0, 0.0, 1.0);
    output.Irradiance = float4(0.0, 0.0, 0.0, 1.0);
    return output;
  }

  float3 worldPosition = camera.GetWorldPosition(input.VertexCoords, depth);

  float3 normal =
      normalize(NormalTexture.Sample(input.VertexCoords).xyz * 2.0 - 1.0);

  // metallic, perceptual roughness, reflectance, stencil
  float4 material = MaterialTexture.Sample(input.VertexCoords);
  float3 albedo = AlbedoTexture.Sample(input.VertexCoords).rgb;

  float3 viewDir = normalize(worldPosition - camera.Position);
  float3 f0 = ComputeFresnel0(albedo, material.r, material.b);
  float3 diffuse = ComputeDiffuseColor(albedo, material.r);
  float NoV = ClampNoV(dot(normal, -viewDir));
  float3 E = prefilteredDFG(material.g, f0, NoV);

  float3 reflectedRay = reflect(viewDir, normal);

  LightProbeSampleInfo probeInfo =
      SampleLightProbes(worldPosition, normal, reflectedRay, material.g);

  if (probeInfo.influence >= 1.0) {
    probeInfo.radiance /= probeInfo.influence;
    probeInfo.irradiance /= probeInfo.influence;
  } else // need skybox influence
  {
    float skyboxInfluence = 1.0 - probeInfo.influence;

    float3 reflected = prefilteredRadiance(
        SpecularTextures, float4(reflectedRay, 0.0), material.g);
    probeInfo.radiance = reflected * skyboxInfluence + probeInfo.radiance;

    float3 irradiance =
        prefilteredIrradiance(IrradianceTextures, float4(normal, 0.0));

    probeInfo.irradiance = irradiance * skyboxInfluence + probeInfo.irradiance;
  }

  probeInfo.irradiance *= INV_PI;

  if (!SpecularEnabled) {
    probeInfo.radiance *= 0.0;
    probeInfo.irradiance *=
        (1.0 - E) *
        albedo; // since we turn off specular, fully metallic objects will be black
    // to solve this, we multiply by albedo instead of diffuse
  } else {
    probeInfo.radiance *= E;
    probeInfo.irradiance *= (1.0 - E) * diffuse;
  }

  // Radiance = probeInfo.radiance;
  // Irradiance = probeInfo.irradiance;
  FragmentOutput output;
  output.Radiance = float4(probeInfo.radiance, 1.0f);
  output.Irradiance = float4(probeInfo.irradiance, 1.0f);

  return output;
}
