#pragma once

#include "common.hpp"
#include "gapi.hpp"
#include <inttypes.h>
#include <unordered_map>

#define GLSL_INL(code) #code
#define GLSL_L(code) #code "\n"
#define GLSL_TL(code) "\t" #code "\n"
#define GLSL_TTL(code) "\t" GLSL_TL(code)
#define GLSL_TTTL(code) "\t" GLSL_TTL(code)
#define GLSL_TTTTL(code) "\t" GLSL_TTTL(code)
#define GLSL_T(code) "\t" #code

#define GLSL_IT(code) "\t" code 
#define GLSL_ITL(code) "\t" code "\n"
#define GLSL_I(code) code
#define GLSL_IL(code) code "\n"

#define NUM_LIGHTS 1

#define GLSL_FILE_HEADER "#version " OPENGL_VERSION_MAJOR_STR "" OPENGL_VERSION_MINOR_STR "0 core\n\n" 
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
  fshader_lights = 1 << 6,
  fshader_unif_model = 1 << 7,
  fshader_lights_shine = 1 << 8,
  fshader_unif_color = 1 << 9,
  fshader_toggle_quad = 1 << 10
};

struct fshader_params {
  const uint32_t light_count {NUM_LIGHTS};
  const bool invert_normals {false};

  const std::string input_normal {"frag_Normal"};
  const std::string input_position {"frag_Position"};
  const std::string input_color {"frag_Color"};
};

typedef uint32_t shadergen_flags_t;

static constexpr shadergen_flags_t fshader_pos_color_normal() {
  return fshader_frag_normal | fshader_frag_position | fshader_frag_color;
}

static constexpr shadergen_flags_t vshader_frag_pos_color_normal() {
  return vshader_frag_normal | vshader_frag_position | vshader_frag_color;
}

#define VSHADER_POINTLIGHTS vshader_frag_pos_color_normal() | vshader_in_normal | vshader_unif_model
#define FSHADER_POINTLIGHTS fshader_pos_color_normal() | fshader_lights | fshader_lights_shine

static inline darray<std::string> uniform_location_pointlight(uint32_t index) {
  return {
    "unif_Lights[" + std::to_string(index) + "].position",
    "unif_Lights[" + std::to_string(index) + "].color"
  };
}

static inline darray<std::string> uniform_location_shine() {
  return {
    "unif_Material.smoothness",
    "unif_CameraPosition"
  };
}

static inline darray<std::string> uniform_location_mv_proj() {
  return {
    "unif_ModelView",
    "unif_Projection"
  };
}

static inline darray<std::string> uniform_location_color() {
  return {
    "unif_Color"
  };
}

static inline darray<std::string> uniform_location_toggle_quad() {
  return {
    "unif_ToggleQuadColor",       // vec4
    "unif_ToggleQuadScreenXY",    // vec2
    "unif_ToggleQuadEnabled"      // int
  };
}

struct dpointlight {
  vec3_t position;
  vec3_t color;
};

struct dmaterial {
  float smoothness;
};

struct module_programs: public type_module {
  using ptr_type = std::unique_ptr<module_programs>;

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

