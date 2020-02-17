#version 450
#extension GL_ARB_separate_shader_objects : enable

// closure definitions can be found in the following document,
// which this shader references:
//
// https://autodesk.github.io/standard-surface/#figure_opacity

layout(location = 0) in vec2 frag_TexCoord;
layout(location = 1) in vec3 frag_Color;

layout(location = 0) out vec4 out_Color;

layout(set=0, binding=0) uniform sampler2D unif_Samplers[2];

layout(push_constant) uniform sampler_index_buffer {
  vec3 opacity; // colored
  float transparency;
  int samplerIndex;
};

void main() {
  //
  // Transparency closure.
  //
  //
  // The inverse of each channel is computed here:
  // a dark blue, for example might be defined as:
  // opacity = (0, 0, 0.25)
  //
  // The inverse operation here will result in:
  // inverse = 1.0 - opacity = (1, 1, 0.75)
  // the end result is a yellowish off-white hue.
  //
  // The transparency can be modified to darken
  // the color if desired.
  //
  vec3 t = (vec3(1.0) - opacity) * transparency;

  //
  // Here is the color component of the opacity closure
  //
  vec3 c = opacity * frag_Color;
  
  vec3 standard_surface = t + c;
  vec4 tex = texture(unif_Samplers[samplerIndex & 0x1], frag_TexCoord);
  out_Color = tex * vec4(standard_surface, 1.0);
}
