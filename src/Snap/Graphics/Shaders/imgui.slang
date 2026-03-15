// fragment shader for ImGui rendering

// #version 450

// layout (location = 0) in vec4 fragColor;
// layout (location = 1) in vec2 fragTexCoord;
// layout (location = 0) out vec4 outColor;

// layout (binding = 0, set = 0) uniform sampler2D textureSampler;

// void main() {
//   vec4 texColor = texture(textureSampler, fragTexCoord);
//   outColor = texColor * fragColor;
//   outColor.a = 1.0;
//   // outColor = fragColor;
// }

// Imgui shader

module imgui;

struct VertexStageOutput
{
    float4 sv_position : SV_Position;
    float2 tex_coord   : TEXCOORD0;
};

struct VertexInput
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input)
{
    VertexStageOutput out;
    out.sv_position = float4(input.Position, 1.0);
    out.tex_coord = input.TexCoord;
    return out;
}

struct FragmentInput
{
    float3 color : COLOR;
};

[shader("fragment")]



float4 fragmentMain(FragmentInput input)
{
  // float4 texColor = texture(sampler2D(textureSampler, input.tex_coord));
  return float4(input.color, 1.0);
}
