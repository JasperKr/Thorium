module directionalLight;

import light;
import pixel;
import utils;
import physicallyBased;
import raytracing;

public struct DirectionalLight : ILight {
  public Light Base;

  public Light GetLight() { return Base; }

  public static override float3 Sample(const float3 WorldPosition,
                                       const uint LightIndex, const Pixel pixel,
                                       const float3 ViewDirection,
                                       const float NoV, const float LambdaV) {
    DirectionalLight light = DirectionalLights[LightIndex];
    float3 Color = light.Base.Color * light.Base.Intensity;

    const float3 LocalForward = float3(0.0f, 0.0f, 1.0f);
    const float3 WorldForward =
        normalize(Rotate(LocalForward, light.Base.Rotation));

    float NdotL = saturate(dot(pixel.Normal, WorldForward));
    float3 Diffuse = ComputeDiffuseColor(pixel.Albedo, pixel.Metallic);

    float3 brdf =
        SampleBRDF(ViewDirection, WorldForward, NoV, pixel.Normal,
                   pixel.Roughness, pixel.Fresnel0, Diffuse, NdotL, LambdaV);

    RayDesc ray;
    ray.Origin = WorldPosition + pixel.Normal * 1e-3f;
    ray.Direction = WorldForward;
    ray.TMin = 0.0f;
    ray.TMax = 1e3f;

    bool inShadow = TraceShadowRay(SceneBVH, ray);

    return Color * brdf * (inShadow ? 0.0f : 1.0f);
  }

  public static override float3 SampleAll(const float3 WorldPosition,
                                          const Pixel pixel,
                                          const float3 ViewDirection,
                                          const float NoV,
                                          const float LambdaV) {
    float3 result = float3(0.0f);
    for (uint i = 0; i < DirectionalLightCount; i++) {
      result += Sample(WorldPosition, i, pixel, ViewDirection, NoV, LambdaV);
    }
    return result;
  }
}

uniform uint32_t DirectionalLightCount;
StructuredBuffer<DirectionalLight> DirectionalLights;
