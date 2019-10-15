#include "common.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <memory>
#include <functional>
#include <array>
#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <limits>

#include "textures.hpp"
#include "util.hpp"
#include "programs.hpp"
#include "geom.hpp"
#include "frame.hpp"
#include "vertex_buffer.hpp"
#include "models.hpp"
#include "view_data.hpp"
#include "scene_graph.hpp"

#include "render_pipeline.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#define SET_CLEAR_COLOR_V4(v) GL_FN(glClearColor((v).r, (v).g, (v).b, (v).a)) 
#define CLEAR_COLOR_DEPTH GL_FN(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT))

#define TEST_SPHERE_RADIUS R(5)
#define TEST_SPHERE_POS R3v(0, 10, 0)

#define ROOM_SPHERE_RADIUS R(30)
#define ROOM_SPHERE_POS R3v(0, 0, 0)

void modules::init() {
  framebuffer = new framebuffer_ops(SCREEN_WIDTH, SCREEN_HEIGHT);
  programs = new module_programs();
  textures = new module_textures();
  vertex_buffer = new module_vertex_buffer();
  models = new module_models();
  geom = new module_geom();

  view = new view_data(SCREEN_WIDTH, SCREEN_HEIGHT);
  graph = new scene_graph();
  
  uniform_store = new shader_uniform_storage();
}

void modules::free() {
  delete view;
  delete framebuffer;
  delete uniform_store;
  delete programs;
  delete textures;
  delete geom;
  delete models;
  delete graph;
  delete vertex_buffer;
}

modules g_m{};

module_textures::index_type g_checkerboard_cubemap {module_textures::k_uninit};

static bool g_framemodelmap = true;
static bool g_reflect = true;

static bool g_unif_gamma_correct = true;

GLuint g_vao = 0;

static move_state  g_cam_move_state = {
    0, 0, 0, 0, 0, 0, 0
};

static move_state  g_select_move_state = {
    0, 0, 0, 0, 0, 0, 0
};

struct frame_model {    
    framebuffer_ops::index_type render_cube_id{framebuffer_ops::k_uninit};
    bool needs_render{true};
};

struct object_manip {
  using index_type = scene_graph::index_type;

  static const inline index_type k_unset = unset<index_type>();

  vec3_t entity_select_reset_pos{R(0)};
  index_type entity_selected{k_unset};

  void clear_select_model_state() {
    if (entity_selected != k_unset) {
      // TODO: cleanup select state for previous model here
    }

    entity_selected = k_unset;
  }
    
  void set_select_model_state(scene_graph::index_type entity) {
    clear_select_model_state();

    entity_selected = entity;
    entity_select_reset_pos = g_m.graph->positions[entity];
  }

  bool has_select_model_state() const {
    return entity_selected != k_unset;
  }

#define MAP_UPDATE_SELECT_MODEL_STATE(dir, axis, amount)	\
  if (g_select_move_state.dir) { update.axis += amount;  }

  // This is continuously called in the main loop,
  // so it's important that we don't assume
  // that this function is alaways called when
  // a valid index hits.
  void update_select_model_state() {
    ASSERT(has_select_model_state());

    vec3_t update{R(0.0)};
    auto OBJECT_SELECT_MOVE_STEP = g_m.graph->bound_volumes[entity_selected].radius;

    MAP_UPDATE_SELECT_MODEL_STATE(front, z, -OBJECT_SELECT_MOVE_STEP);
    MAP_UPDATE_SELECT_MODEL_STATE(back, z, OBJECT_SELECT_MOVE_STEP);

    MAP_UPDATE_SELECT_MODEL_STATE(right, x, OBJECT_SELECT_MOVE_STEP);
    MAP_UPDATE_SELECT_MODEL_STATE(left, x, -OBJECT_SELECT_MOVE_STEP);

    MAP_UPDATE_SELECT_MODEL_STATE(up, y, OBJECT_SELECT_MOVE_STEP);
    MAP_UPDATE_SELECT_MODEL_STATE(down, y, -OBJECT_SELECT_MOVE_STEP);

    move(entity_selected, update, mop_add);
  }

  void reset_select_model_state() {
    if (has_select_model_state()) {
      move(entity_selected, entity_select_reset_pos, mop_set);
    }
  }
    
#undef MAP_UPDATE_SELECT_MODEL_STATE

