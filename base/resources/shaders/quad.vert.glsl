#version 450

layout(set = 1, binding = 0) uniform transforms_uniform_block {
  mat4 viewToClip;
  mat4 worldToView;
};

void main() {
  float x = float((gl_VertexIndex >> 1) & 1);
  float y = float(1 - (gl_VertexIndex & 1));
  
  x = 2.0 * x - 1.0;
  y = 2.0 * y - 1.0;

  vec4 model_position = vec4(x, y, -1.0, 1.0);
  
  gl_Position = viewToClip * worldToView * model_position;
  
  gl_Position.y = -gl_Position.y; // invert y axis since vulkan's coordinate system is inverted on Y
  gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0; // map NDC [-1,1] to NDC [0, 1]
}
