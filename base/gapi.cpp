#include "gapi.hpp"
#include "render_pipeline.hpp"
#include "backend/opengl.hpp"


#define APISEL(opengl, vulkan) do { opengl } while (0)

#define APISTUB (void);

namespace gapi {
  static constexpr handle_int_t k_none_value{std::numeric_limits<handle_int_t>::max()};

  const program_handle k_program_none{k_none_value};
  const program_uniform_handle k_program_uniform_none{k_none_value};
  const texture_object_handle k_texture_object_none{k_none_value};
  const framebuffer_object_handle k_framebuffer_object_none{k_none_value};
  const buffer_object_handle k_buffer_object_none{k_none_value};

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
  // texture_object_handle
  //-------------------------------

  texture_object_handle device::texture_new() {
    texture_object_handle ret{};

    gl_gen_handle<texture_object_handle, &glGenTextures>(ret);

    return ret;
  }

  void device::texture_set_param(texture_object_target target, texture_param_ref param) {
    GLenum gltarget = gl_texture_target_to_enum(target);

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

  void device::texture_bind(texture_object_target target, texture_object_ref texture) {
    if (texture) {
      APISEL(
        GL_FN(glBindTexture(gl_texture_target_to_enum(target), 
                            (texture != k_texture_object_none)
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

  void device::texture_image_2d(texture_object_target target, 
                                miplevel_t mip, 
                                texture_int_fmt internal, 
                                dimension_t width, 
                                dimension_t height, 
                                texture_fmt format, 
                                primitive_type type, 
                                const void *pixels) {
    APISEL(
      GL_FN(glTexImage2D( gl_texture_target_to_enum(target),
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

  void device::texture_get_image( texture_object_ref texture, 
                                  miplevel_t level, 
                                  texture_fmt fmt,
                                  primitive_type ptype,
                                  bytesize_t size,
                                  void* out_pixels) {
    if (texture) {
      ASSERT(texture != k_texture_object_none);

      APISEL(
        GL_FN(glGetTextureImage(texture.value_as<GLuint>(),
                                static_cast<GLint>(level),
                                gl_fmt_to_enum(fmt),
                                gl_primitive_type_to_enum(ptype),
                                static_cast<GLsizei>(size),
                                out_pixels));
        ,
        APISTUB
      );
    }
  }

  //-------------------------------
  // framebuffer_object_handle
  //-------------------------------

  framebuffer_object_handle device::framebuffer_object_new() {
    framebuffer_object_handle ret{};
    glew_gen_handle<framebuffer_object_handle, &glGenFramebuffers>(ret);
    return ret;
  }

  void device::framebuffer_object_bind(fbo_target type, framebuffer_object_ref fbo) {
    if (fbo) {

      if (fbo == k_framebuffer_object_none) {
        GL_FN(glBindFramebuffer(gl_fbo_target_to_enum(type), 0));
      }
      else if (framebuffer_object_unbound_enforced()) {
        GL_FN(glBindFramebuffer(gl_fbo_target_to_enum(type),
                                fbo.value_as<GLuint>()));
      }

      m_curr_framebuffer_object = fbo;
    }
  }

  void device::framebuffer_object_texture_2d(fbo_target target, 
                                      fbo_attach_type attachment,
                                      texture_object_target texture_target,
                                      texture_object_ref texture,
                                      miplevel_t mip) {
    if (texture) {
      if (framebuffer_object_bound_enforced()) {
        ASSERT(texture != k_texture_object_none);

        GL_FN(glFramebufferTexture2D(gl_fbo_target_to_enum(target),
                                    gl_fbo_attach_to_enum(attachment),
                                    gl_texture_target_to_enum(texture_target),
                                    texture.value_as<GLuint>(),
                                    static_cast<GLint>(mip)));
      }
    }
  }

    // Assumes that a framebuffer is bound
  void device::framebuffer_object_read_buffer(fbo_attach_type attachment) {
    if (framebuffer_object_bound_enforced()) {
      GL_FN(glReadBuffer(gl_fbo_attach_to_enum(attachment)));
    }
  }

  // Assumes that a framebuffer is bound
  void device::framebuffer_object_read_pixels(dimension_t x, 
                               dimension_t y,
                               dimension_t width,
                               dimension_t height, 
                               texture_fmt fmt,
                               primitive_type type,
                               void* pixels) {
    if (framebuffer_object_bound_enforced()) {
      GL_FN(glReadPixels(static_cast<GLint>(x),
                         static_cast<GLint>(y),
                         static_cast<GLsizei>(width),
                         static_cast<GLsizei>(height),
                         gl_fmt_to_enum(fmt),
                         gl_primitive_type_to_enum(type),
                         pixels));
    }
  }

  bool device::framebuffer_object_ok() const {
    bool ret = false;
    if (framebuffer_object_bound_enforced()) {
      GLenum result;
      GL_FN(result = glCheckFramebufferStatus(GL_FRAMEBUFFER));

      ret = result == GL_FRAMEBUFFER_COMPLETE;
      
      if (!ret) {
        write_logf("FRAMEBUFFER BIND ERROR: code returned = 0x%x", result);
      }
    }
    return ret;
  }

  //-------------------------------
  // buffer_object_handle
  //-------------------------------

  buffer_object_handle device::buffer_object_new() {
    buffer_object_handle ret{};
    glew_gen_handle<buffer_object_handle, &glGenBuffers>(ret);
    return ret;
  }

  void device::buffer_object_bind(buffer_object_target target, buffer_object_ref obj) {
    if (buffer_object_unbound_enforced(target)) {
      GL_FN(glBindBuffer(gl_buffer_target_to_enum(target), 
                         obj.value_as<GLuint>()));

      m_curr_buffer_object[target] = obj;
    }
  }

  void device::buffer_object_unbind(buffer_object_target target) {
    if (buffer_object_bound_enforced(target)) {
      GL_FN(glBindBuffer(gl_buffer_target_to_enum(target),
                         0));

      m_curr_buffer_object[target] = k_buffer_object_none;
    }
  }

  void device::buffer_object_set_data(buffer_object_target target, bytesize_t size, const void* data, buffer_object_usage usage) {
    if (buffer_object_bound_enforced(target)) {
      GL_FN(glBufferData(gl_buffer_target_to_enum(target),
                        static_cast<GLsizeiptr>(size),
                        static_cast<const GLvoid*>(data),
                        gl_buffer_usage_to_enum(usage)));
    }
  }

  void device::buffer_object_draw_vertices(raster_method method, offset_t offset, count_t count) {
    if (buffer_object_bound_enforced(buffer_object_target::vertex)) {
      GL_FN(glDrawArrays(gl_raster_method_to_enum(method),
                         static_cast<GLint>(offset),
                         static_cast<GLsizei>(count)));
    }
  }

  //-------------------------------
  // viewport
  //-------------------------------

  void device::viewport_set(dimension_t x, dimension_t y, dimension_t width, dimension_t height) {
    GL_FN(glViewport(x, y, width, height));
  }

  void device::viewport_get(dimension_t& out_x, dimension_t& out_y, dimension_t& out_width, dimension_t& out_height) {
    GLint viewport[4] = {0};
    GL_FN(glGetIntegerv(GL_VIEWPORT, &viewport[0]));

    out_x = static_cast<dimension_t>(viewport[0]);
    out_y = static_cast<dimension_t>(viewport[1]);
    out_width = static_cast<dimension_t>(viewport[2]);
    out_height = static_cast<dimension_t>(viewport[3]);
  }
}