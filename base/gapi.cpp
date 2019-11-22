#include "gapi.hpp"
#include "render_pipeline.hpp"



#define APISEL(opengl, vulkan) do { opengl } while (0)

#define APISTUB (void);



namespace gapi {
  static constexpr handle_int_t k_none_value{std::numeric_limits<handle_int_t>::max()};

  const program_handle k_program_none{k_none_value};
  const program_uniform_handle k_program_uniform_none{k_none_value};
  const texture_object_handle k_texture_none{k_none_value};

  static GLenum gl_primitive_type_to_enum(primitive_type ptype) {
    GLenum ret = GL_NONE;
    switch (ptype) {
      case primitive_type::unsigned_byte:
        ret = GL_UNSIGNED_BYTE;
        break;
      case primitive_type::floating_point:
        ret = GL_FLOAT;
        break;
      default:
        __FATAL__("Unknown enum value passed");
        break;
    }
    return ret;
  }

  static GLenum gl_fmt_to_enum(texture_fmt fmt) {
    GLenum ret = GL_NONE;
    switch (fmt) {
      case texture_fmt::rgba: 
        ret = GL_RGBA;
        break;
      case texture_fmt::srgb_a:
        ret = GL_SRGB_ALPHA;
        break;
      case texture_fmt::depth_component:
        ret = GL_DEPTH_COMPONENT;
        break;
      default:
        __FATAL__("Unknown enum value passed");
        break;
    }
    return ret;
  }

  static GLint gl_int_fmt_to_int(texture_int_fmt fmt) {
    GLint ret = GL_NONE;
    switch (fmt) {
      case texture_int_fmt::rgba8:
        ret = GL_RGBA8;
        break;
      case texture_int_fmt::srgb8_alpha8:
        ret = GL_SRGB8_ALPHA8;
        break;
      case texture_int_fmt::depth_component16:
        ret = GL_DEPTH_COMPONENT16;
        break;
      case texture_int_fmt::depth_component24:
        ret = GL_DEPTH_COMPONENT24;
        break;
      default:
        __FATAL__("Unknown enum value passed");
        break;
    }
    return ret;
  }

  template <class enumType>
  static GLint gl_filter_to_int(enumType filter) {
    GLint ret = GL_NONE;
    switch (filter) {
      case enumType::linear:
        ret = GL_LINEAR;
        break;
      case enumType::nearest:
        ret = GL_NEAREST;
        break;
      default:
        __FATAL__("Unknown enum value passed");
        break;
    }
    return ret;
  }

  static GLint gl_wrap_mode_to_int(texture_wrap_mode mode) {
    GLint ret = GL_NONE;
    switch (mode) {
      case texture_wrap_mode::clamp_to_edge:
        ret = GL_CLAMP_TO_EDGE;
        break;
      case texture_wrap_mode::repeat:
        ret = GL_REPEAT;
        break;
      default:
        __FATAL__("Unknown enum value passed");
        break;
    }
    return ret;
  }

  static GLenum gl_target_to_enum(texture_target target) {
    GLenum ret = GL_NONE;
    switch (target) {
      case texture_target::texture_2d:
        ret = GL_TEXTURE_2D;
        break;
      case texture_target::texture_cube_map:
        ret = GL_TEXTURE_CUBE_MAP;
        break;
      default:
        __FATAL__("Unknown target received");
        break;
    }
    return ret;
  }

  void device::apply_state(const gl_state& s) {
    if (s.draw_buffers.fbo) {
      GLenum b[] = {
        GL_COLOR_ATTACHMENT0
      };
      GL_FN(glDrawBuffers(1, b));
    }
    else {
      GLenum b[] = {
        GL_BACK_LEFT
      };
      GL_FN(glDrawBuffers(1, b));
    }

    if (s.depth.test_enabled) {
      GL_FN(glEnable(GL_DEPTH_TEST));
      GL_FN(glDepthFunc(s.depth.func));
    }
    else {
      GL_FN(glDisable(GL_DEPTH_TEST));
    }

    if (s.depth.mask) {
      GL_FN(glDepthMask(GL_TRUE));
    } 
    else {
      GL_FN(glDepthMask(GL_FALSE));
    }

    GL_FN(glDepthRange(s.depth.range_near, s.depth.range_far));

    if (s.face_cull.enabled) {
      GL_FN(glEnable(GL_CULL_FACE));
      GL_FN(glCullFace(s.face_cull.face));
      GL_FN(glFrontFace(s.face_cull.wnd_order));
    }
    else {
      GL_FN(glDisable(GL_CULL_FACE));
    }

    if (s.clear_buffers.depth) {
      GL_FN(glClearDepth(s.clear_buffers.depth_value));
    }

    if (s.clear_buffers.color) {
      GL_FN(glClearColor(s.clear_buffers.color_value.r,
                         s.clear_buffers.color_value.g,
                         s.clear_buffers.color_value.b,
                         s.clear_buffers.color_value.a));
    }

    if (s.gamma.framebuffer_srgb) {
      GL_FN(glEnable(GL_FRAMEBUFFER_SRGB));
    }
    else {
      GL_FN(glDisable(GL_FRAMEBUFFER_SRGB));
    }

    {
      GLenum bits = 0;
      bits = s.clear_buffers.color ? (bits | GL_COLOR_BUFFER_BIT) : bits;
      bits = s.clear_buffers.depth ? (bits | GL_DEPTH_BUFFER_BIT) : bits;
      if (bits != 0) {
        GL_FN(glClear(bits));
      }
    }
  }

