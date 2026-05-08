module sphereLight;

import light;

public struct SphereLight {
  public Light Base;
  public float Range;
  public float Radius;
}

uniform uint32_t numSphereLights;
StructuredBuffer<SphereLight> sphereLights;
