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
  uv.x = atan2(-rayDirection.z, rayDirection.x) * INV_TWO_PI + 0.5;
  uv.y = asin(-rayDirection.y) * INV_PI + 0.5;
  return uv;
}

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  float depth = DepthTexture.SampleLevelZero(input.tex_coord).r;
  // Reverse z, so 0 = far plane and 1 = near plane
  if (depth != 0.0) {
    discard;
  }

  float3 rayDirection =
      GetRayDirection(input.tex_coord, camera.InverseRotationProjectionMatrix);

#if defined(CUBE_MAP)
  return SkyboxTexture.Sample(rayDirection);
#elif defined(ERP)
  float2 uv = GetERPUvs(rayDirection);

  float2 duvdx = ddx(uv);
  float2 duvdy = ddy(uv);

  // fix seam discontinuity
  if (abs(duvdx.x) > 0.5)
    duvdx.x = -sign(duvdx.x) * (1.0 - abs(duvdx.x));
  if (abs(duvdy.x) > 0.5)
    duvdy.x = -sign(duvdy.x) * (1.0 - abs(duvdy.x));

  return SkyboxTexture.SampleGrad(uv, duvdx, duvdy);
#endif

  // return float4(saturate(rayDirection * 0.5 + 0.5), 1.0);
}