  enum _move_op {
    mop_add,
    mop_sub,
    mop_set
  };
    
  void move(index_type entity, const vec3_t& position, _move_op mop) {
    switch (mop) {
    case mop_add: g_m.graph->positions[entity] += position; break;
    case mop_sub: g_m.graph->positions[entity] -= position; break;
    case mop_set: g_m.graph->positions[entity] = position; break;
    }

    g_m.graph->bound_volumes[entity].center = g_m.graph->positions[entity];
  }

  // Place model 'a' ontop of model 'b'.
  // Right now we only care about bounding spheres
  void place_above(index_type a, index_type b) {
    ASSERT(g_m.graph->bound_volumes[a].type == module_geom::bvol::type_sphere);
    ASSERT(g_m.graph->bound_volumes[b].type == module_geom::bvol::type_sphere);

    vec3_t a_position{g_m.graph->positions[b]};

    auto arad = g_m.graph->bound_volumes[a].radius;
    auto brad = g_m.graph->bound_volumes[b].radius;

    a_position.y += arad + brad;

    move(a, a_position, mop_set);
  }
};

static std::unique_ptr<object_manip> g_obj_manip{new object_manip()};

darray<pass_info> g_render_passes{};

std::unordered_map<module_models::index_type, frame_model> g_frame_model_map{};

struct gl_depth_func {
    GLint prev_depth;

    gl_depth_func(GLenum func) {
        GL_FN(glGetIntegerv(GL_DEPTH_FUNC, &prev_depth));
        GL_FN(glDepthFunc(func));
    }

    ~gl_depth_func() {
        GL_FN(glDepthFunc(prev_depth));
    }
};

struct gl_clear_depth {
    real_t prev_depth;

    gl_clear_depth(real_t new_depth) {
        GL_FN(glGetFloatv(GL_DEPTH_CLEAR_VALUE, &prev_depth));
        GL_FN(glClearDepth(new_depth));
    }

    ~gl_clear_depth() {
        GL_FN(glClearDepth(prev_depth));
    }
};

