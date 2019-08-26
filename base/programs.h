#pragma once

#include "common.h"
#include "util.h"

struct programs : public type_module {
  
  struct attrib_layout {
    GLint index;
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLsizei stride;
    const GLvoid* pointer;
  };

  using attrib_map_type = std::unordered_map<std::string, attrib_layout>;
  using attrib_entry_type = std::pair<std::string, attrib_layout>;
  
  struct programdef {
    std::string name;
    const char* vertex;
    const char* fragment;

    std::vector<std::string> uniforms;
    attrib_map_type attribs;
  };

  static attrib_entry_type  attrib_layout_position() {
    return {
      "in_Position",
      {
        0,
        3,
        OPENGL_REAL,
        GL_FALSE,
        sizeof(vertex),
        (void*) offsetof(vertex, position)
      }
    };
  }

  static attrib_entry_type attrib_layout_color() {
    return {
      "in_Color",
      {
        1,
        4,
        OPENGL_REAL,
        GL_FALSE,
        sizeof(vertex),
        (void*) offsetof(vertex, color)
      }
    };
  }

  static attrib_entry_type attrib_layout_normal() {
    return {
      "in_Normal",
	{
	  2,
	    3,
	    OPENGL_REAL,
	    GL_FALSE,
	    sizeof(vertex),
	    (void*) offsetof(vertex, normal)
	}
    };
  }
  
  const char* vertex_shader_standard =
    GLSL(layout(location = 0) in vec3 in_Position;
         layout(location = 1) in vec4 in_Color;
           
         smooth out vec4 frag_Color;          
           
         uniform mat4 unif_ModelView;
         uniform mat4 unif_Projection;
           
         void main() {
           vec4 clip = unif_Projection * unif_ModelView * vec4(in_Position, 1.0);
           gl_Position = clip;             
           frag_Color = in_Color;
         });

  
  
    
  
