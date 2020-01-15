#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 frag_TexCoord;
layout(location = 1) in vec3 frag_Color;

layout(location = 0) out vec4 out_Color;

layout(set=0, binding=0) uniform samplerCube unif_Sampler;

void main() {
  vec4 color = texture(unif_Sampler, frag_TexCoord) * vec4(frag_Color, 1.0);
  out_Color = color;
}
