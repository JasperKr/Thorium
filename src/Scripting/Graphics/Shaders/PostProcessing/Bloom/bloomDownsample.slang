Sampler2D mainScene;

// better bloom weights, credit to `Schmleeb` on discord
const static float Weights[13] = float[13](
    0.125, 0.125, 0.125, 0.125,

    0.03125, 0.0625, 0.03125, 0.0625, 0.125, 0.0625, 0.03125, 0.0625, 0.03125);

struct PushConstantsBuffer {
  uint2 viewportSize;
};

float3 Sample(int i, float2 coords) {
  return Weights[i] * mainScene.SampleLevelZero(coords).rgb;
}

[[vk::push_constant]]
PushConstantsBuffer PushConstants;

float4 DownSample(float2 coords, float2 texelSize) {
  float2 texelSizeMul2 = texelSize * 2.0;

  float3 color = float3(0.0);

  color += Sample(0, coords + texelSize);
  color += Sample(1, coords - texelSize);
  color += Sample(2, coords + float2(-texelSize.x, texelSize.y));
  color += Sample(3, coords - float2(-texelSize.x, texelSize.y));

  color += Sample(4, coords - texelSizeMul2);
  color += Sample(5, coords + float2(0.0, -texelSizeMul2.y));
  color += Sample(6, coords + float2(texelSizeMul2.x, -texelSizeMul2.y));

  color += Sample(7, coords + float2(-texelSizeMul2.x, 0.0));
  color += Sample(8, coords);
  color += Sample(9, coords + float2(texelSizeMul2.x, 0.0));

  color += Sample(10, coords + float2(-texelSizeMul2.x, texelSizeMul2.y));
  color += Sample(11, coords + float2(0.0, texelSizeMul2.y));
  color += Sample(12, coords + texelSizeMul2);

  return float4(color, 1.0);
}

#include "fullscreen.slang"

[shader("fragment")]
float4 fragmentMain(float2 tex_coord: TEXCOORDS) {
  float2 texelSize = 1.0 / float2(PushConstants.viewportSize);
  texelSize *= 0.5; // We render at half-res, so pixels are twice as big

  return DownSample(tex_coord, texelSize);
}
