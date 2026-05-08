module rectangleLight;

import light;

public struct RectangleLight {
  public Light Base;
  public float2 Size;
  public float Range;
}

uniform uint32_t numRectangleLights;
StructuredBuffer<RectangleLight> rectangleLights;