#define DUNIFINT(name, value) \
  duniform(static_cast<int32_t>(value), #name)

#define DUNIFMAT4X4_R(name, value) \
  duniform(mat4_t(R(value)), #name)

#define DUNIFVEC3_XYZ(name, x, y, z) \
  duniform(vec3_t(R(x), R(y), R(z)), #name)

#define POINTLIGHT_POSITION R3v(5, 5, 0)

#define POINTLIGHT_EMIT_COLOR R3v(1.0, 1.0, 1.0)
#define POINTLIGHT_MODEL_COLOR R4v(1, 0, 0, 1)
#define POINTLIGHT_MODEL_RADIUS 0.3
#define POINTLIGHT_MODEL_SIZE R3(POINTLIGHT_MODEL_RADIUS)

static const dpointlight g_pointlight{
  POINTLIGHT_POSITION,
  POINTLIGHT_EMIT_COLOR
};

module_models::index_type g_pointlight_model_index{-1};

void shader_pointlight_update() {
  g_m.uniform_store->set_uniform("unif_CameraPosition", g_m.view->position);
}

void add_pointlights(pass_info& p) {
  if (p.name == "floor" || p.name == "room" || p.name == "envmap") {
    p.add_pointlight(g_pointlight, 0);
    p.add_vec3("unif_CameraPosition", vec3_t{});
  }

  if (p.name == "floor") {
    p.add_material("unif_Material", { 1.0 });
  }

  if (p.name == "room") {
    p.add_material("unif_Material", { 1.0 });
  }

  if (p.name == "envmap") {
    p.add_material("unif_Material", { 1.0 });
  }
}

static void init_render_passes() {
  
  // environment map pass
  {
    gl_state state{};
    state.clear_buffers.depth = true;
    state.clear_buffers.color = true;
    state.face_cull.enabled = false;
    state.draw_buffers.fbo = true;
    
    darray<duniform> unifs;
		 
    unifs.push_back(DUNIFINT(unif_TexCubeMap, 0));
    unifs.push_back(DUNIFMAT4X4_R(unif_ModelView, 1.0));
    unifs.push_back(DUNIFMAT4X4_R(unif_Projection, 1.0));

    darray<bind_texture> tex_bindings = {
      { g_checkerboard_cubemap, 0 }
    };

    write_logf("envmap %s", tex_bindings[0].to_string().c_str());
    
    auto ft = pass_info::frame_envmap;

    auto shader = g_m.programs->skybox;

    auto init = []() {
      g_m.framebuffer->rcube->faces[0] = g_m.framebuffer->rcube->calc_look_at_mats(TEST_SPHERE_POS, TEST_SPHERE_RADIUS);
      g_frame_model_map[g_m.models->modind_sphere].needs_render = true;
      shader_pointlight_update();
    };

    auto select = scene_graph_select(n, n != g_m.graph->test_indices.sphere);

    auto fbo_id = g_frame_model_map[g_m.models->modind_sphere].render_cube_id;
    
    auto active = true;
    
    pass_info envmap{
      "envmap",
      state,
      unifs,
      tex_bindings,
      ft,
      shader,
      init,
      select,
      fbo_id,
      active
    };

    add_pointlights(envmap);
    g_render_passes.push_back(envmap);
  }

  // wall render pass
  {
    gl_state state{};

    state.clear_buffers.depth = true;
    state.clear_buffers.color = true;
    
    darray<duniform> unifs;

    //unifs.push_back(DUNIFINT(unif_GammaCorrect, true));
    unifs.push_back(DUNIFMAT4X4_R(unif_ModelView, 1.0));
    unifs.push_back(DUNIFMAT4X4_R(unif_Projection, 1.0));
    unifs.push_back(DUNIFMAT4X4_R(unif_Model, 1.0));

    auto ft = pass_info::frame_user;

    auto shader = g_m.programs->default_fb;

    auto init = []() {
      shader_pointlight_update();
    };

    auto select = scene_graph_select(n,
                                     g_m.models->type(g_m.graph->model_indices[n]) ==
                                     module_models::model_quad);
    
    auto fbo_id = framebuffer_ops::k_uninit;
    
    auto active = true;
    
    pass_info main{
      "floor",
      state,
      unifs,
      {}, // textures
      ft,
      shader,
      init,
      select,
      fbo_id,
      active
    };
    
    add_pointlights(main);

    g_render_passes.push_back(main);
  }

  // reflect pass
  {
    gl_state state{};

    //state.clear_buffers.color = true;
    //state.clear_buffers.depth = true;
    
    darray<duniform> unifs;
		 
    unifs.push_back(DUNIFINT(unif_TexCubeMap, 0));
    unifs.push_back(DUNIFMAT4X4_R(unif_ModelView, 1.0));
    unifs.push_back(DUNIFMAT4X4_R(unif_Projection, 1.0));
    unifs.push_back(DUNIFVEC3_XYZ(unif_CameraPosition, 0.0, 0.0, 0.0));

    darray<bind_texture> tex_bindings = {
      {
        g_m.framebuffer->render_cube_color_tex(
          g_frame_model_map[g_m.models->modind_sphere].render_cube_id),
	      0
      }
    };

    auto ft = pass_info::frame_user;

    auto shader = g_m.programs->sphere_cubemap;

    auto init = []() {
      // NOTE: unif_CameraPosition is also used for specular reflection - 
      // this unif_CameraPosition does NOT correspond to that.
      g_m.uniform_store ->set_uniform("unif_CameraPosition", g_m.view->position);
    };
    
    auto select = scene_graph_select(n,
				     n == g_m.graph->test_indices.sphere);

    auto fbo_id = framebuffer_ops::k_uninit;
    auto active = true;
    
    pass_info reflect{
      "reflect",
      state,
      unifs,
      tex_bindings,
      ft,
      shader,
      init,
      select,
      fbo_id,
      active
    };

    add_pointlights(reflect);

    g_render_passes.push_back(reflect);
  }

  // room pass
  {
    gl_state state{};
    
    darray<duniform> unifs;
		 
    unifs.push_back(DUNIFINT(unif_TexCubeMap, 0));
    
    unifs.push_back(DUNIFMAT4X4_R(unif_ModelView, 1.0));
    unifs.push_back(DUNIFMAT4X4_R(unif_Projection, 1.0));

    {
      mat4_t model{g_m.graph->model_transform(g_m.graph->test_indices.area_sphere)};
      unifs.push_back(duniform(model,
                               "unif_Model"));
    }

    darray<bind_texture> tex_bindings = {
      { g_checkerboard_cubemap, 0 }
    };
    
    auto ft = pass_info::frame_user;

    auto shader = g_m.programs->skybox;

    auto init = []() {
      shader_pointlight_update();
    };
    
    auto select = scene_graph_select(n, n == g_m.graph->test_indices.area_sphere);

    auto fbo_id = framebuffer_ops::k_uninit;
    
    auto active = true;
    
    pass_info room{
      "room",
      state,
      unifs,
      tex_bindings,
      ft,
      shader,
      init,
      select,
      fbo_id,
      active
    };

    add_pointlights(room);

    g_render_passes.push_back(room);
  }

  {
    gl_state state{};
    
    darray<duniform> unifs;
		     
    unifs.push_back(DUNIFMAT4X4_R(unif_ModelView, 1.0));
    unifs.push_back(DUNIFMAT4X4_R(unif_Projection, 1.0));

    darray<bind_texture> tex_bindings{};
    auto frametype = pass_info::frame_user;

    auto select = scene_graph_select(n, n == g_m.graph->test_indices.pointlight);
    auto shader = g_m.programs->basic;

    auto init = []() {};
    
    auto fbo_id = framebuffer_ops::k_uninit;
    auto active = true;

    pass_info light_model{
      "light_model",
      state,
      unifs,
      tex_bindings,
      frametype,
      shader,
      init,
      select,
      fbo_id,
      active
    };

    g_render_passes.push_back(light_model);
  }

  // mouse pick
  {
    gl_state state{};
    state.gamma.framebuffer_srgb = false;

    state.draw_buffers.fbo = true;

    state.clear_buffers.color = true;
    state.clear_buffers.color_value = R4(0);
    state.clear_buffers.color_value.a = R(1);

    state.clear_buffers.depth_value = R(1);
    state.clear_buffers.depth = true;
    
    darray<duniform> unifs;
		     
    unifs.push_back(DUNIFMAT4X4_R(unif_ModelView, 1.0));
    unifs.push_back(DUNIFMAT4X4_R(unif_Projection, 1.0));
    unifs.push_back(duniform{R4(1.0f), "unif_Color"});

    darray<bind_texture> tex_bindings{};
    auto frametype = pass_info::frame_texture2d;

    auto select = scene_graph_select(n, g_m.graph->pickable[n] == true);
    auto shader = g_m.programs->mousepick;

    auto init = []() {};
    auto fbo_id = g_m.graph->pickfbo;
    
    auto active = true;

    auto permodel_set = [](const scene_graph::index_type& id) {
      if (g_m.graph->pickable[id]) {
        g_m.uniform_store->set_uniform("unif_Color", g_m.graph->pickmap[id]);
        g_m.uniform_store->upload_uniform("unif_Color");
      }
    };

    pass_info mousepick{
      "mousepick",
      state,
      unifs,
      tex_bindings,
      frametype,
      shader,
      init,
      select,
      fbo_id,
      active,
      permodel_set
    };

    g_render_passes.push_back(mousepick);
  }
};

static void init_api_data() {
    g_m.view->reset_proj();

    g_m.programs->load();

    GL_FN(glGenVertexArrays(1, &g_vao));
    GL_FN(glBindVertexArray(g_vao));

    g_m.models->modind_sphere = g_m.models->new_sphere( );
    g_m.models->modind_area_sphere = g_m.models->new_sphere( );
    
    frame_model fmod{};
    fmod.render_cube_id = g_m.framebuffer->add_render_cube(TEST_SPHERE_POS,
                                                  TEST_SPHERE_RADIUS);
    
    g_frame_model_map[g_m.models->modind_sphere] = fmod;
    
    g_checkerboard_cubemap = 
      g_m.textures->new_texture(g_m.textures->cubemap_params( 256, 
                                                              256, 
                                                              module_textures::cubemap_preset_test_room_0));
    
    real_t wall_size = R(15.0);
    
    g_m.vertex_buffer->reset();

    {
      scene_graph::init_info sphere;

      sphere.position = vec3_t{ROOM_SPHERE_POS};
      sphere.scale = vec3_t{ROOM_SPHERE_RADIUS};
      sphere.angle = vec3_t{0};

      sphere.model = g_m.models->modind_area_sphere;
      sphere.parent = 0;
      sphere.bvol = g_m.geom->make_bsphere(ROOM_SPHERE_RADIUS, ROOM_SPHERE_POS);

      g_m.graph->test_indices.area_sphere = g_m.graph->new_node(sphere);
    }
    
    {
      scene_graph::init_info sphere;

      sphere.position = vec3_t{TEST_SPHERE_POS};
      sphere.scale = vec3_t{TEST_SPHERE_RADIUS};
      sphere.angle = vec3_t{0};

      sphere.model = g_m.models->modind_sphere;
      sphere.parent = g_m.graph->test_indices.area_sphere;
      sphere.pickable = true;
      sphere.bvol = g_m.geom->make_bsphere(TEST_SPHERE_RADIUS, TEST_SPHERE_POS);

      g_m.graph->test_indices.sphere = g_m.graph->new_node(sphere);
    }

    {
      scene_graph::init_info pointlight_model;

      pointlight_model.position = POINTLIGHT_POSITION;
      pointlight_model.scale = POINTLIGHT_MODEL_SIZE;
      pointlight_model.angle = R3(0);

      pointlight_model.model = g_pointlight_model_index = g_m.models->new_sphere(POINTLIGHT_MODEL_COLOR);
      pointlight_model.parent = g_m.graph->test_indices.area_sphere;
      pointlight_model.pickable = true;
      pointlight_model.bvol = g_m.geom->make_bsphere(1.0, POINTLIGHT_POSITION);

      g_m.graph->test_indices.pointlight = g_m.graph->new_node(pointlight_model);
    }
    
    {
      scene_graph::init_info floor;

      floor.position = R3v(0, -5, 0);
      floor.scale = R3v(20.0, 1.0, 20.0);
      floor.angle = vec3_t{0};

      floor.model = g_m.models->new_wall(module_models::wall_bottom, R4v(0.0, 0.0, 0.5, 1.0));
      
      floor.parent = g_m.graph->test_indices.area_sphere;
      
      g_m.graph->test_indices.floor = g_m.graph->new_node(floor);
    }
    
    GL_FN(glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT));
    GL_FN(glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS));
    GL_FN(glDisable(GL_CULL_FACE));
    //    GL_FN(glCullFace(GL_BACK));
    //    GL_FN(glFrontFace(GL_CCW));
    GL_FN(glEnable(GL_DEPTH_TEST));
    GL_FN(glDepthFunc(GL_LEQUAL));
    GL_FN(glClearDepth(1.0f));
}

static darray<uint8_t> g_debug_cubemap_buf;
static module_textures::index_type g_debug_cm_index{module_textures::k_uninit};

#define screen_cube_depth(k) (g_m.framebuffer->width * g_m.framebuffer->height * 4 * (k))

static int screen_cube_index = 0;

static void render() {
  vec4_t background(0.0f, 0.5f, 0.3f, 1.0f);

  SET_CLEAR_COLOR_V4(background);

  for (auto& pass: g_render_passes) {
    pass.apply();
  }
}

static void error_callback(int error, const char* description) {
  fputs(description, stdout);
}

// origin for coordinates is the top left
// of the window
struct camera_orientation {
    double prev_xpos;
    double prev_ypos;

    double dx;
    double dy;

    double sdx;
    double sdy;

    bool active;
} static g_cam_orient = {
    0.0, 
    0.0,
    
    0.0, 
    0.0,
    
    0.0,
    0.0,
    
    false
};

// approximately 400 virtual key identifiers
// defined by GLFW
static std::array<bool, 400> g_key_states;

void maybe_enable_cursor(GLFWwindow* w) {
    if (g_cam_orient.active) {
        glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else {
        glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

// we only check once here since
// a keydown can trigger an action,
// and most of the time we don't want
// that action to repeat for
// the entire duration of the key press
bool keydown_if_not(int key) {
    bool isnotdown = !g_key_states[key];

    if (isnotdown) {
        g_key_states[key] = true;
    }

    return isnotdown;
}

void toggle_framebuffer_srgb() {
    if (g_unif_gamma_correct) {
        GL_FN(glEnable(GL_FRAMEBUFFER_SRGB));
    } else {
        GL_FN(glDisable(GL_FRAMEBUFFER_SRGB));
    }
}

void clear_model_selection();

// this callback is a slew of macros to make changes and adaptations easier
// to materialize: much of this is likely to be altered as new needs are met,
// and there are many situations that call for redundant expressions that may
// as well be abstracted away
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
#define MAP_MOVE_STATE_TRUE(key, dir) case key: g_cam_move_state.dir = true; break
#define MAP_MOVE_STATE_FALSE(key, dir) case key: g_cam_move_state.dir = false; break

#define MAP_MOVE_SELECT_STATE_TRUE(key, dir)    \
    case key: {                                 \
        if (g_obj_manip->has_select_model_state()){ \
            g_select_move_state.dir = true;     \
        }                                       \
    } break

#define MAP_MOVE_SELECT_STATE_FALSE(key, dir) case key: g_select_move_state.dir = false; break
    
#define KEY_BLOCK(key, expr)                    \
  case key: {                                   \
      if (keydown_if_not(key)) {                \
          expr;                                 \
      }                                         \
  } break

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;

            KEY_BLOCK(GLFW_KEY_U,
                      screen_cube_index = (screen_cube_index + 1) % 6);

            KEY_BLOCK(GLFW_KEY_I,
                      screen_cube_index = screen_cube_index == 0 ? 5 : screen_cube_index - 1);
            
            KEY_BLOCK(GLFW_KEY_F1,
                      g_cam_orient.active = !g_cam_orient.active;
                      if (g_cam_orient.active) {
                          clear_model_selection();
                      }
                      maybe_enable_cursor(window));

            KEY_BLOCK(GLFW_KEY_N,
                      g_m.models->new_sphere());

            KEY_BLOCK(GLFW_KEY_F2,
                      g_m.framebuffer->screenshot());

            KEY_BLOCK(GLFW_KEY_G,
                      g_unif_gamma_correct = !g_unif_gamma_correct;
                      toggle_framebuffer_srgb());

            KEY_BLOCK(GLFW_KEY_R,
                      g_obj_manip->reset_select_model_state());

	    KEY_BLOCK(GLFW_KEY_M,
		      g_reflect = !g_reflect);

	    KEY_BLOCK(GLFW_KEY_T,
		      g_framemodelmap = !g_framemodelmap);
            
            MAP_MOVE_STATE_TRUE(GLFW_KEY_W, front);
            MAP_MOVE_STATE_TRUE(GLFW_KEY_S, back);
            MAP_MOVE_STATE_TRUE(GLFW_KEY_A, left);
            MAP_MOVE_STATE_TRUE(GLFW_KEY_D, right);
            MAP_MOVE_STATE_TRUE(GLFW_KEY_SPACE, up);
            MAP_MOVE_STATE_TRUE(GLFW_KEY_LEFT_SHIFT, down);

            // These select movement key binds will eventually
            // be replaced with a more user friendly movement scheme involving
            // mouse drag selection. For  now, the goal is simply to
            // have a driver that allows for ease of testing.
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_UP, front);
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_DOWN, back);
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_RIGHT, right);
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_LEFT, left);
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_RIGHT_SHIFT, up);
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_RIGHT_CONTROL, down);
            
        default:
            keydown_if_not(key);
            break;
        }
    }
    else if (action == GLFW_RELEASE) {
        switch (key) {                        
            
            MAP_MOVE_STATE_FALSE(GLFW_KEY_W, front);
            MAP_MOVE_STATE_FALSE(GLFW_KEY_S, back);
            MAP_MOVE_STATE_FALSE(GLFW_KEY_A, left);
            MAP_MOVE_STATE_FALSE(GLFW_KEY_D, right);
            MAP_MOVE_STATE_FALSE(GLFW_KEY_SPACE, up);
            MAP_MOVE_STATE_FALSE(GLFW_KEY_LEFT_SHIFT, down);

            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_UP, front);
            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_DOWN, back);
            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_RIGHT, right);
            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_LEFT, left);
            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_RIGHT_SHIFT, up);
            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_RIGHT_CONTROL, down);

        default:
            g_key_states[key] = false;          
            break;
        }
    }

