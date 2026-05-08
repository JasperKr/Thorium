module spotLight;

import light;

public struct SpotLight : ILight {
  public Light Base;
  public float Range;
  public float InnerConeAngle;
  public float OuterConeAngle;

  public Light GetLight() { return Base; }
}

uniform uint32_t SpotLightCount;
StructuredBuffer<SpotLight> SpotLights;
