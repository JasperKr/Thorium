import raytracing;
import camera;
import utils;
import constants;
import random;

#ifdef SHADER_LIGHT_TYPE_DIRECTIONAL
import directionalLight;
#elif defined(SHADER_LIGHT_TYPE_POINT)
import pointLight;
#elif defined(SHADER_LIGHT_TYPE_SPOT)
import spotLight;
#elif defined(SHADER_LIGHT_TYPE_SPHERE)
import sphereLight;
#elif defined(SHADER_LIGHT_TYPE_RECTANGLE)
import rectangleLight;
#else
#error "No light type defined for shadow sampling shader"
#endif

#define SHADER_TYPE_SHADOW_SAMPLING

Sampler2D DepthTexture;
Sampler2D PreviousDepthTexture;

Sampler2D NormalTexture;
Sampler2D MotionTexture;
restrict WTexture2DArray<float> OutputTexture;
Sampler2DArray<float> PreviousOutputTexture;

struct PushConstantBuffer {
  uint LightIndex;
  uint LayerOffset;
  uint FrameRandomState;
};

[[vk::push_constant]]
PushConstantBuffer PushConstants;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 id: SV_DispatchThreadID) {
  uint width;
  uint height;
  uint layerCount;
  OutputTexture.GetDimensions(width, height, layerCount);

  if (id.x >= width || id.y >= height) {
    return;
  }

  float2 uv = (float2(id.xy) + 0.5) / float2(width, height);

  float3 normal = NormalTexture.SampleLevelZero(uv).xyz;
  normal = normalize(normal * 2.0f - 1.0f);
  float depth = DepthTexture.SampleLevelZero(uv).x;

  if (depth == DepthFarPlane || depth == DepthNearPlane) {
    OutputTexture.Store(int3(id.xy, id.z + PushConstants.LayerOffset), 1.0);
    return;
  }

  float3 worldPosition = camera.GetWorldPosition(uv, depth);
  uint2 screenCoords = uint2(id.xy);

#ifdef SHADER_LIGHT_TYPE_DIRECTIONAL
  bool inShadow = DirectionalLight::TraceShadow(
      worldPosition, PushConstants.LightIndex + id.z, normal, screenCoords,
      PushConstants.FrameRandomState);
#elif defined(SHADER_LIGHT_TYPE_POINT)
  bool inShadow = PointLight::TraceShadow(
      worldPosition, PushConstants.LightIndex + id.z, normal, screenCoords,
      PushConstants.FrameRandomState);
#elif defined(SHADER_LIGHT_TYPE_SPOT)
  bool inShadow = SpotLight::TraceShadow(
      worldPosition, PushConstants.LightIndex + id.z, normal, screenCoords,
      PushConstants.FrameRandomState);
#elif defined(SHADER_LIGHT_TYPE_SPHERE)
  bool inShadow = SphereLight::TraceShadow(
      worldPosition, PushConstants.LightIndex + id.z, normal, screenCoords,
      PushConstants.FrameRandomState);
#elif defined(SHADER_LIGHT_TYPE_RECTANGLE)
  bool inShadow = RectangleLight::TraceShadow(
      worldPosition, PushConstants.LightIndex + id.z, normal, screenCoords,
      PushConstants.FrameRandomState);
#else
  bool inShadow = false;
#endif

  float3 sampleCoord = float3(uv, id.z + PushConstants.LayerOffset);
  int3 storeCoord = int3(id.xy, id.z + PushConstants.LayerOffset);
  sampleCoord.xy -= UnpackMotionVector(MotionTexture.Load(int3(id.xy, 0)).xy);

  if (any(sampleCoord < 0.0) || any(sampleCoord.xy >= 1.0)) {
    OutputTexture.Store(storeCoord, inShadow ? 0.0 : 1.0);
    return;
  }

  float previousDepth = PreviousDepthTexture.SampleLevelZero(sampleCoord.xy).x;
  float3 previousWorldPosition =
      previousCamera.GetWorldPosition(sampleCoord.xy, previousDepth);

  float influence = 1.0F / 20.0F;

  // Increase influence of current sample based on distance between current and previous world position
  influence += length(worldPosition - previousWorldPosition);
  influence = clamp(influence, 0.0F, 1.0F);

  float currentShadow = PreviousOutputTexture.SampleLevelZero(sampleCoord);

  float2 pixelSize = float2(1.0F / width, 1.0F / height);
  float sum = 0.0;
  float sum2 = 0.0;
  uint count = 0;

  [unroll]
  for (int x = -2; x <= 2; ++x) {
    [unroll]
    for (int y = -2; y <= 2; ++y) {
      if (x == 0 && y == 0) {
        continue;
      }

      float2 neighbourCoord = sampleCoord.xy + float2(x, y) * pixelSize;
      if (any(neighbourCoord < 0.0) || any(neighbourCoord >= 1.0)) {
        continue;
      }

      count++;

      float neighbourShadow = PreviousOutputTexture.SampleLevelZero(
          float3(neighbourCoord, sampleCoord.z));

      sum += neighbourShadow;
      sum2 += neighbourShadow * neighbourShadow;
    }
  }

  float mean = sum / count;
  float variance = max(sum2 / count - mean * mean, 0.0);
  float sigma = sqrt(variance);
  const float gamma = 1.5;

  // If the new shadow value is outside the neighbourhood range, clamp it to the range
  currentShadow =
      clamp(currentShadow, mean - gamma * sigma, mean + gamma * sigma);

  float newShadow = lerp(currentShadow, inShadow ? 0.0 : 1.0, influence);

  // If the stored shadow value is 1 / 255 or something and the new shadow value is 0 / 255
  // It might keep rounding up to 1, causing ghost shadows. This forces the shadow value to favour the new shadow value
  // If it is stuck.
  uint currentRoundedShadow = uint(round(newShadow * 255.0F));
  uint previousRoundedShadow = uint(round(currentShadow * 255.0F));

  const float kQuantizationStep = 1.0 / 255.0;

  if (currentRoundedShadow == previousRoundedShadow) {
    float bias = inShadow ? -kQuantizationStep : kQuantizationStep;
    newShadow = saturate(newShadow + bias);
  }

  OutputTexture.Store(storeCoord, newShadow);
}
