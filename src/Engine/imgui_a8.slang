module ImGuiA8;

struct VertexStageOutput
{
    float2 sv_position : SV_Position;
    float2 tex_coord   : TEXCOORD0;
};

struct VertexInput
{
    float2 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

uniform float4x4 DefaultProjectionMatrix;

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input)
{
    VertexStageOutput out;
    out.tex_coord = input.TexCoord;
    return out;
}

struct FragmentInput
{
    float2 tex_coord : TEXCOORD0;
};

Sampler2D MainTexture;

[[vk::push_constant]]
cbuffer PushConstants
{
  float4 color;
};

[shader("fragment")]
float4 fragmentMain(FragmentInput input)
{
  return float4(color.rgb, MainTexture.Sample(input.tex_coord).r * color.a);
}
