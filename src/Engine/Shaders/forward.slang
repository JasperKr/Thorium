module Forward;

import camera;

struct VertexStageOutput
{
    float4 sv_position : SV_Position;
    float4x2 TexCoords : Texture_Coordinates;
    half4 normal : Normal;
    half4 tangent : Tangent;
    float4 color       : Color;
};

struct VertexInput
{
    float3 Position : POSITION;
    uint4 TexCoord : Texture_Coordinates;
    uint2 NormalTangent : Normal_Tangent;
    uint Color    : Packed_Color;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input)
{
    VertexStageOutput out;
    out.sv_position = mul(camera.ViewProjectionMatrix, float4(input.Position, 1.0));

    float2 uv0 = unpackHalf2x16ToFloat(input.TexCoord.x);
    float2 uv1 = unpackHalf2x16ToFloat(input.TexCoord.y);
    float2 uv2 = unpackHalf2x16ToFloat(input.TexCoord.z);
    float2 uv3 = unpackHalf2x16ToFloat(input.TexCoord.w);

    out.TexCoords = float4x2(uv0, uv1, uv2, uv3);
    out.normal = unpackNormal(input.NormalTangent.x);
    out.tangent = unpackTangent(input.NormalTangent.y);

    out.color = input.Color;
    return out;
}

struct FragmentInput
{
    float2 tex_coord : Texture_Coordinates;
    float4 color : COLOR;
};

Sampler2D MainTexture;

[shader("fragment")]
float4 fragmentMain(FragmentInput input)
{
  return float4(float3(1.0), MainTexture.Sample(input.tex_coord).r) * input.color;
}
