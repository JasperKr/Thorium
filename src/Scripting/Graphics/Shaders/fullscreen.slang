struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float2 tex_coord : TEXCOORDS;
};

struct VertexInput {
  uint VertexID : SV_VertexID;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
  VertexStageOutput out;

  float2 uvs = float2((input.VertexID << 1) & 2, input.VertexID & 2);
  float4 vertexPosition = float4(uvs * 2.0f - 1.0f, 0.0f, 1.0f);

  out.sv_position = vertexPosition;
  out.tex_coord = uvs;

  return out;
}
