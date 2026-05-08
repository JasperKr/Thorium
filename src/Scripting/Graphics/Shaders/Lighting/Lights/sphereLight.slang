module sphereLight;

import light;

public struct SphereLight : ILight {
  public Light Base;
  public float Range;
  public float Radius;

  public Light GetLight() { return Base; }
}

uniform uint32_t SphereLightCount;
StructuredBuffer<SphereLight> SphereLights;
