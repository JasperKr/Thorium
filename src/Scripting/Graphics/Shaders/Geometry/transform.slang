module transform;

public struct Transform {
  public float4x4 ModelMatrix;
  public float4x4 NormalMatrix;
};

[vk::binding(0, 3)]
public StructuredBuffer<Transform> ModelTransforms;

public Transform GetModelTransform(uint index) {
  return ModelTransforms[index];
}

public float4x4 GetModelMatrix(uint index) {
  return ModelTransforms[index].ModelMatrix;
}

public float4x4 GetNormalMatrix(uint index) {
  return ModelTransforms[index].NormalMatrix;
}
