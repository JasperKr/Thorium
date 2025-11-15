// fragment shader for ImGui rendering

#version 450

layout (location = 0) in vec4 fragColor;
layout (location = 1) in vec2 fragTexCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0, set = 0) uniform sampler2D textureSampler;

void main() {
  vec4 texColor = texture(textureSampler, fragTexCoord);
  outColor = fragColor * texColor;
}