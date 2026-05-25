module depthPrepass;

import camera;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
};

struct VertexInput {
  float3 Position : POSITION;
};

uniform float4x4 ModelMatrix;

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
  VertexStageOutput out;

  float4 worldPos = mul(float4(input.Position, 1.0), ModelMatrix);
  out.sv_position = mul(camera.ViewProjectionMatrix, worldPos);

  return out;
}
