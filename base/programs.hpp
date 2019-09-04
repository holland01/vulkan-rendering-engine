#pragma once

#include "common.hpp"
#include "util.hpp"

#include <unordered_map>
#define GLSL_INL(code) #code
#define GLSL_L(code) #code "\n"
#define GLSL_TL(code) "\t" #code "\n"
#define GLSL_T(code) "\t" #code

#define GLSL_IT(code) "\t" code 
#define GLSL_ITL(code) "\t" code "\n"
#define GLSL_I(code) code
#define GLSL_IL(code) code "\n"

enum {
  vshader_in_normal = 1 << 0,
  vshader_in_texcoord = 1 << 1,
  vshader_frag_position = 1 << 2,
  vshader_frag_color = 1 << 3,
  vshader_frag_normal = 1 << 4,
  vshader_frag_texcoord = 1 << 5,
  vshader_unif_model = 1 << 6
};

enum {
  fshader_frag_position = 1 << 0,
  fshader_frag_color = 1 << 1,
  fshader_frag_normal = 1 << 2,
  fshader_frag_texcoord = 1 << 3,
  fshader_unif_texcubemap = 1 << 4,
  fshader_reflect = 1 << 5,
  fshader_lights = 1 << 6
};

struct fshader_params {
  uint32_t light_count{1};
  const std::string input_normal{"frag_Normal"};
  const std::string input_position{"frag_Position"};
  const std::string input_color{"frag_Color"};
};

#define GLSL_LIGHTING_DECL(num_lights) \
  GLSL_INL(			       \
     struct light {	       \
       vec3 position;	       \
       vec3 color;	       \
     }			       \
     uniform light lights[num_lights])

#define GLSL_LIGHTING_PASS(num_lights, output, input_position, input_normal, input_color) \
  GLSL_INL(								                      \
     vec3 output;							\
     for (int i = 0; i < num_lights; ++i) {			\
       vec3 lightDir = normalize(lights[i].position - input_position); \
       float diff = max(dot(lightDir, input_normal), 0.0);	\
       vec3 diffuse = lights[i].color * diff * input_color;	\
       vec3 result = diffuse;					\
       float distance = length(input_position - lights[i].position); \
       result *= 1.0 / (distance * distance);			\
       output += result;					\
     })


struct dpointlight {
  vec3_t position;
  vec3_t color;
};

struct programs : public type_module {

  using ptr_type = std::unique_ptr<programs>;
  
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
    std::string vertex;
    std::string fragment;

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