  //-------------------------------
  // program_unit_handle
  //-------------------------------

  program_unit_handle device::create_shader(shader_type type) {
    program_unit_handle h{};

    APISEL(
      GLenum gltype = 0;
      
      switch (type) {
        case shader_type::vertex:
          gltype = GL_VERTEX_SHADER;
          break;
        case shader_type::fragment:
          gltype = GL_FRAGMENT_SHADER;
          break;
        default:
          gltype = 0;
          break;
      }

      GLuint shader = 0;
      GL_FN(shader = glCreateShader(gltype));

      if (shader != 0) {
        h.set_value(shader);
      } else {
        h.set_null();
      }
      ,
      APISTUB
    );

    return h;
  }

  void device::delete_shader(program_unit_mut_ref shader) {
    
    if (shader) {
      APISEL(
        GL_FN(glDeleteShader(shader.value_as<GLuint>()));
        shader.set_null();
        ,
        APISTUB
      );
    }
  }

  void device::attach_shader( program_ref program, 
                              program_unit_ref shader) {
    if (program) {
      if (shader) {
        APISEL(
          GL_FN(glAttachShader(program.value_as<GLuint>(), 
                                shader.value_as<GLuint>()));
          ,
          APISTUB
        );
      }
    }
  }

  void device::detach_shader( program_ref program,
                              program_unit_ref shader) {
    if (program) {
      if (shader) {
        APISEL(
          GL_FN(glDetachShader(program.value_as<GLuint>(), 
                              shader.value_as<GLuint>()));
          ,
          APISTUB
        );
      }
    }
  }

  void device::compile_shader(program_unit_ref shader) {
    if (shader) {
      APISEL(
        GL_FN(glCompileShader(shader.value_as<GLuint>()));
        ,
        APISTUB
      );
    }
  }

  bool device::compile_shader_success(program_unit_mut_ref shader) {
    bool ret = false;

    if (shader) {
      ret = true;

      APISEL(
        GLint compile_success;

        glGetShaderiv(shader.value_as<GLuint>(), 
                      GL_COMPILE_STATUS, 
                      &compile_success);

        if (compile_success == GL_FALSE) {
          ret = false;
          GLint info_log_len;
          GL_FN(glGetShaderiv(shader.value_as<GLuint>(), 
                              GL_INFO_LOG_LENGTH,
                              &info_log_len));

          std::vector<char> log_msg(info_log_len + 1, 0);
          GL_FN(glGetShaderInfoLog(shader.value_as<GLuint>(), 
                                  ( GLsizei) (log_msg.size() - 1),
                                  NULL, &log_msg[0]));

          write_logf("COMPILE ERROR: %s\n\nSOURCE\n\n---------------\n%s\n--------------",
                    &log_msg[0], 
                    shader.source.c_str());
        }
        ,
        APISTUB
      );
    }

    return ret;
  }

  void device::set_shader_source( program_unit_mut_ref shader, 
                                  const std::string& source) {
    if (shader) {
      APISEL(
        const char* s = source.c_str();
        GLint length = static_cast<GLint>(source.length());
        GL_FN(glShaderSource(shader.value_as<GLuint>(), 
                              1, 
                              &s, 
                              &length));

        shader.source = source;
        ,
        APISTUB
      );
    }
  }

  //-------------------------------
  // program_handle
  //-------------------------------

