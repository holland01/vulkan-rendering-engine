#include "gapi.hpp"
#include "render_pipeline.hpp"



#define APISEL(opengl, vulkan) do { opengl } while (0)

#define APISTUB (void);

namespace gapi {
  static constexpr handle_int_t k_null_value{std::numeric_limits<handle_int_t>::max()};
  const program_handle k_null_program{k_null_value};

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

  void device::set_active_texture_unit(int_t unit) {
    APISEL(
      GL_FN(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit)));
      ,
      APISTUB
    );
  }

  //-------------------------------
  // compiled_shader_handle
  //-------------------------------

  compiled_shader_handle device::create_shader(shader_type type) {
    compiled_shader_handle h{0};

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

      h.set_value(shader);
      ,
      APISTUB
    );

    return h;
  }

  void device::delete_shader(compiled_shader_mut_ref shader) {
    shader.assert_ok();

    APISEL(
      GL_FN(glDeleteShader(shader.value_as<GLuint>()));
      shader.set_value(0);
      ,
      APISTUB
    );
  }

  void device::attach_shader( program_ref program, 
                              compiled_shader_ref shader) {
    program.assert_ok();
    shader.assert_ok();

    APISEL(
      GL_FN(glAttachShader(program.value_as<GLuint>(), 
                            shader.value_as<GLuint>()));
      ,
      APISTUB
    );
  }

  void device::detach_shader( program_ref program,
                              compiled_shader_ref shader) {
    program.assert_ok();
    shader.assert_ok();

    APISEL(
      GL_FN(glDetachShader(program.value_as<GLuint>(), 
                           shader.value_as<GLuint>()));
      ,
      APISTUB
    );
  }

  void device::compile_shader(compiled_shader_ref shader) {
    shader.assert_ok();

    APISEL(
      GL_FN(glCompileShader(shader.value_as<GLuint>()));
      ,
      APISTUB
    );
  }

  bool device::compile_shader_success(compiled_shader_mut_ref shader) {
    shader.assert_ok();

    bool ret = true;

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

        delete_shader(shader);
      }
      ,
      APISTUB
    );
    return ret;
  }

  void device::set_shader_source( compiled_shader_mut_ref shader, 
                                  const std::string& source) {
    shader.assert_ok();

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

  //-------------------------------
  // program_handle
  //-------------------------------

  program_handle device::create_program() {
    program_handle h{0};

    APISEL(
      GLuint program = 0;
      GL_FN(program = glCreateProgram());
      h.set_value(program);
      ,
      APISTUB
    );

    return h;
  }

  void device::delete_program(program_mut_ref program) {
    program.assert_ok();

    APISEL(
      GL_FN(glDeleteProgram(program.value_as<GLuint>()));
      program.set_value(0);
      ,
      APISTUB
    );
  }

  void device::link_program(program_ref program) {
    program.assert_ok();

    APISEL(
      GL_FN(glLinkProgram(program.value_as<GLuint>()));
      ,
      APISTUB
    );
  }

  bool device::link_program_success(program_mut_ref program) {
    program.assert_ok();

    bool ret = true;

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

        delete_program(program);

        ret = false;
      }
      ,
      APISTUB
    );

    return ret;
  }

  void device::use_program(program_ref program) {
    program.assert_ok();

    APISEL(
      GL_FN(glUseProgram(program.value() != k_null_value 
                          ? program.value_as<GLuint>()
                          : 0));
      ,
      APISTUB
    );
  }

  program_handle device::make_program(const std::string& vertex, 
                                             const std::string& fragment) {
    program_handle program_ret = create_program();

    compiled_shader_handle vshader = 
      create_shader(shader_type::vertex);
    
    set_shader_source(vshader, vertex);
    compile_shader(vshader);

    if (compile_shader_success(vshader)) {
      compiled_shader_handle fshader = 
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
        delete_program(program_ret);
      }
    } 
    else {
      delete_program(program_ret);
    }

    return program_ret;
  }
}