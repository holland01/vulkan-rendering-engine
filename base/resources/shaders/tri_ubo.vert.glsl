#version 450

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_TexCoord;
layout(location = 2) in vec3 in_Color;

layout(location = 0) out vec2 frag_TexCoord;
layout(location = 1) out vec3 frag_Color;

layout(set = 1, binding = 0) uniform transforms_uniform_block {
  mat4 viewToClip;
  mat4 worldToView;
};


void main() {  
  gl_Position = viewToClip * worldToView * vec4(in_Position, 1.0);
  gl_Position.y = -gl_Position.y; // invert y axis since vulkan's coordinate system is inverted on Y
  gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0; // map NDC [-1,1] to NDC [0, 1]

  frag_TexCoord = in_TexCoord;
  frag_Color = in_Color;
}
