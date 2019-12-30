#version 450

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_TexCoord;

layout(location = 0) out vec2 frag_TexCoord;

layout(set = 1, binding = 0) uniform the_uniform_buffer {
  mat4 viewToClip;
  mat4 worldToView;
};

void main() {
  gl_Position = viewToClip * worldToView * vec4(in_Position, 1.0);
  gl_Position.y = -gl_Position.y;
  gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
  frag_TexCoord = in_TexCoord;
}