  std::vector<programdef> defs = {
    {
      "main",
      vertex_shader_standard,
      GLSL(smooth in vec4 frag_Color;
           out vec4 fb_Color;

           uniform bool unif_GammaCorrect;
           
           void main() {
               if (unif_GammaCorrect) {
                   fb_Color = vec4(pow(frag_Color.xyz, vec3(1.0/ 2.2)), frag_Color.a);
               } else {
                   fb_Color = frag_Color;
               }
           }),
      {
        "unif_ModelView",
        "unif_Projection",
        "unif_GammaCorrect"
      },
      {
        attrib_layout_position(),
        attrib_layout_color()
      },
    },
    {
      "render_to_quad",

      GLSL(smooth out vec2 frag_TexCoord;

           // quad
           // 01    11
           // 00    10
           //
           // xy  |  vertex id
           // 01  |  00
           // 00  |  01
           // 11  |  10
           // 10  |  11
            
           void main() {
             float x = float((gl_VertexID >> 1) & 1);
             float y = float(1 - (gl_VertexID & 1));
             
             frag_TexCoord = vec2(x, y);

             x = 2.0 * x - 1.0;
             y = 2.0 * y - 1.0;
             
             gl_Position = vec4(x, y, 0.0, 1.0);
           }),
      
      GLSL(smooth in vec2 frag_TexCoord;
           out vec4 fb_Color;

           uniform sampler2D unif_TexSampler;
           
           void main() {
             //             fb_Color = vec4(frag_TexCoord.x, 0.0, frag_TexCoord.y, 1.0);
             fb_Color = vec4(texture(unif_TexSampler, frag_TexCoord).rgb, 1.0);
               /** vec4(frag_TexCoord.x, 0.0, frag_TexCoord.y, 1.0);*/
           }),
      {
        "unif_TexSampler"
      }
    },
    {
      "reflection_sphere_cubemap",
      GLSL(layout(location = 0) in vec3 in_Position;
           layout(location = 1) in vec4 in_Color;
	   layout(location = 2) in vec3 in_Normal;
           
           smooth out vec4 frag_Color;
           smooth out vec3 frag_Position;
           out vec3 frag_Normal;
           
           uniform mat4 unif_InverseView;
           uniform mat4 unif_ModelView;
           uniform mat4 unif_Projection;
           
           
           void main() {
             vec4 clip = unif_Projection * unif_ModelView * vec4(in_Position, 1.0);
             gl_Position = clip;             
             frag_Color = in_Color; //abs(clip / clip.w);
	     
             frag_Normal = in_Normal/*in_Position*/;
             

             mat4 noTrans = unif_InverseView;
             noTrans[3] = vec4(0.0, 0.0, 0.0, 1.0);
             
             frag_Position = vec3(noTrans * unif_ModelView * vec4(in_Position, 1.0));
           }),
      
      GLSL(smooth in vec4 frag_Color;
           smooth in vec3 frag_Position;
           in vec3 frag_Normal;

           uniform samplerCube unif_TexCubeMap;
           uniform vec3 unif_CameraPosition;

           out vec4 fb_Color;
           
           void main() {
             vec3 I = normalize(frag_Position - unif_CameraPosition);
             vec3 R = reflect(I, normalize(frag_Normal));
             fb_Color = vec4(texture(unif_TexCubeMap, R).rgb, 1.0) * frag_Color;
           }),

      {
        "unif_InverseView",
        "unif_ModelView",
        "unif_Projection",
        "unif_TexCubeMap",
        "unif_CameraPosition"
      },
      {
        attrib_layout_position(),
        attrib_layout_color(),
	attrib_layout_normal()
      }
    },
    {
      "reflection_sphere",
      GLSL(layout(location = 0) in vec3 in_Position;
           layout(location = 1) in vec4 in_Color;
           
           smooth out vec4 frag_Color;          
           smooth out vec3 frag_ModelPosition;
          
           uniform mat4 unif_ModelView;
           uniform mat4 unif_Projection;
           
           void main() {
             vec4 clip = unif_Projection * unif_ModelView * vec4(in_Position, 1.0);
             gl_Position = clip;             
             frag_Color = in_Color; //abs(clip / clip.w);

             frag_ModelPosition = in_Position;
           }),
      
      GLSL(smooth in vec3 frag_ModelPosition; /* interpolated somewhere between tri vertices */
           smooth in vec4 frag_Color;

           const float PI = 3.1415926535897932384626433832795;
           const float PI_2 = 1.57079632679489661923;
           const float PI_4 = 0.785398163397448309616;
           
           uniform sampler2D unif_TexFramebuffer;
           uniform vec3 unif_LookCenter;

           out vec4 fb_Color;

           vec2 sphereFromCart(vec3 positionNormalized) {
             float phi = asin(positionNormalized.y);
             float theta = acos(positionNormalized.x / cos(phi));
             return vec2(theta, phi);
           }
           
           void main() {             
             vec3 lookCenterNorm = normalize(unif_LookCenter);
             vec3 modelPosNorm = normalize(frag_ModelPosition);

             if (dot(lookCenterNorm, modelPosNorm) < 0) {
               fb_Color = frag_Color;
            
             } else {
               vec2 tpLook = sphereFromCart(lookCenterNorm);
               vec2 tpModel = sphereFromCart(modelPosNorm);

               float u = (tpModel.x - tpLook.x) / PI_2;
               u *= 0.5;

               float v = (tpModel.y - tpLook.y) / PI_2;
               v *= 0.5;
               
               vec2 base = vec2(0.5);
               
               vec4 mirror = vec4(texture(unif_TexFramebuffer, base + vec2(u, v)).rgb, 1.0);
               
               fb_Color = mirror * frag_Color;
             }
           }),
      {
        "unif_TexFramebuffer",
        "unif_LookCenter",
        
        "unif_ModelView",
        "unif_Projection"
      },
      {
        attrib_layout_position(),
        attrib_layout_color()
      }
    },
    {
      "cubemap",

      GLSL(layout(location = 0) in vec3 in_Position;
           layout(location = 1) in vec4 in_Color;           
           
           uniform mat4 unif_ModelView;
           uniform mat4 unif_Projection;
         
           out vec4 frag_Color;
           out vec3 frag_TexCoord;
           
           void main() {
             vec4 clip = unif_Projection * unif_ModelView * vec4(in_Position, 1.0);
             gl_Position = clip;

             frag_TexCoord = normalize(in_Position);
             frag_Color = in_Color;
           }),

      GLSL(in vec3 frag_TexCoord;
           in vec4 frag_Color;
           out vec4 fb_Color;

           uniform samplerCube unif_TexCubeMap;

           void main() {
             fb_Color = frag_Color * texture(unif_TexCubeMap, frag_TexCoord);
           }),
      {
        "unif_ModelView",
        "unif_Projection",
        "unif_TexCubeMap"
      },
      
      {
        attrib_layout_position(),
        attrib_layout_color()
      }
    }
  };
  
