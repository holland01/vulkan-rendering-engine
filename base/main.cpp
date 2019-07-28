#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <memory>
#include <functional>
#include <array>

#include "render.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

using v3 = glm::vec3;
using v4 = glm::vec4;

static const
char* g_shader_vertex = GLSL(layout(location = 0) in vec3 in_Position;
                             layout(location = 1) in vec4 in_Color;

                             smooth out vec4 frag_Color;

                             uniform mat4 unif_ModelView;
                             uniform mat4 unif_Projection;

                             void main() {
                               vec4 clip = unif_Projection * unif_ModelView * vec4(in_Position, 1.0);
                               gl_Position = clip;
                               frag_Color = abs(clip / clip.w);
                             });

static const
char* g_shader_fragment = GLSL(smooth in vec4 frag_Color;
                               out vec4 fb_Color;

                               void main() {
                                 fb_Color = frag_Color;
                               });
struct {
  GLuint vao;
  GLuint program;
} static g_api_data = {
  0,
  0
};

struct move_state {
  uint8_t up : 1;
  uint8_t down : 1;
  uint8_t right : 1;
  uint8_t left : 1;
  uint8_t front : 1;
  uint8_t back : 1;
  uint8_t __rem : 2;
} static  g_move_state = {
  0,0,0,0,0,0,0
};


// key facts about transformations:
// - a matrix transform represents the source space from the perspective
// of the destination space

struct view_data {
  glm::mat4 proj;    // proj: camera to clip space transform

  glm::mat3 orient;  // orient: camera orientation
  
  v3 position;  // position: theoretical position of the camera: technically,
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
      step(0.01f),
      view_width(width),
      view_height(height) {
  }

  void reset_proj() {
    float aspect = static_cast<float>(view_width) / static_cast<float>(view_height);
    proj = glm::perspective(45.0f, aspect, 0.1f, 100.0f);
  }

  //
  // view_<dir>(): series of functions used for "moving" the camera in a particular direction.
  //
  // Pick a translation direction, and RET_VIEW_DIR will produce a transformed vector offset
  // such that any object's within the viewing volume will be oriented/moved
  // to provide the illusion that the viewer is moving in the requested direction.
  //
#define RET_VIEW_DIR(x, y, z) v3 dir(glm::inverse(orient) * v3(x, y, z)); return dir
  
  v3 view_up() const { RET_VIEW_DIR(0.0f, 1.0f, 0.0f); }
  v3 view_down() const { RET_VIEW_DIR(0.0f, -1.0f, 0.0f); }

  v3 view_right() const { RET_VIEW_DIR(1.0f, 0.0f, 0.0f); }
  v3 view_left() const { RET_VIEW_DIR(-1.0f, 0.0f, 0.0f); }
  
  v3 view_front() const { RET_VIEW_DIR(0.0f, 0.0f, -1.0f); }
  v3 view_back() const { RET_VIEW_DIR(0.0f, 0.0f, 1.0f); }

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
  // operator (): updates position
  //
  // move struct is  because we're using bitsets here.
  //
  // very simple: if the corresponding direction is set to 1, 
  // we compute the appropriate offset for that direction and add its
  // scaled value to our position vector.
  //
  // Our goal is to let whatever key-input methods are used
  // be handled separately from this class.
  //
  

#define TESTDIR(move, dir)                        \
  if ((move).dir == true) {                       \
    this->position += view_##dir() * this->step;  \
  }
  
  void operator ()( move_state m) {
    TESTDIR(m, up);
    TESTDIR(m, down);
    TESTDIR(m, right);
    TESTDIR(m, left);
    TESTDIR(m, front);
    TESTDIR(m, back);
  }
  
#undef TESTDIR
};

static view_data g_view(SCREEN_WIDTH, SCREEN_HEIGHT);

//
// glsl_uniform: wrapper around GLSL host<->device uniform operations.
//
// These operations boil down to querying for the reference IDs which are bound to their
// variable names and attained via the graphics API and setting the value of the uniform for the next
// invocation of the program the uniform belongs to.
//
struct glsl_uniform {
  GLint id;
  std::string name;

  glsl_uniform(const std::string& n)
    : id(-1),
      name(n) {
  }

  void load(GLuint program) {
    ASSERT(id == -1);
    GL_FN(id = glGetUniformLocation(program, name.c_str()));
  }

