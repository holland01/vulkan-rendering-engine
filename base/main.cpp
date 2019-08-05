#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <GL/glew.h>

#include <GLFW/glfw3.h>
#include <memory>
#include <functional>
#include <array>

#include "common.h"
#include "textures.h"
#include "util.h"
#include "programs.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

std::vector<type_module*> g_modules;

textures::index_type g_skybox_texture{textures::k_uninit};

GLuint g_vao = 0;

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

  auto calc_aspect() const {
    return static_cast<float>(view_width) / static_cast<float>(view_height);
  }
  
  void reset_proj() {   
    set_proj_from_fovy(45.0f);
  }

  void set_proj_from_fovy(float fovy) {
    proj = glm::perspective(fovy, calc_aspect(), 0.1f, 1000.0f);
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
    
    unbind();
  }

  auto num_vertices() const {
    return static_cast<int>(data.size());
  }

  auto add_triangle(const v3& a_position, const v4& a_color, const v2& a_uv,
                    const v3& b_position, const v4& b_color, const v2& b_uv,
                    const v3& c_position, const v4& c_color, const v2& c_uv) {
    vertex a = {
      a_position,
      a_color,
      a_uv
    };
    
    vertex b = {
      b_position,
      b_color,
      b_uv
    };
    
    vertex c = {
      c_position,
      c_color,
      c_uv
    };

    auto offset = num_vertices();

    data.push_back(a);
    data.push_back(b);
    data.push_back(c);
    
    return offset;

  }
  
  auto add_triangle(const v3& a_position, const v4& a_color,
                    const v3& b_position, const v4& b_color,
                    const v3& c_position, const v4& c_color) {
    v2 defaultuv(glm::zero<v2>());
    
    return add_triangle(a_position, a_color, defaultuv,
                        b_position, b_color, defaultuv,
                        c_position, c_color, defaultuv);
  }
  
} static g_vertex_buffer;

struct models {
  enum transformorder {
    // srt -> translate first, then rotate, then scale. Order is backwards
    // on purpose, since mathematically this is how transforms work with the current
    // conventions used.
    transformorder_srt = 0, 
    transformorder_lookat,
    transformorder_skybox,
    transformorder_count
  };

  using transform_fn_type = std::function<glm::mat4(int model)>;
  using index_type = int16_t;

  static const inline index_type k_uninit = -1;
  
  std::array<transform_fn_type, transformorder_count> __table = {
    [this](int model) { return scale(model) * rotate(model) * translate(model); },
    [this](int model) { return glm::inverse(g_view.view())
                        * glm::lookAt(look_at.eye, look_at.center, look_at.up); },
    [this](int model) {
      return glm::translate(glm::mat4(1.0f), g_view.position) * scale(model);
    }
  };
  
  std::vector<v3> positions;
  std::vector<v3> scales;
  std::vector<v3> angles;  
  std::vector<index_type> vertex_offsets;
  std::vector<index_type> vertex_counts;
  std::vector<bool> draw;

  struct {
    v3 eye;
    v3 center;
    v3 up;
  } look_at = {
    glm::zero<v3>(),
    glm::zero<v3>(),
    glm::zero<v3>()
  };

  int modind_tri = 0;
  int modind_sphere = 1;
  
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

    draw.push_back(true);

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

  void render(int model, transformorder to) const {
    if (draw.at(model) == true) {
      glm::mat4 T = __table[to](model);
      glm::mat4 mv = g_view.view() * T;

      g_programs.up_mat4x4("unif_ModelView", mv);
      g_programs.up_mat4x4("unif_Projection", g_view.proj);
    
      auto ofs = vertex_offsets[model];
      auto count = vertex_counts[model];

      GL_FN(glDrawArrays(GL_TRIANGLES,
                         ofs,
                         count));
    }
  }
} static g_models;


static models::index_type g_skybox_model{models::k_uninit};