#undef MAP_MOVE_STATE_TRUE
#undef MAP_MOVE_STATE_FALSE
}


struct click_state {
    enum click_mode {
        mode_nothing = 0,
        mode_select
    };

    click_mode mode{mode_select};

private:
    static const int32_t DEPTH_SCAN_XY_RANGE = 5;

public:
    
    //
    // cast_ray - used for mouse picking
    //
    // Creates an inverse transform, designed to map coordinates in screen_space back to worldspace. The coordinates
    // being mapped are the current coordinates of the mouse cursor.
    //
    // A raycast is used to determine whether or not an intersection has occurred
    //
    // See https://www.glfw.org/docs/latest/input_guide.html#cursor_pos:
    // The documentation states that the screen coordinates are relative to the upper left
    // of the window's content area (not the entire desktop/viewing area). So, we don't have to perform any additional calculations
    // on the mouse coordinates themselves.

    struct  {
        mat4_t plane{R(1.0)};
        vec3_t normal;
        vec3_t point;
        real_t d;
        bool calc{true};
    } mutable select;
    
    vec3_t screen_out() const {
        int32_t x_offset = 0;
        int32_t y_offset = 0;
        
        real_t depth{};

        // This will read from GL_BACK by default
        GL_FN(glReadPixels(static_cast<int32_t>(g_cam_orient.prev_xpos),
                           static_cast<int32_t>(g_cam_orient.prev_ypos),
                           1, 1,
                           GL_DEPTH_COMPONENT, OPENGL_REAL,
                           &depth));
        
        vec4_t mouse( R(g_cam_orient.prev_xpos) + R(x_offset), 
                      R(g_cam_orient.prev_ypos) + R(y_offset),
                      -depth,
                      R(1.0));

        // TODO: cache screen_sp and clip_to_ndc, since they're essentially static data
        mat4_t screen_sp{R(1.0)};
        screen_sp[0][0] = R(g_m.view->view_width);
        screen_sp[1][1] = R(g_m.view->view_height);

        // OpenGL viewport origin is lower left, GLFW's is upper left.
        mouse.y = screen_sp[1][1] - mouse.y;

        mouse.x /= screen_sp[0][0];
        mouse.y /= screen_sp[1][1];

          // In screen space the range is is [0, width] and [0, height],
        // so after normalization we're in [0, 1].
        // NDC is in [-1, 1], so we scale from [0, 1] to [-1, 1],
        // to complete the NDC transform
        mouse.x = R(2.0) * mouse.x - R(1.0);
        mouse.y = R(2.0) * mouse.y - R(1.0);

        std::cout << AS_STRING_SS(depth) SEP_SS AS_STRING_SS(x_offset) SEP_SS AS_STRING_SS(y_offset)
                  << std::endl;

        return glm::inverse(g_m.view->proj * g_m.view->view()) * mouse;
    }
    
