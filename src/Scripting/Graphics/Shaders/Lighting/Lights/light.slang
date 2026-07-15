module light;

import pixel;

public struct Light {
  public float3 Color;
  public float Intensity;

  public float4 Position;
  public float4 Rotation;
};

public interface ILight {
  Light GetLight();

  static float3 Sample(const float3 WorldPosition, const uint32_t LightIndex,
                       const Pixel pixel, const float3 ViewDirection,
                       const float NoV, const float LambdaV, inout uint state,
                       uint2 screenCoords, uint frameRandomState) {
    return float3(0.0f);
  }

  static float3 SampleAll(const float3 WorldPosition, const Pixel pixel,
                          const float3 ViewDirection, const float NoV,
                          const float LambdaV, inout uint state,
                          uint2 screenCoords, uint frameRandomState) {
    return float3(0.0f);
  }
};

public float AttenuateCusp(float distance, float radius, float falloff) {
  float s = distance / radius;

  if (s >= 1.0) {
    return 0.0;
  }

  float s2 = s * s;
  float iS2 = 1.0 - s2;

  return (iS2 * iS2) / (1.0 + falloff * s);
}
