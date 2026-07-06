Sampler2D mainScene;

struct PushConstantsBuffer {
  uint2 viewportSize;
  float intensity;
};

[[vk::push_constant]]
PushConstantsBuffer PushConstants;

float3 Sample(float2 coords) { return mainScene.SampleLevelZero(coords).rgb; }

float4 UpSample(float2 coords) {
  float3 color = float3(0.0);

  uint textureWidth = 0;
  uint textureHeight = 0;

  mainScene.GetDimensions(textureWidth, textureHeight);

  float2 inverseHalfSize = 1.0 / float2(textureWidth, textureHeight);

  color += Sample(coords) * 4.0;

  color += Sample(coords + inverseHalfSize);
  color += Sample(coords - inverseHalfSize);

  color += Sample(coords + float2(inverseHalfSize.x, -inverseHalfSize.y));
  color += Sample(coords + float2(-inverseHalfSize.x, inverseHalfSize.y));

  color += Sample(coords + float2(0.0, inverseHalfSize.y)) * 2.0;
  color += Sample(coords - float2(0.0, inverseHalfSize.y)) * 2.0;

  color += Sample(coords + float2(inverseHalfSize.x, 0.0)) * 2.0;
  color += Sample(coords - float2(inverseHalfSize.x, 0.0)) * 2.0;

  color *= 0.0625;
  color *= PushConstants.intensity;

  return float4(color, 1.0);
}

#include "fullscreen.slang"

struct FragmentInput {
  float2 tex_coord : TEXCOORDS;
};

[shader("fragment")]
float4 fragmentMain(FragmentInput input) { return UpSample(input.tex_coord); }