  GLint operator ()() const {
    ASSERT(id != -1);
    return id;
  }

  //
  // up_mat4x4(): sets the value of the matrix 
  // that is represented by 'id' for 'program'
  //
  // in this case, we don't need to "bind" the program to the opengl context
  // before performing the upload (as is necessary in pre-4.x versions of the API).
  //
  void up_mat4x4(GLuint program, const glm::mat4& m) const {
#if 1
    GL_FN(glUniformMatrix4fv(id, 1, GL_FALSE, &m[0][0]));
#else
    GL_FN(glProgramUniformMatrix4fv(program,
                                    id,
                                    1,
                                    GL_FALSE,
                                    &m[0][0] ));

#endif  
  }
};

static glsl_uniform g_unif_model_view("unif_ModelView");
static glsl_uniform g_unif_projection("unif_Projection");

struct vertex {
  v3 position;
  v4 color;
};

struct vertex_buffer {
  
  std::vector<vertex> data;
  
  mutable GLuint vbo;

  vertex_buffer()
    : vbo(0) {
  }
  
  void bind() const {
    GL_FN(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  }

  void unbind() const {
    GL_FN(glBindBuffer(GL_ARRAY_BUFFER, 0));
  }

  void push(const vertex& v) {
    data.push_back(v);
  }
  
  void reset() const {
    if (vbo == 0) {
      GL_FN(glGenBuffers(1, &vbo));
    }
    
    bind();

    GL_FN(glBufferData(GL_ARRAY_BUFFER,
                       sizeof(data[0]) * data.size(),
                       &data[0],
                       GL_STATIC_DRAW));
    
    GL_FN(glEnableVertexAttribArray(0));
    GL_FN(glVertexAttribPointer(0,
                                3,
                                GL_FLOAT,
                                GL_FALSE,
                                sizeof(vertex),
                                (void*) offsetof(vertex, position)));

    GL_FN(glEnableVertexAttribArray(1));
    GL_FN(glVertexAttribPointer(1,
                                4,
                                GL_FLOAT,
                                GL_FALSE,
                                sizeof(vertex),
                                (void*) offsetof(vertex, color)));


    unbind();
  }

  auto num_vertices() const {
    return static_cast<int>(data.size());
  }

  auto add_triangle(v3 a_position, v4 a_color,
                   v3 b_position, v4 b_color,
                   v3 c_position, v4 c_color) {
    vertex a = {
      a_position,
      a_color
    };        
    vertex b = {
      b_position,
      b_color    
    };                                                                  
    vertex c = {
      c_position,
      c_color
    };

    auto offset = num_vertices();

    data.push_back(a);
    data.push_back(b);
    data.push_back(c);

#if 0
    v4 a4(a_position, 1.0f);
    v4 b4(b_position, 1.0f);
    v4 c4(c_position, 1.0f);

    a4 = g_view.proj * a4;
    b4 = g_view.proj * b4;
    c4 = g_view.proj * c4;
#endif

    return offset;
  }
  
} static g_vertex_buffer;

struct models {
  enum transformorder {
    to_srt = 0,
    transformorder_count
  };

  using transform_fn_type = std::function<glm::mat4(int model)>;
  using index_type = uint16_t;
  
  std::array<transform_fn_type, transformorder_count> __table{
    [this](int model) { return scale(model) * rotate(model) * translate(model); }
  };
  
  std::vector<v3> positions;
  std::vector<v3> scales;
  std::vector<v3> angles;  
  std::vector<index_type> vertex_offsets;
  std::vector<index_type> vertex_counts;
  
  int new_model(index_type vbo_offset = 0,
                index_type num_vertices = 0,
                v3 position = glm::zero<v3>(),
                v3 scale = v3(1.0f),
                v3 angle = v3(0.0f)) {
    
    int id = static_cast<int>(positions.size());

    positions.push_back(position);
    scales.push_back(scale);
    angles.push_back(angle);

    vertex_offsets.push_back(vbo_offset);
    vertex_counts.push_back(num_vertices);

    return id;
  }
  
  glm::mat4 scale(int model) const {
    return glm::scale(glm::mat4(1.0f), scales.at(model));
  }

  glm::mat4 translate(int model) const {
    return glm::translate(glm::mat4(1.0f), positions.at(model)); 
  }