    bool cast_ray(scene_graph::index_type entity) const {
        vec3_t nearp = screen_out();
        module_geom::ray world_raycast{}; 
        world_raycast.dir = glm::normalize(nearp - g_m.view->position);
        world_raycast.orig = g_m.view->position;

        auto bvol = g_m.graph->bound_volumes[entity];

        bool success = g_m.geom->test_ray_sphere(world_raycast, bvol);
        
        if (success) {
            std::cout << "HIT\n";
            g_obj_manip->set_select_model_state(entity);
        } else {
            std::cout << "NO HIT\n";
            clear_model_selection();
        }
        #if 0
        std::cout << AS_STRING_GLM_SS(nearp) << "\n";
        std::cout << AS_STRING_GLM_SS(world_raycast.orig) << "\n";
        std::cout << AS_STRING_GLM_SS(world_raycast.dir) << "\n";

        std::cout << std::endl;
        #endif

        return success;
    }

    auto coeff(real_t d) const {
        real_t base = R(250);
        real_t logb_d = glm::log(d) / glm::log(base);
        return R(1); //std::min(logb_d, R(1.0));
    }
    
    auto calc_new_selected_position() const {
        if (select.calc) {
            //ASSERT(false);
            
            vec3_t Po{g_m.graph->positions[g_obj_manip->entity_selected]}; 

            vec3_t Fo{Po - g_m.view->position}; // negated z-axis of transform defined by the plane of interest

            select.normal = -glm::normalize(Fo);
            select.d = glm::abs(glm::dot(select.normal, Po));
            select.point = Po;
            select.calc = false;
        }
        
        vec3_t s2w{screen_out()};
        s2w.x *= 10.0;
        s2w.y *= 2.0;
        //s2w.y *= 10.0;
        std::cout << AS_STRING_GLM_SS(s2w) << std::endl;

        //s2w.x += g_cam_orient.sdx * 3.0;
        //s2w.y += g_cam_orient.sdy * 3.0;

        vec3_t new_pos{g_m.geom->proj_point_plane(s2w, select.normal, select.point)};

        return new_pos;
    }