auto new_sphere(const v3& position = glm::zero<v3>(), const v3& scale = v3(1.0f)) {
  auto offset = g_vertex_buffer.num_vertices();

  float step = 0.1f;

  auto count = 0;
  
  auto cart = [&scale](float phi, float theta) {
    v3 ret;
    ret.x = scale.x * glm::cos(theta) * glm::cos(phi);
    ret.y = scale.y * glm::sin(phi);
    ret.z = scale.z * glm::sin(theta) * glm::cos(phi);
    return ret;
  };
  
  for (float phi = -glm::half_pi<float>(); phi <= glm::half_pi<float>(); phi += step) {
    for (float theta = 0.0f; theta <= glm::two_pi<float>(); theta += step) {
      auto color = v4(1.0f);
    
      auto a = cart(phi, theta);
      auto b = cart(phi, theta + step);
      auto c = cart(phi + step, theta + step);
      auto d = cart(phi + step, theta);

      g_vertex_buffer.add_triangle(a, color,
                                   b, color,
                                   c, color);

      g_vertex_buffer.add_triangle(c, color,
                                   d, color,
                                   a, color);

      count += 6;
    }
  }

  return g_models.new_model(offset, count, position, scale);
}

auto new_cube(const v3& position = glm::zero<v3>(), const v3& scale = v3(1.0f), const v4& color = v4(1.0f)) {
  std::array<float, 36 * 3> vertices = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
    1.0f,  1.0f, -1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
    1.0f, -1.0f,  1.0f
  };

  auto offset = g_vertex_buffer.num_vertices();
  
  for (auto i = 0; i < vertices.size(); i += 9) {
    v3 a(vertices[i + 0], vertices[i + 1], vertices[i + 2]);
    v3 b(vertices[i + 3], vertices[i + 4], vertices[i + 5]);
    v3 c(vertices[i + 6], vertices[i + 7], vertices[i + 8]);

    g_vertex_buffer.add_triangle(a, color,
                                 b, color,
                                 c, color);
  }

  return g_models.new_model(offset, 36, position, scale);
}


static std::vector<models::index_type> g_model_ids;

struct capture {
  GLuint fbo, tex, rbo;
  int width, height;

  capture()
    :
    fbo(0),
    tex(0),
    rbo(0),
    width(SCREEN_WIDTH),
    height(SCREEN_HEIGHT){
  }

  void load() {
    GL_FN(glGenFramebuffers(1, &fbo));
    bind();

    GL_FN(glGenTextures(1, &tex));
    GL_FN(glBindTexture(GL_TEXTURE_2D, tex));
    GL_FN(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL));
    GL_FN(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_FN(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_FN(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_FN(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));


    GL_FN(glBindTexture(GL_TEXTURE_2D, 0));

    // attach it to currently bound framebuffer object
    GL_FN(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0));

    GL_FN(glGenRenderbuffers(1, &rbo));
    GL_FN(glBindRenderbuffer(GL_RENDERBUFFER, rbo)); 
    GL_FN(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height));  
    GL_FN(glBindRenderbuffer(GL_RENDERBUFFER, 0));

    GL_FN(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo));

    ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    unbind();
  }
  
  void bind() const {
    GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
  }

  void unbind() const {
    GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  }

  void sample_begin(const std::string& sampler, int slot) const {
    GL_FN(glBindTexture(GL_TEXTURE_2D, tex));
    GL_FN(glActiveTexture(GL_TEXTURE0 + static_cast<decltype(GL_TEXTURE0)>(slot)));
    g_programs.up_int(sampler, slot);
  }

  void sample_end() const {
    GL_FN(glBindTexture(GL_TEXTURE_2D, 0));
  }
  
} static g_capture;

