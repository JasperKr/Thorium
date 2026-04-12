module camera;

public struct CameraData
{
  public float4x4 ViewMatrix;
  public float4x4 InverseViewMatrix;
  public float4x4 ProjectionMatrix;
  public float4x4 InverseProjectionMatrix;
  public float4x4 ViewProjectionMatrix;
  public float4x4 InverseViewProjectionMatrix;
  public float4x4 RotationProjectionMatrix;
  public float4x4 InverseRotationProjectionMatrix;
  public float3 Position;
  public float Near;
  public float Far;
  public float NearMulFar;
  public float FarMinusNear;
  public uint HistoryInvalidated;
  public float2 Jitter;
  public uint ProjectionType;
  public uint ShadowCascadeCount;
};

public cbuffer CameraData {
  public CameraData camera;
  public CameraData previousCamera;
};
