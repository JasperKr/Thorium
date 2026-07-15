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
  float4 screen_position : SV_Position;
};

struct PushConstantBuffer {
  uint FrameRandomState;
};

[[vk::push_constant]]
PushConstantBuffer PushConstants;

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
  MaterialChannel material =
      MaterialChannel(MaterialTexture.Sample(input.tex_coord));
  float3 emissive = EmissiveTexture.Sample(input.tex_coord).rgb;
  float depth = DepthTexture.Sample(input.tex_coord).r;

  if (depth == DepthFarPlane) {
    discard;
  }

  float3 normal = normalize(normalData.xyz * 2.0f - 1.0f);

  float3 incomingLight = emissive.rgb;

  float3 viewDir = -camera.GetRayDirection(input.tex_coord);
  float3 worldPosition = camera.GetWorldPosition(input.tex_coord, depth);

  Pixel pixel;
  pixel.Normal = normal;
  pixel.Albedo = albedo.rgb;
  pixel.Metallic = material.metallic;
  pixel.PerceptualRoughness = material.perceptualRoughness;
  pixel.Roughness = material.roughness;
  pixel.Fresnel0 =
      ComputeFresnel0(albedo, material.metallic, material.reflectance);

  float NoV = ClampNoV(dot(normal, viewDir));
  float LambdaV = ComputeLambdaV(pixel.Roughness, NoV);

  uint state = uint(input.screen_position.x) ^ uint(input.screen_position.y);
  state ^= PushConstants.FrameRandomState;

  uint2 screenCoords = uint2(input.screen_position.xy);

  incomingLight += DirectionalLight.SampleAll(worldPosition, pixel, viewDir,
                                              NoV, LambdaV, state, screenCoords,
                                              PushConstants.FrameRandomState);
  // incomingLight +=
  //     PointLight.SampleAll(worldPosition, pixel, viewDir, NoV, LambdaV, state,
  //                          screenCoords, PushConstants.FrameRandomState);
  // incomingLight +=
  //     SpotLight.SampleAll(worldPosition, pixel, viewDir, NoV, LambdaV, state,
  //                         screenCoords, PushConstants.FrameRandomState);

  out.color = float4(incomingLight, 1.0f);

  return out;
}
