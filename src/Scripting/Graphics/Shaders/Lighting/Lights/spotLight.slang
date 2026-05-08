module spotLight;

import light;

public struct SpotLight {
  public Light Base;
  public float Range;
  public float InnerConeAngle;
  public float OuterConeAngle;
}

uniform uint32_t numSpotLights;
StructuredBuffer<SpotLight> spotLights;
