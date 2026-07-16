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
  sampleCoord.xy -= UnpackMotionVector(MotionTexture.Load(int3(id.xy, 0)).xy);

  float currentShadow = PreviousOutputTexture.SampleLevelZero(sampleCoord);
  float newShadow = lerp(currentShadow, inShadow ? 0.0 : 1.0, 1.0F / 16.0F);

  if (any(sampleCoord < 0.0) || any(sampleCoord.xy >= 1.0)) {
    newShadow = inShadow ? 0.0 : 1.0;
  }

  int3 storeCoord = int3(id.xy, id.z + PushConstants.LayerOffset);
  OutputTexture.Store(storeCoord, newShadow);
}
