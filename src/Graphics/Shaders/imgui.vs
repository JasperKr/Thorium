// vertex shader for ImGui rendering

#version 450

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec2 fragTexCoord;

layout(push_constant) uniform uPushConstant { vec2 uScale; vec2 uTranslate; } pc;

void main() {
  gl_Position = vec4(inPosition * pc.uScale + pc.uTranslate, 0, 1);

  fragColor = inColor;
  fragTexCoord = inTexCoord;
}