module camera;

public cbuffer CameraData {
  public float4 CameraPosition;
  public float4x4 ViewMatrix;
  public float4x4 InverseViewMatrix;
  public float4x4 RotationMatrix;
  public float4x4 InverseRotationMatrix;
  public float4x4 ProjectionMatrix;
  public float4x4 InverseProjectionMatrix;
  public float4x4 ViewProjectionMatrix;
  public float4x4 InverseViewProjectionMatrix;
  public float4x4 RotationProjectionMatrix;
  public float4x4 InverseRotationProjectionMatrix;
};
