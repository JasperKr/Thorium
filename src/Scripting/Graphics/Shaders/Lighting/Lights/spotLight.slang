module spotLight;

import light;
import pixel;

public struct SpotLight : ILight {
  public Light Base;
  public float Range;
  public float InnerConeAngle;
  public float OuterConeAngle;

  public Light GetLight() { return Base; }

  public static override float3 SampleAll(const float3 WorldPosition,
                                          const Pixel pixel,
                                          const float3 ViewDirection,
                                          const float NoV,
                                          const float LambdaV) {
    return float3(0.0f);
  }
}

uniform uint32_t SpotLightCount;
StructuredBuffer<SpotLight> SpotLights;
