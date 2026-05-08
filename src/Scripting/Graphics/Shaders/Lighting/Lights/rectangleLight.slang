module rectangleLight;

import light;

public struct RectangleLight : ILight {
  public Light Base;
  public float2 Size;
  public float Range;

  public Light GetLight() { return Base; }
}

uniform uint32_t RectangleLightCount;
StructuredBuffer<RectangleLight> RectangleLights;
