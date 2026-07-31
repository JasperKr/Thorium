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
RWTexture2D<uint> OutputTexture;

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
  OutputTexture.GetDimensions(width, height);

  if (id.x >= width || id.y >= height) {
    return;
  }

  id.z += PushConstants.LayerOffset;

  float2 uv = (float2(id.xy) + 0.5) / float2(width, height);

  float3 normal = NormalTexture.SampleLevelZero(uv).xyz;
  normal = normalize(normal * 2.0f - 1.0f);
  float depth = DepthTexture.SampleLevelZero(uv).x;

  if (depth == DepthFarPlane || depth == DepthNearPlane) {
    OutputTexture.Store(id.xy, 1);
    return;
  }

  float3 worldPosition = camera.GetWorldPosition(uv, depth);
  uint2 screenCoords = uint2(id.xy);
  uint randomState = PushConstants.FrameRandomState + screenCoords.x * 1702 +
                     screenCoords.y * 40301;

#ifdef SHADER_LIGHT_TYPE_DIRECTIONAL
  bool inShadow = DirectionalLight::TraceShadow(
      worldPosition, PushConstants.LightIndex + id.z, normal, screenCoords,
      randomState);
#elif defined(SHADER_LIGHT_TYPE_POINT)
  bool inShadow =
      PointLight::TraceShadow(worldPosition, PushConstants.LightIndex + id.z,
                              normal, screenCoords, randomState);
#elif defined(SHADER_LIGHT_TYPE_SPOT)
  bool inShadow =
      SpotLight::TraceShadow(worldPosition, PushConstants.LightIndex + id.z,
                             normal, screenCoords, randomState);
#elif defined(SHADER_LIGHT_TYPE_SPHERE)
  bool inShadow =
      SphereLight::TraceShadow(worldPosition, PushConstants.LightIndex + id.z,
                               normal, screenCoords, randomState);
#elif defined(SHADER_LIGHT_TYPE_RECTANGLE)
  bool inShadow = RectangleLight::TraceShadow(
      worldPosition, PushConstants.LightIndex + id.z, normal, screenCoords,
      randomState);
#else
  bool inShadow = false;
#endif

  uint shadowBit = inShadow ? (1U << id.z) : 0;

  InterlockedOr(OutputTexture[id.xy], shadowBit);
}
