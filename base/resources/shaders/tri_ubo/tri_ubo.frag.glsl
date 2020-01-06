#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 frag_TexCoord;
layout(location = 1) in vec3 frag_Color;

layout(location = 0) out vec4 out_Color;

layout(set=0, binding=0) uniform sampler2D unif_SamplerA;

void main() {
  vec4 color = texture(unif_SamplerA, frag_TexCoord) * vec4(frag_Color, 1.0);
  out_Color = color;
}
