#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 frag_Color;
layout(location = 0) out vec4 out_Color;

void main() {
    out_Color = vec4(frag_Color, 1.0);
}