import camera;
import utils;
import constants;

struct FragmentInput {
  float2 tex_coord : TEXCOORDS;
};

#define ERP

#if defined(CUBE_MAP)
SamplerCube SkyboxTexture;
#elif defined(ERP)
Sampler2D SkyboxTexture;
#endif
Sampler2D DepthTexture;

#include "fullscreen.slang"

float2 GetERPUvs(float3 rayDirection) {
  float2 uv;
  uv.x = atan2(rayDirection.z, rayDirection.x) * INV_TWO_PI + 0.5;
  uv.y = asin(rayDirection.y) * INV_PI + 0.5;
  return uv;
}

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  float depth = DepthTexture.SampleLevel(input.tex_coord, 0.0).r;
  // Reverse z, so 0 = far plane and 1 = near plane
  if (depth != 0.0) {
    discard;
  }

  float3 rayDirection =
      GetRayDirection(input.tex_coord, camera.ProjectionMatrix);

  // #if defined(CUBE_MAP)
  //   return SkyboxTexture.Sample(rayDirection);
  // #elif defined(ERP)
  //   float2 uv = GetERPUvs(rayDirection);
  //   return SkyboxTexture.Sample(uv);
  // #endif

  return float4(saturate(rayDirection * 0.5 + 0.5), 1.0);
}
