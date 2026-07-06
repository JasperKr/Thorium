import utils;

Sampler2D mainScene;

struct PushConstantsBuffer {
  float threshold;
};

[[vk::push_constant]]
PushConstantsBuffer PushConstants;

#include "fullscreen.slang"

[shader("fragment")]
float4 fragmentMain(float2 texCoord: TEXCOORDS) {
  float3 color = Sanitise(mainScene.SampleLevelZero(texCoord).rgb);

  float luminance = Luminance(color);

  float threshold = PushConstants.threshold;

  color *= clamp(log(max(1.0, luminance - threshold + 1.0)), 0.0, 10.0);

  return float4(color, 1.0);
}