static void init_api_data() { 
  g_view.reset_proj();

  g_programs.load();
  g_capture.load();
  
  GL_FN(glGenVertexArrays(1, &g_vao));
  GL_FN(glBindVertexArray(g_vao));

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
  
  g_skybox_model = new_cube();
  
  g_model_ids.push_back(new_sphere(v3(0.0f, 5.0f, -10.0f)));
  g_model_ids.push_back(g_skybox_model);
  
  g_vertex_buffer.reset();

#define p(path__) fs::path("skybox0") / fs::path(path__ ".jpg")
  textures::cubemap_paths_type paths = {
    p("right"),
    p("left"),
    p("top"),
    p("bottom"),
    p("front"),
    p("back")
  };
#undef p

  g_models.scales[g_skybox_model] = v3(100.0f);
  
  g_skybox_texture = g_textures.new_cubemap(paths);
  
  GL_FN(glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT));
  
  GL_FN(glDisable(GL_CULL_FACE));
  GL_FN(glEnable(GL_DEPTH_TEST));
  GL_FN(glDepthFunc(GL_LEQUAL));
  GL_FN(glClearDepth(1.0f));
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

#define DRAW_MODELS(transform_order) for (auto id: g_model_ids) { g_models.render(id, transform_order); }

#define SET_CLEAR_COLOR_V4(v) GL_FN(glClearColor((v).r, (v).g, (v).b, (v).a)) 
#define CLEAR_COLOR_DEPTH GL_FN(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT))

void test_sphere_fbo_pass(const v4& background) {
  g_capture.bind();

  SET_CLEAR_COLOR_V4(background);
  CLEAR_COLOR_DEPTH;

  g_vertex_buffer.bind();

  {
    use_program u(g_programs.default_fb);
    
    g_models.look_at.eye = g_models.positions[g_models.modind_sphere];
    g_models.look_at.center = g_models.positions[g_models.modind_tri];
    g_models.look_at.up = v3(0.0f, 1.0f, 0.0f);

    // we're rendering from the sphere's perspective
    g_models.draw[g_models.modind_sphere] = false;

    DRAW_MODELS(models::transformorder_lookat);
  }
    
  g_vertex_buffer.unbind();
  g_capture.unbind();
}

void test_render_to_quad(const v4& background) {
  SET_CLEAR_COLOR_V4(background);
  CLEAR_COLOR_DEPTH;

  {
    use_program u(g_programs.default_rtq);
      
    g_capture.sample_begin("unif_TexSampler", 0);

    GL_FN(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    
    g_capture.sample_end();
  }
}

void test_draw_reflect_sphere(const v4& background) {
  SET_CLEAR_COLOR_V4(background);
  CLEAR_COLOR_DEPTH;
  
  g_vertex_buffer.bind();
  {
    use_program u(g_programs.default_mir);

    g_capture.sample_begin("unif_TexFramebuffer", 0);
      
    g_programs.up_vec3("unif_LookCenter", g_view.position - g_models.look_at.eye);

    g_models.draw[g_models.modind_sphere] = true;

    DRAW_MODELS(models::transformorder_srt);

    g_capture.sample_end();
  }
  g_vertex_buffer.unbind();
}

void test_draw_skybox_scene(const v4& background) {
  SET_CLEAR_COLOR_V4(background);
  CLEAR_COLOR_DEPTH;

  g_vertex_buffer.bind();
  {
    use_program u(g_programs.skybox);

    int slot = 0;
    
    g_textures.bind(g_skybox_texture, slot);

    g_programs.up_int("unif_TexCubeMap", slot);
    
    g_models.render(g_skybox_model,
                    models::transformorder_srt);
    
    g_textures.unbind(g_skybox_texture, slot);
  }
  g_vertex_buffer.unbind();
}

static void render() {
  v4 background(0.0f, 0.3f, 0.0f, 1.0f);

  //test_sphere_fbo_pass(background);
  //test_draw_reflect_sphere(background);

  test_draw_skybox_scene(background);
}

int main(void) {
  g_programs.registermod();
  g_textures.registermod();
  
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

  init_api_data();
  
  while (!glfwWindowShouldClose(window)) {
    g_view(g_move_state);
    
    render();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  for (auto module : g_modules) {
    module->free_mem();
  }
  
  glfwDestroyWindow(window);

  glfwTerminate();
  exit(EXIT_SUCCESS);
 error:
  glfwTerminate();
  exit(EXIT_FAILURE);
}