  program_handle device::create_program() {
    program_handle h{};

    APISEL(
      GLuint program = 0;
      GL_FN(program = glCreateProgram());

      if (program != 0) {
        h.set_value(program);
      } else {
        h.set_null();
      }
      ,
      APISTUB
    );

    return h;
  }

  void device::delete_program(program_mut_ref program) {
    if (program) {
      APISEL(
        GL_FN(glDeleteProgram(program.value_as<GLuint>()));
        program.set_null();
        ,
        APISTUB
      );
    }
  }

  void device::link_program(program_ref program) {
    if (program) {
      APISEL(
        GL_FN(glLinkProgram(program.value_as<GLuint>()));
        ,
        APISTUB
      );
    }
  }

  bool device::link_program_success(program_mut_ref program) {

    bool ret = false;

    if (program) {
      ret = true;

      APISEL(
        GLint link_success = GL_FALSE;

        glGetProgramiv(program.value_as<GLuint>(),
                      GL_LINK_STATUS, 
                      &link_success);

        if (link_success == GL_FALSE) {
          GLint info_log_len;
          GL_FN(glGetProgramiv(program.value_as<GLuint>(), 
                              GL_INFO_LOG_LENGTH, 
                              &info_log_len));

          std::vector<char> log_msg(info_log_len + 1, 0);

          GL_FN(glGetProgramInfoLog(program.value_as<GLuint>(), 
                                    ( GLsizei) (log_msg.size() - 1),
                                    NULL, &log_msg[0]));

          write_logf("LINKER ERROR: \n---------------\n%s\n--------------\n",
                    &log_msg[0]);

          ret = false;
        }
        ,
        APISTUB
      );
    }

    return ret;
  }

  void device::use_program(program_ref program) {
    if (program) {
      APISEL(
        GL_FN(glUseProgram(program != k_program_none 
                            ? program.value_as<GLuint>()
                            : 0));
        ,
        APISTUB
      );
    }
  }

  program_handle device::make_program(const std::string& vertex, 
                                      const std::string& fragment) {
    program_handle program_ret = create_program();

    program_unit_handle vshader = 
      create_shader(shader_type::vertex);
    
    set_shader_source(vshader, vertex);
    compile_shader(vshader);

    if (compile_shader_success(vshader)) {
      program_unit_handle fshader = 
        create_shader(shader_type::fragment);
      
      set_shader_source(fshader, fragment);
      compile_shader(fshader);

      if (compile_shader_success(fshader)) {
        attach_shader(program_ret, vshader);
        attach_shader(program_ret, fshader);

        link_program(program_ret);

        detach_shader(program_ret, vshader);
        detach_shader(program_ret, fshader);

        if (!link_program_success(program_ret)) {
          delete_program(program_ret);
        }

        delete_shader(vshader);
        delete_shader(fshader);
      }
      else {
        delete_shader(vshader);
        delete_shader(fshader);
        delete_program(program_ret);
      }
    } 
    else {
      delete_shader(vshader);
      delete_program(program_ret);
    }

    return program_ret;
  }

  program_uniform_handle device::program_query_uniform(program_ref program, const std::string& name) {
    program_uniform_handle location{};
    
    if (program) {
      APISEL(
        GLint location_v{-1};
        GL_FN(location_v = glGetUniformLocation(program.value_as<GLuint>(), name.c_str()));

        if (location_v != -1) { 
          location.set_value(location_v);
        } else {
          location.set_null();
        }
        ,
        APISTUB 
      );
    }

    return location;
  }

  void device::program_set_uniform_int(program_uniform_ref uniform, int value) {
    if (uniform) {
      APISEL(
        GL_FN(glUniform1i(uniform.value_as<GLint>(), value));
        ,
        APISTUB 
      );
    }
  }

  void device::program_set_uniform_float(program_uniform_ref uniform, float value) {
    if (uniform) {
      APISEL(
        GL_FN(glUniform1f(uniform.value_as<GLint>(), value));
        ,
        APISTUB 
      );
    }
  }

  void device::program_set_uniform_vec2(program_uniform_ref uniform, const vec2_t& v) {
    if (uniform) {
      APISEL(
        GL_FN(glUniform2fv(uniform.value_as<GLint>(), 1, &v[0]));
        ,
        APISTUB 
      );
    }
  }

  void device::program_set_uniform_vec3(program_uniform_ref uniform, const vec3_t& v) {
    if (uniform) {
      APISEL(
        GL_FN(glUniform3fv(uniform.value_as<GLint>(), 1, &v[0]));
        ,
        APISTUB 
      );
    }
  }

