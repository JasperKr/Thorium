module depthPrepass;

import camera;

import transform;
struct PushConstantBuffer {
  uint ModelTransformIndex;
};

[[vk::push_constant]]
PushConstantBuffer PushConstants;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
};

struct VertexInput {
  float3 Position : POSITION;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
  Transform transform = GetModelTransform(PushConstants.ModelTransformIndex);

  VertexStageOutput out;

  float4 worldPos = mul(float4(input.Position, 1.0), transform.ModelMatrix);
  out.sv_position = mul(camera.ViewProjectionMatrix, worldPos);

  return out;
}
