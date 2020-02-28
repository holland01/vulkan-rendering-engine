#version 450
#extension GL_ARB_separate_shader_objects : enable

// closure definitions can be found in the following document,
// which this shader references:
//
// https://autodesk.github.io/standard-surface/#figure_opacity
//
// Other resources:
//   - [2] https://learnopengl.com/PBR/Theory
//
//
// "Solid angle" - measured in steradians. Is a portion of a sphere,
// in the same sense that an "angle" measured in radians is
// a portion of a unit circle.
//
// "radiance" - the strength of the light
//
// Notes from learnopengl.com
//--------------------------------------------------------------------------
//
// At a high level, this is our lighting formula:
//
// input:
//
// int steps = 100;
// vec3 P    = ...; <- arbitrary point on surface
// vec3 Wo   = ...; <- direction from point to viewer's position
// vec3 N    = ...; <- surface normal
// float dW  = 1.0 / steps;
//
// output:
//
// float sum = 0.0;
//
// procedure:
//
// for(int i = 0; i < steps; ++i) {
//   vec3 Wi = getNextIncomingLightDir(i);
//   sum += reflectance(P, Wi, Wo) * radiance(P, Wi) * dot(N, Wi) * dW;
// }
//
// reflectance is our BRDF function, which has a slew of various models
// that can be relied upon.
//
// The first model that we'll be experimenting with is the cook torrance model,
// which is fairly common.
//
// The reflectance is built of the following sum:
//
// reflectance = diffuse + specular
//
// the diffuse is calculated as follows:
//
// k_refracted_light * (surface_color / pi)
//
// the specular is more complicated:
//
// specular = normal_distribution(halfway_vector, smoothness) * fresnel(dot(N, Wo)) * geometry() / normalization_factor()
//
// ----
// normal_distribution(halfway_vector, smoothness):
// ----
// computes an approximation for the amount that the surface's microfacets are aligned to the halfway vector.
//
// note that the halfway_vector = (Wi + Wo) / length(Wi + Wo)
//
// smoothness is obviously specific to the material.
//
// ----
// fresnel(cosTheta):
// ----
// provides the ratio of surface reflection for the given surface angle
//
// ----
// geometry()
// ----
//
// describes a self-shadowing property of the microfacets


layout(location = 0) in vec2 frag_TexCoord;
layout(location = 1) in vec3 frag_Color;
layout(location = 2) in vec3 frag_Normal;
layout(location = 3) in vec3 frag_WorldPosition;

layout(location = 0) out vec4 out_Color;

layout(set=0, binding=0) uniform sampler2D unif_Samplers[2];

// note that vec3 is 16 byte aligned
layout(push_constant) uniform basic_pbr {
  layout(offset = 0) vec3 cameraPosition;
  layout(offset = 16) vec3 albedo;
  layout(offset = 32) float metallic;
  layout(offset = 36) float roughness;
  layout(offset = 40) float ao;
  layout(offset = 44) int samplerIndex;
} basicPbr;

const float PI = 3.1415926535;

// [2]
// N: surface normal.
// H: halfway vector. Is measured against the surface's microfacets
// a: surface roughness.
float distribution_ggx(vec3 N, vec3 H, float a) {
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float nom    = a2;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom        = PI * denom * denom;
	
    return nom / denom;
}

// [2]
float k_direct(float a) {
  return ((a + 1.0) * (a + 1.0)) * 0.125;
}

// [2] 
float k_ibl(float a) {
  return (a * a) * 0.5;
}

// [2]
vec3 f0() {
  return mix(vec3(0.04), basicPbr.albedo, basicPbr.metallic);
}

// [2]
// cosTheta: dot product between surface normal and viewing direction
// F0: reflectance; computed from f0()
vec3 fresnel_schlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// [2]
// 
float geometry_schlick_ggx(float NdotV, float k)
{
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return nom / denom;
}
  
float geometry_smith(vec3 N, vec3 V, vec3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = geometry_schlick_ggx(NdotV, k);
    float ggx2 = geometry_schlick_ggx(NdotL, k);
	
    return ggx1 * ggx2;
}

// F0: reflectance; computed from f0()
// N: vertex normal
// V: direction to viewer
vec3 sample_radiance(in vec3 F0, in vec3 N, in vec3 V, in vec3 lightPosition, in vec3 lightColor) {
  
  // calculate per-light radiance
  vec3 L = normalize(lightPosition - frag_WorldPosition); // direction to light
  vec3 H = normalize(V + L); // half angle
  float distance = length(lightPosition - frag_WorldPosition);
  float attenuation = 1.0 / (distance * distance);
  vec3 radiance = lightColor * attenuation;

  // Cook-Torrance BRDF
  float NDF = distribution_ggx(N, H, basicPbr.roughness);   
  float G   = geometry_smith(N, V, L, basicPbr.roughness);      
  vec3 F    = fresnel_schlick(clamp(dot(H, V), 0.0, 1.0), F0);
           
  vec3 nominator    = NDF * G * F; 
  float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
  vec3 specular = nominator / max(denominator, 0.001); // prevent divide by zero for NdotV=0.0 or NdotL=0.0
        
  // kS is equal to Fresnel
  vec3 kS = F;
  // for energy conservation, the diffuse and specular light can't
  // be above 1.0 (unless the surface emits light); to preserve this
  // relationship the diffuse component (kD) should equal 1.0 - kS.
  vec3 kD = vec3(1.0) - kS;
  // multiply kD by the inverse metalness such that only non-metals 
  // have diffuse lighting, or a linear blend if partly metal (pure metals
  // have no diffuse light).
  kD *= 1.0 - basicPbr.metallic;	  

  // scale light by NdotL
  float NdotL = max(dot(N, L), 0.0);        

  // add to outgoing radiance Lo
  return (kD * basicPbr.albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again  
}

vec4 sample_texture() {
  return
    texture(unif_Samplers[basicPbr.samplerIndex & 0x1],
	    frag_TexCoord);
}

void main() {
  vec3 testLightPosition = vec3(0.0, 10.0, 0.0);
  vec3 testLightColor = vec3(300.0, 300.0, 300.0);
  
  vec3 N = normalize(frag_Normal);
  vec3 V = normalize(basicPbr.cameraPosition - frag_WorldPosition);
  vec3 F0 = f0();

  vec3 Lo = sample_radiance(F0, N, V, testLightPosition, testLightColor);

  vec3 ambient = vec3(0.03) * basicPbr.albedo * basicPbr.ao;
  vec3 color   = ambient + Lo;

  color = color / (color + vec3(1.0));
  color = pow(color, vec3(1.0/2.2));  
  
  out_Color = vec4(color, 1.0);
}
