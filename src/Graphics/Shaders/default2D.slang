// Default shader

module default2D;

struct VertexStageOutput
{
    float4 sv_position : SV_Position;
    float2 tex_coord   : TEXCOORD0;
    float4 color       : COLOR;
};

struct VertexInput
{
    float2 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input)
{
    VertexStageOutput out;
    out.sv_position = float4(input.Position, 1.0);
    out.color = input.Color;
    out.tex_coord = input.TexCoord;
    return out;
}

struct FragmentInput
{
    float2 tex_coord : TEXCOORD0;
    float4 color : COLOR;
};

Sampler2D MainTexture;

[shader("fragment")]
float4 fragmentMain(FragmentInput input)
{
  float4 texColor = MainTexture.Sample(input.tex_coord);
  return input.color * texColor;
}
