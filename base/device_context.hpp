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

  int m_screen_width{0};
  int m_screen_height{0};

  bool initialized{false};

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
    free_glfw();
  }

  int width() const { return m_screen_width; }
  int height() const { return m_screen_height; }

  template <typename T>
  T width() const { return static_cast<T>(m_screen_width); }
  
  template <typename T>
  T height() const { return static_cast<T>(m_screen_height); }
  
  const GLFWwindow* window() const { 
    return glfw_window; 
  }

  GLFWwindow* window() { 
    return glfw_window; 
  }  

  bool ok() const { 
    return initialized; 
  }

  void glfw_settings() {
    switch (g_conf.api_backend) {
      case gapi::backend::opengl:
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_VERSION_MAJOR);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_VERSION_MINOR);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
        break;
      case gapi::backend::vulkan:
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        break;
    }

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GL_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);

    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
  }

  bool init(int screen_width, int screen_height) {
    glfwSetErrorCallback(error_callback);

    bool success = true;
    bool glfw_init = false;

    m_screen_width = screen_width;
    m_screen_height = screen_height;

    if (glfwInit()) {
      glfw_init = true;

      glfw_settings();

      

      glfw_window = glfwCreateWindow(screen_width, 
                                screen_height, 
                                "Renderer", 
                                g_conf.fullscreen ? glfwGetPrimaryMonitor() : NULL, 
                                NULL);
      if (glfw_window != nullptr) {

        glfwMakeContextCurrent(glfw_window);

        if (g_conf.api_backend == gapi::backend::opengl) {
          GLenum init_result;
          
          glewExperimental = GL_TRUE;
          init_result = glewInit();

          success = init_result == GLEW_OK;
          
          if (!success) {
            printf("Glew ERROR: %s\n", glewGetErrorString(init_result));
          }
        }
	else {
	  success = true;
	}

        if (success) {
          glfwSetKeyCallback(glfw_window, key_callback);
          glfwSetCursorPosCallback(glfw_window, cursor_position_callback);
          glfwSetMouseButtonCallback(glfw_window, mouse_button_callback);
        }
      } 
      else {
        success = false;
      }
    } 
    else {
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
};
