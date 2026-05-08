module directionalLight;

import light;

public struct DirectionalLight {
  public Light Base;
  public float2 Size;
}

uniform DirectionalLight directionalLight;
