// #include "constants.glsl"
// #include "cubemapFilter.glsl"

import helpers;
import cubemapHelpers;
import constants;

uniform int Slice;

// uniform highp sampler2DArray RadianceInput;
// layout(r11f_g11f_b10f) uniform restrict writeonly image2DArray IrradianceOutput;

Sampler2DArray RadianceInput;
RWTexture2DArray<float4> IrradianceOutput;

float3 calculateIrradiance(const float3 normal) {
  float3 irradiance = float3(0.0);

  float3 up = float3(0.0, 1.0, 0.0);
  float3 right = normalize(cross(up, normal));
  up = normalize(cross(normal, right));

  float sampleDelta = PI / 8.0;
  float nrSamples = 0.0;

  float2 dirYaw = float2(0.0, 1.0); // (cos(PI/2), sin(PI/2))
  float2 rot = float2(cos(sampleDelta), sin(sampleDelta));

  for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
    dirYaw = float2(dirYaw.x * rot.x - dirYaw.y * rot.y,
                    dirYaw.x * rot.y + dirYaw.y * rot.x);

    float2 dirPitch = float2(1.0, 0.0); // (cos(0), sin(0))

    for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
      dirPitch = float2(dirPitch.x * rot.x - dirPitch.y * rot.y,
                        dirPitch.x * rot.y + dirPitch.y * rot.x);

      // tangent space to world
      float3 sampleVec = dirPitch.y * dirYaw.x * right +
                         dirPitch.y * dirYaw.y * up + dirPitch.x * normal;
      float2 uv = Oct3To2(sampleVec) * 0.5 + 0.5;

      // irradiance += textureLod(RadianceInput, float3(uv, Slice), 5.0).rgb *
      //               dirPitch.x * dirPitch.y;

      irradiance += RadianceInput.SampleLevel(float3(uv, Slice), 5.0).rgb *
                    dirPitch.x * dirPitch.y;

      nrSamples++;
    }
  }

  return PI * irradiance / nrSamples;
}

#define BASE 32

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(int3 id: SV_DispatchThreadID) {
  if (id.x >= BASE || id.y >= BASE)
    return; // Out of bounds

  float2 uv = (float2(id.xy) + 0.5) / float2(BASE);

  float3 normal = DecodeOctahedron(uv);

  float3 irradiance = calculateIrradiance(normal);

  IrradianceOutput.Store(int3(id.xy, Slice), float4(irradiance, 1.0));
}
