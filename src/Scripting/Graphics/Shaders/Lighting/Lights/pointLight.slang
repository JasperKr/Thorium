module pointLight;

import light;

public struct PointLight {
  public Light Base;
  public float Range;
}

uniform uint32_t numPointLights;
StructuredBuffer<PointLight> pointLights;