  struct program {
    std::unordered_map<std::string, GLint> uniforms;
    attrib_map_type attribs;
    
    GLuint handle;
  };
  
  std::unordered_map<std::string, std::unique_ptr<program>> data;

  std::string current;

  const std::string default_fb = "main";
  const std::string default_rtq = "render_to_quad";
  const std::string default_mir = "reflection_sphere";
  const std::string sphere_cubemap = "reflection_sphere_cubemap";
  const std::string skybox = "cubemap";
  
  void free_mem() override {
    GL_FN(glUseProgram(0));
    
    for (auto& entry: data) {
      GL_FN(glDeleteProgram(entry.second->handle));
    }
  }
  
  auto get(const std::string& name) const {
    return data.at(name).get();
  }

  void load() {
    for (const auto& def: defs) {
      auto p = std::make_unique<program>();

      p->handle = make_program(def.vertex, def.fragment);
      
      for (auto unif: def.uniforms) {
        GL_FN(p->uniforms[unif] = glGetUniformLocation(p->handle, unif.c_str()));
      }
      
      p->attribs = def.attribs; 
      data[def.name] = std::move(p);
    }
  }

  void make_current(const std::string& name) {
    current = name;
  }

  auto uniform(const std::string& name) const {
    auto id = data.at(current)->uniforms.at(name);
    ASSERT(id != -1);
    return id;
  }
  
  void up_mat4x4(const std::string& name, const glm::mat4& m) const {    
    GL_FN(glUniformMatrix4fv(uniform(name), 1, GL_FALSE, &m[0][0]));
  }

  void up_int(const std::string& name, int i) const {
    GL_FN(glUniform1i(uniform(name), i));
  }

  void up_vec3(const std::string& name, const vec3_t& v) const {
    GL_FN(glUniform3fv(uniform(name), 1, &v[0]));
  }
  
  auto fetch_attrib(const std::string& program, const std::string& attrib) const {
    return data.at(program)->attribs.at(attrib).index;
  }

  void load_layout() const {
    const auto& p = data.at(current);
    
    for (const auto& attrib: p->attribs) {
      const auto& layout = attrib.second;
      
      GL_FN(glEnableVertexAttribArray(layout.index));
      GL_FN(glVertexAttribPointer(layout.index,
                                  layout.size,
                                  layout.type,
                                  layout.normalized,
                                  layout.stride,
                                  layout.pointer));                                 
    } 
  }

  void unload_layout() const {
    const auto& p = data.at(current);
    
    for (const auto& attrib: p->attribs) {
      GL_FN(glDisableVertexAttribArray(attrib.second.index));
    }
  }
  
} static g_programs;

// Make sure the VBO is bound BEFORE
// this is initialized
struct use_program {
  GLuint prog;

  use_program(const std::string& name)
    : prog(g_programs.get(name)->handle){

    g_programs.make_current(name);
    g_programs.load_layout();
    
    GL_FN(glUseProgram(prog));
  }

  ~use_program() {

    g_programs.unload_layout();
    GL_FN(glUseProgram(0));
  }
};
