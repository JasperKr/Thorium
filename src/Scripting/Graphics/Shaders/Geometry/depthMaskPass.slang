module depthMaskPrepass;

import camera;
import material;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float2 tex_coord : TEXCOORDS;
};

struct VertexInput {
  float3 Position : POSITION;
  float2 TexCoords : TEXCOORD;
};

uniform float4x4 ModelMatrix;
Sampler2D AlbedoTexture;

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
  VertexStageOutput out;

  float4 worldPos = mul(float4(input.Position, 1.0), ModelMatrix);
  out.sv_position = mul(camera.ViewProjectionMatrix, worldPos);
  out.tex_coord = input.TexCoords;

  return out;
}

struct FragmentInput {
  float2 tex_coord : TEXCOORDS;
};

[shader("fragment")]
void fragmentMain(FragmentInput input) {
  float4 texColor = AlbedoTexture.Sample(input.tex_coord);
  MaterialData matData = MaterialBuffer[MaterialBufferIndex];

  float alpha = texColor.a * matData.albedoFactor.a;

  // No need to check if material is alpha masked
  // Since this shader is only used for alpha masked materials
  if (alpha <= matData.alphaCutoff) {
    discard;
  }
}
