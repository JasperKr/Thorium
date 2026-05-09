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
                       const float NoV, const float LambdaV) {
    return float3(0.0f);
  }

  static float3 SampleAll(const float3 WorldPosition, const Pixel pixel,
                          const float3 ViewDirection, const float NoV,
                          const float LambdaV) {
    return float3(0.0f);
  }
};
