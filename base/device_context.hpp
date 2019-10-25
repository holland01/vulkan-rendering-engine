#pragma once

#include "common.hpp"

// GLFW callbacks (defined in main.cpp)
extern void error_callback(int error, const char* description);
extern void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
extern void mouse_button_callback(GLFWwindow* window, int button, int action, int mmods);
extern void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

class device_context {
private:
  GLFWwindow* glfw_window{nullptr};

  GLsync sync_current{0};

  bool initialized{false};

  void free_sync() {
    if (sync_current) {
      GL_FN(glDeleteSync(sync_current));
      sync_current = 0;
    }
  }

  void free_glfw() {
    if (window() != nullptr) {
      glfwDestroyWindow(window());
      glfw_window = nullptr;
      glfwTerminate();
    }
  }

public:
  using time_type = GLuint64;

  ~device_context() {
    free_sync();
    free_glfw();
  }

  const GLFWwindow* window() const { 
    return glfw_window; 
  }

  GLFWwindow* window() { 
    return glfw_window; 
  }  

  bool ok() const { 
    return initialized; 
  }

  bool init(int screen_width, int screen_height) {
    glfwSetErrorCallback(error_callback);

    bool success = true;
    bool glfw_init = false;

    if (glfwInit()) {
      glfw_init = true;

      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_VERSION_MAJOR);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_VERSION_MINOR);
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
      glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
      glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
      glfwWindowHint(GLFW_SRGB_CAPABLE, GL_TRUE);
      glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);

      glfwWindowHint(GLFW_DEPTH_BITS, 24);

      glfwWindowHint(GLFW_RED_BITS, 8);
      glfwWindowHint(GLFW_GREEN_BITS, 8);
      glfwWindowHint(GLFW_BLUE_BITS, 8);
      glfwWindowHint(GLFW_ALPHA_BITS, 8);

      glfw_window = glfwCreateWindow(screen_width, 
                                screen_height, 
                                "Renderer", 
                                g_conf.fullscreen ? glfwGetPrimaryMonitor() : NULL, 
                                NULL);
      if (glfw_window != nullptr) {
        GLenum init_result;

        glfwMakeContextCurrent(glfw_window);

        glewExperimental = GL_TRUE;
        init_result = glewInit();

        success = init_result == GLEW_OK;

        if (success) {
          glfwSetKeyCallback(glfw_window, key_callback);
          glfwSetCursorPosCallback(glfw_window, cursor_position_callback);
          glfwSetMouseButtonCallback(glfw_window, mouse_button_callback);
        } else {
          printf("Glew ERROR: %s\n", glewGetErrorString(init_result));
        }
      } else {
        success = false;
      }
    } else {
      success = false;
    }

    if (!success) {
      if (glfw_init) {
        glfwTerminate();
      }
    }
    
    initialized = success;

    return success;
  }

  void fence_sync() {
    free_sync();
    GL_FN(sync_current = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0));
  }

  void wait_sync(time_type timeout_ns = time_type(100)) const {
    if (sync_current) {
      bool await = true;
      while (await) {
        GLenum wait = glClientWaitSync(sync_current, GL_SYNC_FLUSH_COMMANDS_BIT, timeout_ns);
        await = !(wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED || wait == GL_TIMEOUT_EXPIRED);
        ASSERT(wait != GL_TIMEOUT_EXPIRED);
      }
    }
  }
};
