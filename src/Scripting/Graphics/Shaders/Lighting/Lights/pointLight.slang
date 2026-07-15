module pointLight;

import light;
import pixel;
import physicallyBased;

public struct PointLight : ILight {
  public Light Base;
  public float Range;

  public Light GetLight() { return Base; }

  public static override float3 Sample(const float3 WorldPosition,
                                       const uint LightIndex, const Pixel pixel,
                                       const float3 ViewDirection,
                                       const float NoV, const float LambdaV,
                                       inout uint state, uint2 screenCoords,
                                       uint frameRandomState) {
    PointLight light = PointLights[LightIndex];

    float3 LightVector = light.Base.Position.xyz - WorldPosition;
    float Attenuation = AttenuateCusp(length(LightVector), light.Range, 4.0f);

    if (Attenuation <= 0.0f) {
      return float3(0.0f);
    }

    LightVector = normalize(LightVector);
    float NoL = saturate(dot(pixel.Normal, LightVector));

    float3 Diffuse = ComputeDiffuseColor(pixel.Albedo, pixel.Metallic);
    float3 brdf =
        SampleBRDF(ViewDirection, LightVector, NoV, pixel.Normal,
                   pixel.Roughness, pixel.Fresnel0, Diffuse, NoL, LambdaV);

    return light.Base.Color * brdf *
           (light.Base.Intensity * (NoL * Attenuation));
  }

  public static override float3 SampleAll(const float3 WorldPosition,
                                          const Pixel pixel,
                                          const float3 ViewDirection,
                                          const float NoV, const float LambdaV,
                                          inout uint state, uint2 screenCoords,
                                          uint frameRandomState) {
    return float3(0.0f);
  }
}

uniform uint32_t PointLightCount;
StructuredBuffer<PointLight> PointLights;
