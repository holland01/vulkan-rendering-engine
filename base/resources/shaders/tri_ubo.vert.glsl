#version 450

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_TexCoord;
layout(location = 2) in vec3 in_Color;
layout(location = 3) in vec3 in_Normal;

layout(location = 0) out vec2 frag_TexCoord;
layout(location = 1) out vec3 frag_Color;
layout(location = 2) out vec3 frag_Normal;
layout(location = 3) out vec3 frag_WorldPosition;

layout(set = 1, binding = 0) uniform transforms_uniform_block {
  mat4 viewToClip;
  mat4 worldToView;
};

layout(push_constant) uniform model_buffer {
  layout(offset = 64) mat4 modelToWorld;
} modelBuffer;

vec4 fixup_position(in vec4 p) {
  // invert y axis since vulkan's coordinate system is inverted on Y
  p.y = -p.y;
  p.z = (p.z + p.w) * 0.5; // map NDC [-1,1] to NDC [0, 1]
  return p;
}

void main() {
  vec4 worldPosition = modelBuffer.modelToWorld * vec4(in_Position, 1.0);
  
  gl_Position = fixup_position(viewToClip * worldToView * worldPosition);
  
  frag_TexCoord = in_TexCoord;
  frag_Color = in_Color;
  frag_Normal = mat3(modelBuffer.modelToWorld) * in_Normal;
  frag_WorldPosition = worldPosition.xyz;
}