    void scan_object_selection() const {
        auto filtered = 
          g_m.graph->select(scene_graph_select(entity, 
                                             g_m.models->type(g_m.graph->model_indices[entity]) == 
                                              module_models::model_sphere && 
                                              g_m.graph->pickable[entity] == true));
        
        for (auto id: filtered) {
            if (cast_ray(id)) {
                break;
            }
        }

        DEBUGLINE;
    }

    void unselect() {
        select.calc = true;
        select.plane = mat4_t{R(1.0)};
    }
} g_click_state;

void clear_model_selection() {
    g_obj_manip->clear_select_model_state();
    DEBUGLINE;
    g_click_state.unselect();
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    auto dxdy = [xpos, ypos](double scale) {
        g_cam_orient.dx = xpos - g_cam_orient.prev_xpos;
        g_cam_orient.dy = ypos - g_cam_orient.prev_ypos; //g_cam_orient.prev_ypos - ypos;

        g_cam_orient.dx *= scale;
        g_cam_orient.dy *= scale;

        g_cam_orient.sdx = (g_cam_orient.dx > 0.0 
                                            ? 1.0 
                                            : (g_cam_orient.dx == 0.0 
                                                               ? 0.0 
                                                               : -1.0));

        g_cam_orient.sdy = (g_cam_orient.dy < 0.0 
                                            ? 1.0 
                                            : (g_cam_orient.dy == 0.0 
                                                               ? 0.0 
                                                               : -1.0));
    };

    if (g_cam_orient.active) {
        dxdy(0.01);
        g_cam_orient.prev_xpos = xpos;
        g_cam_orient.prev_ypos = ypos;

        mat4_t xRot = glm::rotate(mat4_t(1.0f),
                                  static_cast<real_t>(g_cam_orient.dy),
                                  vec3_t(1.0f, 0.0f, 0.0f));

        mat4_t yRot = glm::rotate(mat4_t(1.0f),
                                  static_cast<real_t>(g_cam_orient.dx),
                                  vec3_t(0.0f, 1.0f, 0.0f));

        g_m.view->orient = mat3_t(yRot * xRot) * g_m.view->orient;
    } else {
        dxdy(1.0);
        g_cam_orient.prev_xpos = xpos;
        g_cam_orient.prev_ypos = ypos;
     
        if (g_obj_manip->has_select_model_state()) {
            g_obj_manip->move(g_obj_manip->entity_selected,
                              g_click_state.calc_new_selected_position(),
                              object_manip::mop_set);
        }
      DEBUGLINE;
    } 
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mmods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        switch (g_click_state.mode) {
        case click_state::mode_select: {
            if (!g_cam_orient.active) {
                g_click_state.scan_object_selection();
            }
        }break;
        }
    }
}

int main(void) {
    g_key_states.fill(false);

    GLFWwindow* window;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        exit(EXIT_FAILURE);

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

    g_m.init();

    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    maybe_enable_cursor(window);
    
    init_api_data();
    init_render_passes();

    GL_FN(glEnable(GL_FRAMEBUFFER_SRGB));
    
    //    toggle_framebuffer_srgb();
    
    while (!glfwWindowShouldClose(window)) {
        (*g_m.view)(g_cam_move_state);

        if (g_obj_manip->has_select_model_state()) {
            g_obj_manip->update_select_model_state();
        }

        render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_m.free();

    glfwDestroyWindow(window);
    
    glfwTerminate();
    exit(EXIT_SUCCESS);

error:
    glfwTerminate();
    exit(EXIT_FAILURE);
}