    darray<std::string> uniforms;
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
      ( void*) offsetof(vertex, position)
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
      ( void*) offsetof(vertex, color)
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
        ( void*) offsetof(vertex, normal)
      }
    };
  }

  static std::string from_bool(bool b) {
    return b ? "true" : "false";
  }

  static inline uint32_t vshader_count {0};
  static inline uint32_t fshader_count {0};

  static std::string gen_vshader(shadergen_flags_t flags, const std::string& name="UNSPECIFIED") {
    std::stringstream ss;

    bool in_normal = flags & vshader_in_normal;
    bool in_texcoord = flags & vshader_in_texcoord;
    bool frag_position = flags & vshader_frag_position;
    bool frag_color = flags & vshader_frag_color;
    bool frag_normal = flags & vshader_frag_normal;
    bool frag_texcoord = flags & vshader_frag_texcoord;
    bool unif_model = flags & vshader_unif_model;

    ss << GLSL_FILE_HEADER
      << GLSL_L(layout(location = 0) in vec3 in_Position;)
      << GLSL_L(layout(location = 1) in vec4 in_Color;);

    if (in_normal) ss << GLSL_L(layout(location = 2) in vec3 in_Normal;);

    if (in_texcoord) ss << GLSL_L(layout(location = 3) in vec2 in_TexCoord;);

    if (frag_position) ss << GLSL_L(smooth out vec3 frag_Position;);
    if (frag_color) ss << GLSL_L(smooth out vec4 frag_Color;);
    if (frag_normal) ss << GLSL_L(smooth out vec3 frag_Normal;);

    if (frag_texcoord) {
      ss << (in_texcoord
            ? GLSL_L(smooth out vec2 frag_TexCoord;)
            : GLSL_L(smooth out vec3 frag_TexCoord;));
    }

    if (unif_model) ss << GLSL_L(uniform mat4 unif_Model;);

    ss << GLSL_L(uniform mat4 unif_ModelView;);
    ss << GLSL_L(uniform mat4 unif_Projection;);
    ss << GLSL_L(void main() {
      );

    if (frag_position) {
      ss << GLSL_T(frag_Position =)
        << (unif_model
               ? GLSL_L(vec3(unif_Model * vec4(in_Position, 1.0));)
               : GLSL_L(in_Position;));
    }

    if (frag_normal) {
      ASSERT(in_normal);
      ss << GLSL_T(frag_Normal =)
        << (unif_model
                ? GLSL_L(vec3(unif_Model * vec4(in_Normal, 0.0));)
                : GLSL_L(in_Normal;));
    }

    if (frag_texcoord) {
      ss << GLSL_T(frag_TexCoord =)
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

    ss << GLSL_L(
    });

    auto s = ss.str();

    write_logf("\n----------vshader %" PRIu32 " (%s)----------\n%s\n\n\n",
                vshader_count, name.c_str(), s.c_str());

    vshader_count++;

    return s;
  }

  static std::string gen_fshader(shadergen_flags_t flags,
                                  fshader_params p=fshader_params {},
                                  const std::string& name = "UNSPECIFIED") {
    std::stringstream ss;

    bool frag_position = flags & fshader_frag_position;
    bool frag_color = flags & fshader_frag_color;
    bool frag_normal = flags & fshader_frag_normal;
    bool frag_texcoord = flags & fshader_frag_texcoord;
    bool unif_texcubemap = flags & fshader_unif_texcubemap;
    bool reflect = flags & fshader_reflect;
    bool lights = flags & fshader_lights;
    bool unif_model = flags & fshader_unif_model;
    bool unif_color = flags & fshader_unif_color;
    bool lights_shine = flags & fshader_lights_shine;
    bool toggle_quad = flags & fshader_toggle_quad;

    ASSERT(!(frag_color && unif_color)); // both of these enabled will probably be supported at some point, but we don't want it currently.

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

    ss << GLSL_FILE_HEADER;

    if (frag_position) ss << GLSL_L(smooth in vec3 frag_Position;);
    if (frag_color) ss << GLSL_L(smooth in vec4 frag_Color;);
    if (frag_normal) ss << GLSL_L(smooth in vec3 frag_Normal;);
    if (frag_texcoord) ss << GLSL_L(smooth in vec3 frag_TexCoord;);

    if (lights) {
      ASSERT(p.light_count != 0);
      ss << GLSL_L(struct light {
        )
        << GLSL_TL(vec3 position;)
          << GLSL_TL(vec3 color;)
          << GLSL_L(
      };)
          << GLSL_INL(uniform light unif_Lights[) << p.light_count
          << GLSL_L(];);
    }

    if (toggle_quad) {
      ss  << GLSL_L(uniform vec4 unif_ToggleQuadColor;)
        << GLSL_L(uniform vec2 unif_ToggleQuadScreenXY;)
        << GLSL_L(uniform int unif_ToggleQuadEnabled;);
    }

    if (lights_shine) {
      ASSERT(lights);
      ss << GLSL_L(struct material {
        )
        << GLSL_TL(float smoothness;)
          << GLSL_L(
      };)
          << GLSL_L(uniform material unif_Material;)
          << GLSL_L(uniform vec3 unif_CameraPosition;);
    }

    if (unif_model) {
      ASSERT(lights); // only expected use case currently
      ss << GLSL_L(uniform mat4 unif_Model;);
    }

    if (unif_color) {
      ss << GLSL_L(uniform vec4 unif_Color;);
    }

    if (unif_texcubemap) ss << GLSL_L(uniform samplerCube unif_TexCubeMap;);

    if (reflect) ss << GLSL_L(uniform vec3 unif_CameraPosition;);

    if (lights && !frag_normal) ss << GLSL_L(uniform vec3 frag_Normal;);

    ss << GLSL_L(out vec4 fb_Color;);

    ss << GLSL_L(vec3 debugVec3(in vec3 v) {
      )
      << GLSL_TL(return min(max(normalize(v), vec3(0.0)), vec3(1.0));)
        << GLSL_L(
    });

      if (lights_shine) {
        ASSERT(lights);
        ss  << GLSL_L(float applySpecular(in vec3 vposition,
                                          in vec3 vnormal,
                                          in vec3 dirToViewer,
                                          in vec3 lightPos,
                                          float angleOfIncidence) {
          )
          << GLSL_TL(vec3 dirToLight = normalize(lightPos - vposition);)
            << GLSL_TL(vec3 reflectDir = reflect(-dirToLight, normalize(vnormal));)
            << GLSL_TL(vec3 halfAngle = normalize(dirToLight + dirToViewer);)
            << GLSL_TL(float term = dot(halfAngle, vnormal);)
            << GLSL_TL(term = clamp(term, 0, 1);)
            //<< GLSL_TL(term = angleOfIncidence != 0.0 ? term : 0.0;)
            << GLSL_TL(term = pow(term, unif_Material.smoothness);)
            << GLSL_TL(return term;)
            << GLSL_L(
        });
      }

      if (toggle_quad) {
        ss  << GLSL_L(bool toggleQuad() {
          )
          << GLSL_TL(bool ret = false;)
            << GLSL_TL(if (unif_ToggleQuadEnabled == 1) {
            )
            << GLSL_TTL(vec2 center = unif_ToggleQuadScreenXY;)
              << GLSL_TTL(const float RADIUS = 50;)
              << GLSL_TTL(float xmin = center.x - RADIUS;)
              << GLSL_TTL(float xmax = center.x + RADIUS;)
              << GLSL_TTL(float ymin = center.y - RADIUS;)
              << GLSL_TTL(float ymax = center.y + RADIUS;)
              << GLSL_TTL(ret = (xmin <= gl_FragCoord.x && gl_FragCoord.x <= xmax);)
              << GLSL_TTL(ret = ret && (ymin <= gl_FragCoord.y && gl_FragCoord.y <= ymax);)
              << GLSL_TL(
          })
              << GLSL_TL(return ret;)
              << GLSL_L(
        });
      }

      if (lights) {
        /* TODO: eliminate conditional overhead associated with "invertNormals".
         * there is a function which allows direct bit manipulation of floats,
         * and combinatorial logic and should be used in conjunction with this
         * function to perform the inversion when invertNormals == true, and
         * not invert when invertNormals == false.
         */
        ss  << GLSL_L(vec3 applyPointLights(in vec3 vposition, in vec3 vnormal, int numLights, bool invertNormals) {
          )
          << GLSL_TL(vec3 lightpass = vec3(0.0);)
            << GLSL_TL(const float c1 = 0.0;)
            << GLSL_TL(const float c2 = 0.0;)
            << GLSL_TL(const float c3 = invertNormals ? -1.0 : 1.0;);
          if (lights_shine) {
            ss << GLSL_TL(vec3 dirToViewer = normalize(unif_CameraPosition - vposition););
          }
          ss
            << GLSL_TL(for (int i = 0; i < numLights; ++i) {
            )
            << GLSL_TTL(vec3 lightDir = normalize(unif_Lights[i].position - vposition);)
              << GLSL_TTL(float diff = max(dot(lightDir, c3 * normalize(vnormal)), 0.0);)
              << GLSL_TTL(vec3 diffuse = unif_Lights[i].color * diff * frag_Color.xyz;)
              << GLSL_TTL(vec3 result = diffuse;);
            if (lights_shine) {
              ss << GLSL_TTL(result += applySpecular(vposition,
                                                     vnormal,
                                                     dirToViewer,
                                                     unif_Lights[i].position,
                                                     diff););
            }
            ss
#if 0
              << GLSL_TTL(float distance = length(unif_Lights[i].position - vposition);)
              << GLSL_TTL(result *= (1.0 / (1.0 + (c1 * distance) + (c2 * distance * distance)));)
#endif
              << GLSL_TTL(lightpass += result;)
              << GLSL_TL(
          })
              << GLSL_TL(return lightpass;)
              << GLSL_L(
        });
      }


      ss << GLSL_L(void main() {
        )
        << GLSL_TL(vec4 out_color = vec4(1.0););

        // as implied by the assert above,
        // unif_color and frag_color cannot both be true.
        if (unif_color) {
          ss << GLSL_TL(vec4 frag_Color = unif_Color;);
        }
        else if (!frag_color) {
          ss << GLSL_TL(vec4 frag_Color = vec4(1.0););
        }

        if (reflect) {
          ss << GLSL_TL(vec3 I = frag_Position - unif_CameraPosition;)
            << GLSL_TL(vec3 R = reflect(I, normalize(frag_Normal));)
            << GLSL_TL(out_color = texture(unif_TexCubeMap, R) * frag_Color;);
        }

        if (frag_texcoord) {
          ss << GLSL_TL(out_color = texture(unif_TexCubeMap, frag_TexCoord););
        }

        if (lights) {
          ASSERT(frag_position);
          ASSERT(frag_color);
          //ASSERT(frag_normal);

          ss  << (unif_model
                  ? GLSL_TL(vec3 vposition = vec3(unif_Model * vec4(frag_Position, 1.0));)
                  : GLSL_TL(vec3 vposition = frag_Position;))
            << (unif_model
                ? GLSL_TL(vec3 vnormal = vec3(transpose(inverse(unif_Model)) * vec4(frag_Normal, 0.0));)
                : GLSL_TL(vec3 vnormal = frag_Normal;))
            << GLSL_T(int numLights =) <<  p.light_count << GLSL_L(;)
            << GLSL_T(bool invertNormals =) << from_bool(p.invert_normals) << GLSL_L(;)
            << GLSL_TL(out_color.xyz *= applyPointLights(vposition, vnormal, numLights, invertNormals););
        }

        if (reflect || lights || frag_texcoord) {
          ss << GLSL_TL(vec4 interm1 = out_color * frag_Color;);
        }
        else {
          ss << GLSL_TL(vec4 interm1 = frag_Color;);
        }

        if (toggle_quad) {
          ss  << GLSL_TL(if (toggleQuad()) {
            )
            << GLSL_TTL(interm1 = unif_ToggleQuadColor;)
              << GLSL_TL(
          });
        }

        ss << GLSL_TL(fb_Color = interm1;)
          << GLSL_L(
      });

        auto s = ss.str();

        write_logf("\n---------fshader %" PRIu32 " (%s)-----------\n%s\n\n\n",
                    fshader_count, name.c_str(), s.c_str());

        fshader_count++;

        return s;
  }

  darray<programdef> defs = {
    {
      "basic",
      gen_vshader(vshader_frag_color, "basic"),
      gen_fshader(fshader_frag_color, {}, "basic"),
      uniform_location_mv_proj(),
  {
    attrib_layout_position(),
    attrib_layout_color()
  }
    },
    {
      "single_color",
      gen_vshader(0, "single_color"),
      gen_fshader(fshader_unif_color, {}, "single_color"),
      uniform_location_mv_proj() +
      uniform_location_color() /*+
      uniform_location_toggle_quad()*/,
  {
    attrib_layout_position()
  }
    },
    {
      "main",
      gen_vshader(VSHADER_POINTLIGHTS, "main"),

      gen_fshader(FSHADER_POINTLIGHTS,
                  {NUM_LIGHTS,
                  true}, "main"),
                  ([&]() -> darray<std::string> {
    return darray<std::string> {
      "unif_ModelView",
        "unif_Projection",
        "unif_Model"
    } + uniform_location_pointlight(0)
        + uniform_location_shine();
  })(),
  {
    attrib_layout_position(),
    attrib_layout_color(),
    attrib_layout_normal()
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

       gen_vshader(VSHADER_POINTLIGHTS |
       vshader_frag_texcoord, "cubemap"),

    gen_fshader(FSHADER_POINTLIGHTS |
                fshader_frag_texcoord |
                fshader_unif_texcubemap,
                {
                NUM_LIGHTS,
                true        // invert normals
                }, "cubemap"),
              ([&]() -> darray<std::string>  {
    return darray<std::string>{
      "unif_ModelView",
        "unif_Projection",
        "unif_TexCubeMap",
        "unif_Model"
    }  + uniform_location_pointlight(0)
        + uniform_location_shine();
  })(),
  {
    attrib_layout_position(),
    attrib_layout_color(),
    attrib_layout_normal()
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
      gen_vshader(vshader_in_normal | vshader_frag_pos_color_normal(), "reflection_sphere_cubemap"),
#endif           

#if 0      
      GLSL(smooth in vec4 frag_Color;
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
      gen_fshader(fshader_pos_color_normal() | fshader_unif_texcubemap | fshader_reflect, {},
                  "reflection_sphere_cubemap"),
#endif
    ([&]() -> darray<std::string> {
    return darray<std::string> {
      "unif_ModelView",
      "unif_Projection",
      "unif_TexCubeMap",
      "unif_CameraPosition"
    };
  })(),
  {
    attrib_layout_position(),
    attrib_layout_color(),
    attrib_layout_normal()
  }
    }};

  struct program {
    std::unordered_map<std::string, gapi::program_uniform_handle> uniforms;
    attrib_map_type attribs;

    gapi::program_handle handle;
  };

  std::unordered_map<std::string, std::unique_ptr<program>> data;

  std::string current;

  const std::string basic = "basic";
  const std::string mousepick = "single_color";
  const std::string default_fb = "main";
  const std::string default_rtq = "render_to_quad";
  const std::string default_mir = "reflection_sphere";
  const std::string sphere_cubemap = "reflection_sphere_cubemap";
  const std::string skybox = "cubemap";

  using id_type = std::string;

  ~module_programs() {
    g_m.gpu->use_program(gapi::k_null_program);

    for (auto& entry: data) {
      g_m.gpu->delete_program(entry.second->handle);
    }
  }

  auto get(const std::string& name) const {
    return data.at(name).get();
  }

  void load() {
    for (const auto& def: defs) {
      auto p = std::make_unique<program>();

      p->handle = g_m.gpu->make_program(def.vertex, def.fragment);

      CLOG(logflag_programs_load, "loading program %s\n", def.name.c_str());

      if (p->handle) {
        for (auto unif: def.uniforms) {
          p->uniforms[unif] = g_m.gpu->program_query_uniform(p->handle, unif);

          CLOG(logflag_programs_load, "\tuniform %s -> %i\n", unif.c_str(), p->uniforms[unif].value());

          if (p->uniforms[unif].value() == -1) {
            __FATAL__("Uniform location fetch failure for %s@%s\n", 
                      unif.c_str(), 
                      def.name.c_str());
          }
        }

        p->attribs = def.attribs;
        data[def.name] = std::move(p);
      } else {
        __FATAL__("Could not successfully link program %s\n", def.name.c_str());
      }
    }
  }

  void make_current(const std::string& name) {
    current = name;
  }

  gapi::program_uniform_ref uniform(const std::string& name) const {
    auto it = data.at(current)->uniforms.find(name);

    return (it != data.at(current)->uniforms.end()) 
            ? it->second 
            : gapi::k_null_program_uniform;
  }

  void up_mat4x4(const std::string& name, const glm::mat4& m) const {
    g_m.gpu->program_set_uniform_matrix4(uniform(name), m);
  }

  void up_int(const std::string& name, int i) const {
    g_m.gpu->program_set_uniform_int(uniform(name), i);
  }

  void up_float(const std::string& name, float f) const {
    g_m.gpu->program_set_uniform_float(uniform(name), f);
  }

  void up_vec2(const std::string& name, const vec2_t& v) const {
    g_m.gpu->program_set_uniform_vec2(uniform(name), v);
  }

  void up_vec3(const std::string& name, const vec3_t& v) const {
    g_m.gpu->program_set_uniform_vec3(uniform(name), v);
  }

  void up_vec4(const std::string& name, const vec4_t& v) const {
    g_m.gpu->program_set_uniform_vec4(uniform(name), v);
  }

  void up_pointlight(const std::string& name, const dpointlight& pl) const {
    g_m.gpu->program_set_uniform_vec3(uniform(name + ".position"), pl.position);
    g_m.gpu->program_set_uniform_vec3(uniform(name + ".color"), pl.color);
  }

  void up_material(const std::string& name, const dmaterial& dm) const {
    g_m.gpu->program_set_uniform_float(uniform(name + ".smoothness"), dm.smoothness);
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

// Make sure the VBO is bound BEFORE
// this is initialized
struct use_program {
  CLOG_CODE(std::string clog_name;)
  gapi::program_ref prog;

  use_program(const std::string& name)
    : CLOG_CODE(clog_name(name)),
      prog(g_m.programs->get(name)->handle) {

    g_m.programs->make_current(name);
    g_m.programs->load_layout();
    CLOG(logflag_programs_use_program, "setting current program: %s\n", clog_name.c_str());
    g_m.gpu->use_program(prog);

   // GL_FN(glUseProgram(prog));
  }

  ~use_program() {
    CLOG(logflag_programs_use_program, "releasing current program: %s\n", clog_name.c_str());
    g_m.programs->unload_layout();
    g_m.gpu->use_program(gapi::k_null_program);
   // GL_FN(glUseProgram(0));
  }
};