  glm::mat4 rotate(int model) const {
    glm::mat4 rot(1.0f);
    
    rot = glm::rotate(glm::mat4(1.0f), angles.at(model).x, v3(1.0f, 0.0f, 0.0f));
    rot = glm::rotate(glm::mat4(1.0f), angles.at(model).y, v3(0.0f, 1.0f, 0.0f)) * rot;
    rot = glm::rotate(glm::mat4(1.0f), angles.at(model).z, v3(0.0f, 0.0f, 1.0f)) * rot;
    
    return rot;
  }

  void render(int model, transformorder to, GLuint program) const {
    glm::mat4 T = __table[to](model);
    glm::mat4 mv = g_view.view() * T;

    g_unif_model_view.up_mat4x4(g_api_data.program, mv);
    g_unif_projection.up_mat4x4(g_api_data.program, g_view.proj);
    
    auto ofs = vertex_offsets[model];
    auto count = vertex_counts[model];

    GL_FN(glDrawArrays(GL_TRIANGLES,
                       ofs,
                       count));
  }
} static g_models;

static void init_api_data() { 
  g_view.reset_proj();

  GL_FN(glGenVertexArrays(1, &g_api_data.vao));
  GL_FN(glBindVertexArray(g_api_data.vao));

  {
    float s = 1.0f;
    float t = -3.5f;

    // returns 0; used for the model id
    auto offset = g_vertex_buffer.add_triangle(v3(-s, 0.0f, t), // a
                                               v4(1.0f, 0.0f, 0.0f, 1.0f),                            

                                               v3(s, 0.0f, t), // b
                                               v4(1.0f, 1.0f, 1.0f, 1.0f),
                               
                                               v3(0.0f, s, t), // c
                                               v4(0.0f, 0.0f, 1.0f, 1.0f));


    g_model_ids.push_back(g_models.new_model(offset, 3));
  }
  
  g_vertex_buffer.reset();
  
  g_api_data.program = make_program(g_shader_vertex, g_shader_fragment);

  GL_FN(glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT));
  
  GL_FN(glDisable(GL_CULL_FACE));
  GL_FN(glEnable(GL_DEPTH_TEST));
  GL_FN(glDepthFunc(GL_LEQUAL));
  GL_FN(glClearDepth(1.0f));

  g_unif_model_view.load(g_api_data.program);
  g_unif_projection.load(g_api_data.program);  
}

static void error_callback(int error, const char* description) {
  fputs(description, stdout);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
#define map_move_t(key, dir) case key: g_move_state.dir = true; break
#define map_move_f(key, dir) case key: g_move_state.dir = false; break
  
  if (action == GLFW_PRESS) {
    switch (key) {
    case GLFW_KEY_ESCAPE:
      glfwSetWindowShouldClose(window, GL_TRUE);
      break;
      
      map_move_t(GLFW_KEY_W, front);
      map_move_t(GLFW_KEY_S, back);
      map_move_t(GLFW_KEY_A, left);
      map_move_t(GLFW_KEY_D, right);
      map_move_t(GLFW_KEY_SPACE, up);
      map_move_t(GLFW_KEY_LEFT_SHIFT, down);

    default:
      break;
    }
  } else if (action == GLFW_RELEASE) {
    switch (key) {
      map_move_f(GLFW_KEY_W, front);
      map_move_f(GLFW_KEY_S, back);
      map_move_f(GLFW_KEY_A, left);
      map_move_f(GLFW_KEY_D, right);
      map_move_f(GLFW_KEY_SPACE, up);
      map_move_f(GLFW_KEY_LEFT_SHIFT, down);

    default:
      break;
    }
  }

#undef map_move_t
#undef map_move_f
}

static void render(GLFWwindow* window) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  GL_FN(glUseProgram(g_api_data.program));


  g_vertex_buffer.bind();

  for (auto id: g_model_ids) {  
    g_models.render(id,
                    models::to_srt,
                    g_api_data.program);
  }

  g_vertex_buffer.unbind();
  
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

  window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OpenGL Boilerplate", NULL, NULL);
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

  glClearColor(0.0f, 0.3f, 0.0f, 1.0f);

  init_api_data();
  
  while (!glfwWindowShouldClose(window)) {
    g_view(g_move_state);
    
    render(window);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);

  glfwTerminate();
  exit(EXIT_SUCCESS);
 error:
  glfwTerminate();
  exit(EXIT_FAILURE);
}
