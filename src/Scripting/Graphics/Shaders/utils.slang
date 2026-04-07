module Utils;

public func UnpackNormal(uint packedNormal) -> half4
{
  uint x = (packedNormal >> 0) & 0x3FF;
  uint y = (packedNormal >> 10) & 0x3FF;
  uint z = (packedNormal >> 20) & 0x3FF;

  half3 normal;
  normal.x = half((float(x) * (1.0f / 1023.0f)) * 2.0 - 1.0);
  normal.y = half((float(y) * (1.0f / 1023.0f)) * 2.0 - 1.0);
  normal.z = half((float(z) * (1.0f / 1023.0f)) * 2.0 - 1.0);

  return half4(normal, (half)1.0);
}

public func UnpackTangent(uint packedTangent) -> half4
{
  uint x = (packedTangent >> 0) & 0x3FF;
  uint y = (packedTangent >> 10) & 0x3FF;
  uint z = (packedTangent >> 20) & 0x3FF;

  half4 tangent;
  tangent.x = half((float(x) * (1.0f / 1023.0f)) * 2.0 - 1.0);
  tangent.y = half((float(y) * (1.0f / 1023.0f)) * 2.0 - 1.0);
  tangent.z = half((float(z) * (1.0f / 1023.0f)) * 2.0 - 1.0);
  tangent.w = half(packedTangent >> 30 & 1);

  return tangent;
}