#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

// key facts about transformations:
// - a matrix transform represents the source space from the perspective
// of the destination space

struct view_data {
  glm::mat4 proj;    // proj: camera to clip space transform

  glm::mat3 orient;  // orient: camera orientation
  
  glm::vec3 position;  // position: theoretical position of the camera: technically,
                       // view space is always centered at a specific location that's
                       // k units of distance before the near plane. Thus, it's always
                       // looking down -z. In other words, the viewing volume (frustum)
                       // is always in the same space, and the camera facing its near plane is also
                       // always at the same space. The "position" vector we have here
                       // is used to provide the illusion of a moving camera. More on this later.

  float step;          // step: move step amount

  uint16_t view_width;
  uint16_t view_height;
  
  view_data(uint16_t width, uint16_t height)
    : proj(1.0f),
      orient(1.0f),
      position(0.0f),
      step(0.1f),
      view_width(width),
      view_height(height)
  {}

  // Pick a translation direction, and this will modify it
  // so that any object's within the viewing volume will be oriented/moved
  // to provide the illusion that the viewer is moving in the requested direction.
#define RET_VIEW_DIR(x, y, z) glm::vec3 dir(glm::inverse(orient) * glm::vec3(x, y, z)); return dir
  
  glm::vec3 view_up() const { RET_VIEW_DIR(0.0f, 1.0f, 0.0f); }
  glm::vec3 view_down() const { RET_VIEW_DIR(0.0f, -1.0f, 0.0f); }

  glm::vec3 view_right() const { RET_VIEW_DIR(1.0f, 0.0f, 0.0f); }
  glm::vec3 view_left() const { RET_VIEW_DIR(-1.0f, 0.0f, 0.0f); }
  
  glm::vec3 view_front() const { RET_VIEW_DIR(0.0f, 0.0f, -1.0f); }
  glm::vec3 view_back() const { RET_VIEW_DIR(0.0f, 0.0f, 1.0f); }

#undef RET_VIEW_DIR

  //
  // view(): compute the model to camera transform.
  //
  // We need two proper transforms for this:
  // the first is our translation, the second is 
  // the orientation for the view.
  // Let T be the translation matrix and O
  // be our orientation.
  // 
  // Back to the idea of the "position",
  // mentioned earlier: we're performing an illusion
  // in terms of how we express the camera's movement
  // relative to the world. In other words, the camera _isn't_ moving,
  // but the world around it is.
  //
  // Suppose we have some model with a corresponding transform M
  // that we wish to render. We know that M is going to have its own
  // separate orientation and position offset within our world. 
  // Thus, the translation we're creating here is designed to move
  // the model represented by M a series of units _closer_ to the viewing volume.
  //
  // That series of units is our camera's "position".
  //
  // Suppose p is our position vector, then T equals:
  //
  // T = | 1 0 0 -p.x |
  //     | 0 1 0 -p.y |
  //     | 0 0 1 -p.z |
  //     | 0 0 0   1  |
  //
  // Now, suppose we don't care about orientation for the moment.
  // Ignoring the camera to clip transform as well,
  // when the model M transform is used with T, we can express the resulting
  // transform Q as:
  //
  // Q = TM
  //
  // If our resulting transform Q contains a translation whose coordinates
  // lie _outside_ of the viewing volume, then the object will be clipped,
  // and we can think of the viewer simply not having moved far enough to spot
  // the object.
  //
  // However, the further they "move" in the direction of the object, the more the
  // object itself will actually move toward the viewing volume.
  // 
  // Now, as far as the orientation, what we want to do is maintain that same principle,
  // but ensure that objects in our viewing volume maintain their distance irrespective
  // of the orientation of the viewer.
  //
  // A mouse move to the left is supposed to produce a clockwise rotation
  // about the vertical axis of any object in the viewing volume that we wish to render.
  //
  // We don't want the vertices of an object to be rotated about the object's center of origin though;
  // rather, we want to them to be rotated about the center of the origin of the _viewer_.
  // Thus, we perform the translation _first_, and then orientation second.
  //
  glm::mat4 view() const {
    return glm::mat4(orient) * glm::translate(glm::mat4(1.0f),
                                              -position); 
  }

  //
  // operator ():
  //
  // move struct is volatile because we're using bitsets here.
  //
  // very simple: if the corresponding direction is set to 1, 
  // we compute the appropriate offset for that direction and add its
  // scaled value to our position vector.
  //
  volatile struct move {
    uint8_t up : 1;
    uint8_t down : 1;
    uint8_t right : 1;
    uint8_t left : 1;
    uint8_t front : 1;
    uint8_t back : 1;
    uint8_t __rem : 2;
  };


#define TESTDIR(move, dir)                        \
  if ((move).dir == true) {                       \
    this->position += view_##dir() * this->step;  \
  }
  
  void operator ()(move m) {
    TESTDIR(m, up);
    TESTDIR(m, down);
    TESTDIR(m, right);
    TESTDIR(m, left);
    TESTDIR(m, front);
    TESTDIR(m, back);
  }
  
#undef TESTDIR
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
