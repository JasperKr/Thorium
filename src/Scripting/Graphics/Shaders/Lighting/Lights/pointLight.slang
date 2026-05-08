module pointLight;

import light;

public struct PointLight : ILight {
  public Light Base;
  public float Range;

  public Light GetLight() { return Base; }
}

uniform uint32_t PointLightCount;
StructuredBuffer<PointLight> PointLights;