  static std::string gen_vshader(uint32_t flags) {
    std::stringstream ss;

    bool in_normal = flags & vshader_in_normal;
    bool in_texcoord = flags & vshader_in_texcoord;
    bool frag_position = flags & vshader_frag_position; 
    bool frag_color = flags & vshader_frag_color;
    bool frag_normal = flags & vshader_frag_normal;
    bool frag_texcoord = flags & vshader_frag_texcoord;
    bool unif_model = flags & vshader_unif_model;

    ss << GLSL_L(#version 450 core)
       << GLSL_L(layout(location = 0) in vec3 in_Position;)
       << GLSL_L(layout(location = 1) in vec4 in_Color;);

    if (in_normal) ss << GLSL_L(layout(location = 2) in vec3 in_Normal;);
    
    if (in_texcoord) ss << GLSL_L(layout(location = 3) in vec2 in_TexCoord;);

    if (frag_position) ss << GLSL_L(smooth out vec3 frag_Position;);
    if (frag_color) ss << GLSL_L(out vec4 frag_Color;);
    if (frag_normal) ss << GLSL_L(smooth out vec3 frag_Normal;);
    
    if (frag_texcoord) { 
      ss << (in_texcoord 
            ? GLSL_L(out vec2 frag_TexCoord;) 
            : GLSL_L(out vec3 frag_TexCoord;));
    }

    if (unif_model) ss << GLSL_L(uniform mat4 unif_Model;);
    
    ss << GLSL_L(uniform mat4 unif_ModelView;); 
    ss << GLSL_L(uniform mat4 unif_Projection;);
    ss << GLSL_L(void main() {);
  
    if (frag_position) {
      ss << GLSL_T(frag_Position = )
         << (unif_model 
                ? GLSL_L(vec3(unif_Model * vec4(in_Position, 1.0));) 
                : GLSL_L(in_Position;)); 
    }
  
    if (frag_normal) {
      ss << GLSL_T(frag_Normal = in_Normal;);
    }

    if (frag_texcoord) {
      ss << GLSL_T(frag_TexCoord = )
         << (in_texcoord 
              ? GLSL_L(normalize(in_TexCoord);) 
              : GLSL_L(normalize(in_Position);));
    }

    if (frag_color) {
      ss << GLSL_TL(frag_Color = in_Color;); 
    }

    #undef ASSIGN_FRAG

    ss  << GLSL_TL(vec4 clip = unif_Projection * unif_ModelView * vec4(in_Position, 1.0);)
        << GLSL_TL(gl_Position = clip;);

    ss << GLSL_L(});

    return ss.str();
  }

  static std::string gen_fshader(uint32_t flags, fshader_params p=fshader_params{}) {
    std::stringstream ss;

    bool frag_position = flags & fshader_frag_position;
    bool frag_color = flags & fshader_frag_color;
    bool frag_normal = flags & fshader_frag_normal;
    bool frag_texcoord = flags & fshader_frag_texcoord;
    bool unif_texcubemap = flags & fshader_unif_texcubemap;
    bool reflect = flags & fshader_reflect;

    if (reflect) {
      ASSERT(!frag_texcoord);
      ASSERT(frag_position);
      ASSERT(frag_normal);
      ASSERT(unif_texcubemap);
    }

    if (frag_texcoord) {
      ASSERT(!reflect);
      ASSERT(unif_texcubemap); // no 2d sampling currently, so this is all that's available
    }

    ss << GLSL_L(#version 450 core);

    if (frag_position) ss << GLSL_L(smooth in vec3 frag_Position;);
    if (frag_color) ss << GLSL_L(smooth in vec4 frag_Color;);
    if (frag_normal) ss << GLSL_L(smooth in vec3 frag_Normal;);
    if (frag_texcoord) ss << GLSL_L(in vec3 frag_TexCoord;);

    if (unif_texcubemap) ss << GLSL_L(uniform samplerCube unif_TexCubeMap;);

    if (reflect) ss << GLSL_L(uniform vec3 unif_CameraPosition;);

    ss << GLSL_L(out vec4 fb_Color;)
       << GLSL_L(void main() {)
       << GLSL_TL(vec4 out_color;);

    if (!frag_color) {
      ss << GLSL_TL(vec4 frag_Color = vec4(1.0););
    }

    if (reflect) {
       ss << GLSL_TL(vec3 I = frag_Position - unif_CameraPosition;)
          << GLSL_TL(vec3 R = reflect(I, normalize(frag_Normal));)
          << GLSL_TL(out_color = texture(unif_TexCubeMap, R) * frag_Color;);
    }
    
    if (frag_texcoord) {
      ss << GLSL_TL(out_color = frag_Color * texture(unif_TexCubeMap, frag_TexCoord););
    }

    if (!(reflect || frag_texcoord)) {
      ss << GLSL_TL(out_color = frag_Color;);
    }

    ss << GLSL_TL(fb_Color = out_color;)
       << GLSL_L(});

    return ss.str();
  }

  std::vector<programdef> defs = {
    {
      "main",
      gen_vshader(vshader_frag_color),
      gen_fshader(fshader_frag_color),
      {
        "unif_ModelView",
        "unif_Projection"
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
             fb_Color = vec4(texture(unif_TexSampler, frag_TexCoord).rgb, 1.0);
           }),
      {
        "unif_TexSampler"
      }
    },
     {
      "cubemap",

      gen_vshader(vshader_frag_color | 
                  vshader_frag_texcoord),

      gen_fshader(fshader_frag_color | 
                  fshader_frag_texcoord | 
                  fshader_unif_texcubemap),
      {
        "unif_ModelView",
        "unif_Projection",
        "unif_TexCubeMap"
      },
      
      {
        attrib_layout_position(),
        attrib_layout_color()
      }
    },
    {
      "reflection_sphere_cubemap",
#if 0
      GLSL(layout(location = 0) in vec3 in_Position;
           layout(location = 1) in vec4 in_Color;
            layout(location = 2) in vec3 in_Normal;
           
           smooth out vec4 frag_Color;
           smooth out vec3 frag_Position;
           out vec3 frag_Normal;
           
           uniform mat4 unif_Model;
           uniform mat4 unif_ModelView;
           uniform mat4 unif_Projection;
           
           void main() {
             vec4 clip = unif_Projection * unif_ModelView * vec4(in_Position, 1.0);
             gl_Position = clip;             
             frag_Color = in_Color; //abs(clip / clip.w);
       
             frag_Normal = in_Normal/*in_Position*/;
             
             frag_Position = vec3(unif_Model * vec4(in_Position, 1.0));
           }),
#else
      gen_vshader(vshader_in_normal | 
                  vshader_frag_position | 
                  vshader_frag_color |
                  vshader_frag_normal |
                  vshader_unif_model),
#endif           

#if 0      
      GLSL( smooth in vec4 frag_Color;
            smooth in vec3 frag_Position;
            in vec3 frag_Normal;

            uniform samplerCube unif_TexCubeMap;
            uniform vec3 unif_CameraPosition;

            out vec4 fb_Color;
           
            void main() {
              vec3 I = frag_Position - unif_CameraPosition;
              vec3 R = reflect(I, normalize(frag_Normal));
              vec4 x = texture(unif_TexCubeMap, R) * frag_Color;
              fb_Color = x;
            }),
#else
      gen_fshader(fshader_frag_position |
                  fshader_frag_color | 
                  fshader_frag_normal |
                  fshader_unif_texcubemap |
                  fshader_reflect),
#endif
      {
        "unif_Model",
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
    }};
  
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

  using id_type = std::string;
  
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

      p->handle = make_program(def.vertex.c_str(), def.fragment.c_str());
      
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
    GLint id = -1;
    auto it = data.at(current)->uniforms.find(name);

    if (it != data.at(current)->uniforms.end()) {
      id = it->second;
      //    ASSERT(id != -1);

      if (id == -1) {
  write_logf("%s -> unif %s. Not found\n",
       current.c_str(),
       name.c_str());
      }
    }
    
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

  void up_plight(const std::string& name,
          const vec3_t& position,
          const vec3_t& color) {
    GL_FN(glUniform3fv(uniform(name + ".position"), 1, &position[0]));
    GL_FN(glUniform3fv(uniform(name + ".color"), 1, &color[0]));    
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
  
};

extern programs::ptr_type g_programs;

// Make sure the VBO is bound BEFORE
// this is initialized
struct use_program {
  GLuint prog;

  use_program(const std::string& name)
    : prog(g_programs->get(name)->handle){

    g_programs->make_current(name);
    g_programs->load_layout();
    
    GL_FN(glUseProgram(prog));
  }

  ~use_program() {

    g_programs->unload_layout();
    GL_FN(glUseProgram(0));
  }
};


