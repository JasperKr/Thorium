module directionalLight;

import light;
import pixel;
import utils;
import physicallyBased;

public struct DirectionalLight : ILight {
  public Light Base;
  public float2 Size;

  public Light GetLight() { return Base; }

  public static override float3 Sample(const float3 WorldPosition,
                                       const uint LightIndex, const Pixel pixel,
                                       const float3 ViewDirection,
                                       const float NoV, const float LambdaV) {
    DirectionalLight light = DirectionalLights[LightIndex];
    float3 Color = light.Base.Color * light.Base.Intensity;

    const float3 LocalForward = float3(0.0f, 0.0f, 1.0f);
    const float3 WorldForward = Rotate(LocalForward, light.Base.Rotation);

    float NdotL = saturate(dot(pixel.Normal, -WorldForward));
    float3 Diffuse = ComputeDiffuseColor(pixel.Albedo, pixel.Metallic);

    float3 brdf =
        SampleBRDF(ViewDirection, -WorldForward, NoV, pixel.Normal,
                   pixel.Roughness, pixel.Fresnel0, Diffuse, NdotL, LambdaV);

    return Color * brdf;
  }
}

uniform uint32_t DirectionalLightCount;
StructuredBuffer<DirectionalLight> DirectionalLights;
