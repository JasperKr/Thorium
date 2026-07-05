module lineDrawer;

struct VertexInput {
  float3 start : POSITION;
  float3 end : END_POSITION;
  float4 color : COLOR;
  float thickness : THICKNESS;
};

struct VertexOutput {
  float4 position : SV_POSITION;
  float4 color : COLOR;
  float2 tex_coord : TEXCOORDS;
};

import utils;
import camera;

struct PushConstantsBuffer {
  float2 viewportSize;
};

[[vk::push_constant]]
PushConstantsBuffer PushConstants;

static const float2 corners[6] = { { 0, 1 }, { 1, 1 }, { 1, 0 },
                                   { 0, 1 }, { 1, 0 }, { 0, 0 } };

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint vertexID: SV_VertexID) {
  VertexOutput output;

  float4 clip = mul(camera.ViewProjectionMatrix, float4(input.start, 1.0));
  float3 ndcStart = clip.xyz / clip.w;

  clip = mul(camera.ViewProjectionMatrix, float4(input.end, 1.0));
  float3 ndcEnd = clip.xyz / clip.w;

  float2 dir = normalize(ndcEnd.xy - ndcStart.xy);

  output.color = input.color;

  // Triangle list layout for 6 vertices per instance:
  // 0:(0,1), 1:(1,1), 2:(1,0), 3:(0,1), 4:(1,0), 5:(0,0)
  float2 texCoord = corners[vertexID % 6];

  float2 offset = float2(-dir.y, dir.x) * input.thickness * 0.5 /
                  PushConstants.viewportSize;
  float2 ndcPos = lerp(ndcStart.xy, ndcEnd.xy, texCoord.x) +
                  offset * (1.0 - 2.0 * texCoord.y);

  output.position = float4(ndcPos, 0.0, 1.0);

  output.tex_coord = texCoord;

  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) {
  float4 color = input.color;
  color.w *= input.tex_coord.y;
  return color;
}
