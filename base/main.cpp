#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "render.h"

static const char* g_shader_vertex = GLSL(layout(location = 0) in vec3 in_Position;
                                          layout(location = 1) in vec4 in_Color;

                                          smooth out vec4 frag_Color;

                                          void main() {
                                            gl_Position = vec4(in_Position, 1.0);
                                            frag_Color = in_Color;
                                          });

static const char* g_shader_fragment = GLSL(smooth in vec4 frag_Color;
                                            out vec4 fb_Color;

                                            void main() {
                                              fb_Color = frag_Color;
                                            });
struct {
  GLuint vao;
  GLuint vbo;
  GLuint program;
} static g_api_data = {
  0
};

struct gl_color_vertex {
  glm::vec3 position;
  glm::vec4 color;
};

static void init_api_data() {
  float s = 0.5f;
  float t = -0.5f;
  
  float vertices[] = {
    0.0f, 0.0f, t, 1.0f, 0.0f, 0.0f, 1.0f,
    s, 0.0f, t, 1.0f, 1.0f, 1.0f, 1.0f,
    s, s, t, 0.0f, 0.0f, 1.0f, 1.0f
  };
  
  GL_FN(glGenBuffers(1, &g_api_data.vbo));
  GL_FN(glGenVertexArrays(1, &g_api_data.vao));
  
  GL_FN(glBindVertexArray(g_api_data.vao));
  GL_FN(glBindBuffer(GL_ARRAY_BUFFER, g_api_data.vbo));

  GL_FN(glEnableVertexAttribArray(0));
  GL_FN(glVertexAttribPointer(0,
                              3,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(float) * 7,
                              (void*) 0));

  GL_FN(glEnableVertexAttribArray(1));
  GL_FN(glVertexAttribPointer(1,
                              4,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(float) * 7,
                              (void*)(sizeof(float) * 3)));

  GL_FN(glBufferData(GL_ARRAY_BUFFER,
                     sizeof(vertices),
                     &vertices[0],
                     GL_STATIC_DRAW));

  g_api_data.program = make_program(g_shader_vertex, g_shader_fragment);

  GL_FN(glViewport(0, 0, 640, 480));
  
  GL_FN(glDisable(GL_CULL_FACE));
  GL_FN(glEnable(GL_DEPTH_TEST));
  GL_FN(glDepthFunc(GL_LEQUAL));
  GL_FN(glClearDepth(1.0f));
}

static void error_callback(int error, const char* description) {
  fputs(description, stdout);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GL_TRUE);
}

static void render(GLFWwindow* window) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  GL_FN(glUseProgram(g_api_data.program));
  GL_FN(glDrawArrays(GL_TRIANGLES, 0, 3));
  GL_FN(glUseProgram(0));
}

int main(void) {
  GLFWwindow* window;

  glfwSetErrorCallback(error_callback);

  if (!glfwInit())
    exit(EXIT_FAILURE);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

  window = glfwCreateWindow(640, 480, "OpenGL Boilerplate", NULL, NULL);
  if (!window) {
    goto error;
  }

  glfwMakeContextCurrent(window);
  
  glewExperimental = GL_TRUE;

  {
    GLenum r = glewInit();
    if (r != GLEW_OK) { 
      printf("Glew ERROR: %s\n", glewGetErrorString(r));
      goto error;
    }
  }

  glfwSetKeyCallback(window, key_callback);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  init_api_data();
  
  while (!glfwWindowShouldClose(window)) {
    render(window);

    glfwSwapBuffers(window);
    glfwPollEvents();
    //glfwWaitEvents();
  }

  glfwDestroyWindow(window);

  glfwTerminate();
  exit(EXIT_SUCCESS);
 error:
  glfwTerminate();
  exit(EXIT_FAILURE);
}
