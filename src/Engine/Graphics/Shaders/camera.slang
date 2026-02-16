module Camera;

public struct CameraData {
  public float4 Position;
  public float4x4 ViewMatrix;
  public float4x4 InverseViewMatrix;
  public float4x4 RotationMatrix;
  public float4x4 InverseRotationMatrix;
  public float4x4 ViewProjectionMatrix;
  public float4x4 InverseViewProjectionMatrix;
  public float4x4 RotationProjectionMatrix;
  public float4x4 InverseRotationProjectionMatrix;
};

public ParameterBlock<CameraData> camera;