  void device::program_set_uniform_vec4(program_uniform_ref uniform, const vec4_t& v) {
    if (uniform) {
      APISEL(
        GL_FN(glUniform4fv(uniform.value_as<GLint>(), 1, &v[0]));
        ,
        APISTUB 
      );
    }
  }

  void device::program_set_uniform_matrix4(program_uniform_ref uniform, const mat4_t& m) {
    if (uniform) {
      APISEL(
        GL_FN(glUniformMatrix4fv(uniform.value_as<GLint>(), 1, GL_FALSE, &m[0][0]));
        ,
        APISTUB
      );
    }
  }

  //-------------------------------
  // textures
  //-------------------------------

  texture_object_handle device::texture_new() {
    texture_object_handle ret{};

    APISEL(
      GLuint handle = 0;
      GL_FN(glGenTextures(1, &handle));
      
      if (handle != 0) {
        ret.set_value(handle);
      } 
      else {
        ret.set_null();
      }
      ,
      APISTUB
    );

    return ret;
  }

  void device::texture_set_param(texture_target target, texture_param_ref param) {
    GLenum gltarget = gl_target_to_enum(target);

    switch (param.param_type()) {
      case texture_param::type::mag_filter:
        GL_FN(glTexParameteri(gltarget,
                              GL_TEXTURE_MAG_FILTER, 
                              gl_filter_to_int<texture_mag_filter>(param.value<texture_mag_filter>())));
        break;
      
      case texture_param::type::min_filter:
        GL_FN(glTexParameteri(gltarget,
                              GL_TEXTURE_MIN_FILTER,
                              gl_filter_to_int<texture_min_filter>(param.value<texture_min_filter>())));
        break;

      case texture_param::type::wrap_mode_s:
        GL_FN(glTexParameteri(gltarget,
                              GL_TEXTURE_WRAP_S,
                              gl_wrap_mode_to_int(param.value<texture_wrap_mode>())));
        break;

      case texture_param::type::wrap_mode_t:
        GL_FN(glTexParameteri(gltarget,
                              GL_TEXTURE_WRAP_T,
                              gl_wrap_mode_to_int(param.value<texture_wrap_mode>())));
        break;

      case texture_param::type::wrap_mode_r:
        GL_FN(glTexParameteri(gltarget,
                              GL_TEXTURE_WRAP_R,
                              gl_wrap_mode_to_int(param.value<texture_wrap_mode>())));
        break;

      case texture_param::type::mipmap_base_level:
        GL_FN(glTexParameteri(gltarget,
                              GL_TEXTURE_BASE_LEVEL,
                              static_cast<GLint>(param.value<uint8_t>())));
        break;

      case texture_param::type::mipmap_max_level:
        GL_FN(glTexParameteri(gltarget,
                              GL_TEXTURE_MAX_LEVEL,
                              static_cast<GLint>(param.value<uint8_t>())));
        break;
      
      default:
        __FATAL__("Unsupported texture parameter type received");
        break;
    }
  }

  void device::texture_set_active_unit(int_t unit) {
    APISEL(
      GL_FN(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit)));
      ,
      APISTUB
    );
  }

  void device::texture_bind(texture_target target, texture_object_ref texture) {
    if (texture) {
      APISEL(
        GL_FN(glBindTexture(gl_target_to_enum(target), 
                            (texture != k_texture_none)
                             ? texture.value_as<GLuint>()
                             : 0));
        ,
        APISTUB
      );
    }
  }

  void device::texture_delete(texture_object_mut_ref texture) {
    if (texture) {
      APISEL(
        GLuint handle = texture.value_as<GLuint>();
        GL_FN(glDeleteTextures(1, &handle));
        texture.set_null();
        ,
        APISTUB
      );
    }
  }

  void device::texture_image_2d(texture_target target, 
                                miplevel_t mip, 
                                texture_int_fmt internal, 
                                dimension_t width, 
                                dimension_t height, 
                                texture_fmt format, 
                                primitive_type type, 
                                const void *pixels) {
    APISEL(
      GL_FN(glTexImage2D( gl_target_to_enum(target),
                          static_cast<GLint>(mip),
                          gl_int_fmt_to_int(internal),
                          static_cast<GLsizei>(width),
                          static_cast<GLsizei>(height),
                          0, // border - always 0
                          gl_fmt_to_enum(format),
                          gl_primitive_type_to_enum(type),
                          pixels));
      ,
      APISTUB
    );
  }  
}