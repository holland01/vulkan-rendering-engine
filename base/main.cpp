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
  geom = new module_geom();
}

void modules::free() {
  safe_del(framebuffer);
  safe_del(programs);
  safe_del(textures);
  safe_del(geom);
}

modules g_m{};

module_textures::index_type g_checkerboard_cubemap {module_textures::k_uninit};

static bool g_framemodelmap = true;
static bool g_reflect = true;

static bool g_unif_gamma_correct = true;

GLuint g_vao = 0;
//
//

struct move_state {
    uint8_t up : 1;
    uint8_t down : 1;
    uint8_t right : 1;
    uint8_t left : 1;
    uint8_t front : 1;
    uint8_t back : 1;
    uint8_t __rem : 2;
};

static move_state  g_cam_move_state = {
    0, 0, 0, 0, 0, 0, 0
};

static move_state  g_select_move_state = {
    0, 0, 0, 0, 0, 0, 0
};


#define DRAW_MODELS(transform_order) g_models.render(transform_order)

// key facts about transformations:
// - a matrix transform represents the source space from the perspective
// of the destination space

struct view_data {
    mat4_t proj, skyproj, cubeproj, ortho;    // proj: camera to clip space transform

    mat4_t view_mat;

    mat3_t orient;  // orient: camera orientation

    vec3_t position;  // position: theoretical position of the camera: technically,
                         // view space is always centered at a specific location that's
                         // k units of distance before the near plane. Thus, it's always
                         // looking down -z. In other words, the viewing volume (frustum)
                         // is always in the same space, and the camera facing its near plane is also
                         // always at the same space. The "position" vector we have here
                         // is used to provide the illusion of a moving camera. More on this later.

    real_t step;          // step: move step amount

    real_t skynearp;
    real_t nearp;
    real_t skyfarp;
    real_t farp;

    uint16_t view_width;
    uint16_t view_height;

    bool view_bound;
    
    view_data(uint16_t width, uint16_t height)
        : proj(1.0f), skyproj(R(1.0)), cubeproj(R(1.0)), ortho(R(1.0)),
          view_mat(R(1.0)),
          orient(1.0f),
          position(0.0f),
          step(0.1f),
          skynearp(10.0f),
          nearp(1.0f),
          skyfarp(1000.0f),
          farp(1000.0f),
          view_width(width),
          view_height(height),
          view_bound(false)
    {
    }

    auto calc_aspect() const {
        return static_cast<real_t>(view_width) / static_cast<real_t>(view_height);
    }

    void reset_proj() {
        set_proj_from_fovy(45.0f);
        skyproj = glm::perspective(45.0f, calc_aspect(), skynearp, skyfarp);
        cubeproj = glm::perspective(45.0f, calc_aspect(), nearp, farp);

        real_t w = view_width * 0.5;
        real_t h = view_height * 0.5;

        ortho = glm::orthoRH(-w, w, -h, h, nearp, farp);
    }

    void set_proj_from_fovy(real_t fovy) {
        proj = glm::perspective(fovy, calc_aspect(), nearp, farp);
    }

    //
    // view_<dir>(): series of functions used for "moving" the camera in a particular direction.
    //
    // Pick a translation direction, and RET_VIEW_DIR will produce a transformed vector offset
    // such that any object's within the viewing volume will be oriented/moved
    // to provide the illusion that the viewer is moving in the requested direction.
    //
#define RET_VIEW_DIR(x, y, z) vec3_t dir(glm::inverse(orient) * vec3_t(x, y, z)); return dir

    vec3_t view_up() const { RET_VIEW_DIR(0.0f, 1.0f, 0.0f); }
    vec3_t view_down() const { RET_VIEW_DIR(0.0f, -1.0f, 0.0f); }

    vec3_t view_right() const { RET_VIEW_DIR(1.0f, 0.0f, 0.0f); }
    vec3_t view_left() const { RET_VIEW_DIR(-1.0f, 0.0f, 0.0f); }

    vec3_t view_front() const { RET_VIEW_DIR(0.0f, 0.0f, -1.0f); }
    vec3_t view_back() const { RET_VIEW_DIR(0.0f, 0.0f, 1.0f); }

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
    mat4_t view() const {
        if (view_bound) {
            return view_mat;
        } else {
            return mat4_t(orient) * glm::translate(mat4_t(1.0f), -position);
        }
    }

    void bind_view(const mat4_t& view) {
        view_mat = view;
        view_bound = true;
    }

    void unbind_view() {
        view_bound = false;
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
  
  void operator ()(move_state m) {
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
  
  darray<vertex> data;

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

  auto add_triangle(const vec3_t& a_position, const vec4_t& a_color, const vec3_t& a_normal, const vec2_t& a_uv,
		    const vec3_t& b_position, const vec4_t& b_color,  const vec3_t& b_normal, const vec2_t& b_uv,
		    const vec3_t& c_position, const vec4_t& c_color, const vec3_t& c_normal, const vec2_t& c_uv) {
        vertex a = {
            a_position,
            a_color,
	    a_normal,
            a_uv
        };

        vertex b = {
            b_position,
            b_color,
	    b_normal,
            b_uv
        };

        vertex c = {
            c_position,
            c_color,
	    c_normal,
            c_uv
        };

        auto offset = num_vertices();

        data.push_back(a);
        data.push_back(b);
        data.push_back(c);

        return offset;

    }

    auto add_triangle(const vec3_t& a_position, const vec4_t& a_color,
                      const vec3_t& b_position, const vec4_t& b_color,
                      const vec3_t& c_position, const vec4_t& c_color) {
        vec2_t defaultuv(glm::zero<vec2_t>());
	
        return add_triangle(a_position, a_color, a_position, defaultuv,
                            b_position, b_color, b_position, defaultuv,
                            c_position, c_color, c_position, defaultuv);
    }

  auto add_triangle(const vec3_t& a_position, const vec4_t& a_color, const vec3_t& a_normal, 
		    const vec3_t& b_position, const vec4_t& b_color, const vec3_t& b_normal,
		    const vec3_t& c_position, const vec4_t& c_color, const vec3_t& c_normal) {
    vec2_t defaultuv(glm::zero<vec2_t>());
    
    return add_triangle(a_position, a_color, a_normal, defaultuv,
			b_position, b_color, b_normal, defaultuv,
			c_position, c_color, c_normal, defaultuv);
  }


} static g_vertex_buffer;

#define MODLAMSEL(name, return_expr) [](const models::index_type& name) -> bool { return return_expr; }

struct models {

  enum model_type {
    model_unknown = 0,
    model_tri,
    model_sphere,
    model_cube,
    model_quad
  };

  enum wall_type {
    wall_front = 0,
    wall_left,
    wall_right,
    wall_back,
    wall_top,
    wall_bottom
  };

  using index_type = int32_t;
  using transform_fn_type = std::function<mat4_t(index_type model)>;
  using index_list_type = darray<index_type>;
  using predicate_fn_type = std::function<bool(const index_type&)>;
    
  static const inline index_type k_uninit = -1;

  darray<module_geom::bvol> bound_volumes;
  darray<model_type> model_types;
  darray<index_type> vertex_offsets;
  darray<index_type> vertex_counts;
  mutable darray<bool> draw;

  vec3_t model_select_reset_pos{glm::zero<vec3_t>()};
    
  index_type model_count = 0;
    
  index_type modind_tri = k_uninit;
  index_type modind_sphere = k_uninit;
  index_type modind_skybox = k_uninit;
  index_type modind_area_sphere = k_uninit;
  index_type modind_selected = k_uninit;

  mutable bool framebuffer_pinned = false;
    
  // It's assumed that vertices
  // have been already added to the vertex
  // buffer when this function is caglled,
  // so we explicitly reallocate the needed
  // VBO memory every time we add new
  // model data with this function.
  auto new_model(
      model_type mt,
      index_type vbo_offset = 0,
      index_type num_vertices = 0) {

    index_type id = static_cast<index_type>(model_count);

    model_types.push_back(mt);

    vertex_offsets.push_back(vbo_offset);
    vertex_counts.push_back(num_vertices);
    
    model_count++;

    g_vertex_buffer.reset();

    return id;
  }    
    
  auto new_sphere(vec4_t color = vec4_t{R(1.0)}) {
    auto offset = g_vertex_buffer.num_vertices();

    real_t step = 0.05f;

    auto count = 0;

    auto cart = [](real_t phi, real_t theta) {
      vec3_t ret;
      ret.x = glm::cos(theta) * glm::cos(phi);
      ret.y = glm::sin(phi);
      ret.z = glm::sin(theta) * glm::cos(phi);
      return ret;
    };
      	
    for (real_t phi = -glm::half_pi<real_t>(); phi <= glm::half_pi<real_t>(); phi += step) {
      for (real_t theta = 0.0f; theta <= glm::two_pi<real_t>(); theta += step) {
        auto a = cart(phi, theta);
        auto b = cart(phi, theta + step);
        auto c = cart(phi + step, theta + step);
        auto d = cart(phi + step, theta);

        g_vertex_buffer.add_triangle(a, color, a,
                                     d, color, d,
                                     c, color, c);

        g_vertex_buffer.add_triangle(c, color, c,
                                     a, color, a,
                                     b, color, b);
        
	      count += 6;
      }
    }

    return new_model(model_sphere,
		     offset,
		     count);
  }
    
  auto new_wall(
    wall_type type,
    const vec4_t& color = R4(1.0)) {

    std::array<vec3_t, 6> normals = {
      R3v(0, 0, -1), // front
      R3v(-1, 0, 0), // left
      R3v(1, 0, 0),  // right
      R3v(0, 0, 1),  // back
      R3v(0, 1, 0),  // top
      R3v(0, -1, 0)  // bottom
    };

    // TODO:
    // rewrite this so that the API
    // takes planes XY, XZ, and YZ
    // instead of wall types,
    // since the scaling that's used
    // is going to offset the faces
    // in directions (in terms of translation)
    // which we don't want them to be offset
    // in.
    std::array<real_t, 36 * 3> vertices = {
      // positions
      // front
      -1.0f, 1.0f, 0.0f,
      -1.0f, -1.0f, 0.0f,
      1.0f, -1.0f, 0.0f,
      1.0f, -1.0f, 0.0f,
      1.0f, 1.0f, 0.0f,
      -1.0f, 1.0f, 0.0f,

      // left
      0.0f, -1.0f, 1.0f,
      0.0f, -1.0f, -1.0f,
      0.0f, 1.0f, -1.0f,
      0.0f, 1.0f, -1.0f,
      0.0f, 1.0f, 1.0f,
      0.0f, -1.0f, 1.0f,

      // right
      0.0f, -1.0f, -1.0f,
      0.0f, -1.0f, 1.0f,
      0.0f, 1.0f, 1.0f,
      0.0f, 1.0f, 1.0f,
      0.0f, 1.0f, -1.0f,
      0.0f, -1.0f, -1.0f,

      // back
      -1.0f, -1.0f, 0.0f,
      -1.0f, 1.0f, 0.0f,
      1.0f, 1.0f, 0.0f,
      1.0f, 1.0f, 0.0f,
      1.0f, -1.0f, 0.0f,
      -1.0f, -1.0f, 0.0f,

      // top
      -1.0f, 0.0f, -1.0f,
      1.0f, 0.0f, -1.0f,
      1.0f, 0.0f, 1.0f,
      1.0f, 0.0f, 1.0f,
      -1.0f, 0.0f, 1.0f,
      -1.0f, 0.0f, -1.0f,

      // bottom
      -1.0f, 0.0f, -1.0f,
      -1.0f, 0.0f, 1.0f,
      1.0f, 0.0f, -1.0f,
      1.0f, 0.0f, -1.0f,
      -1.0f, 0.0f, 1.0f,
      1.0f, 0.0f, 1.0f
    };

    auto vbo_offset = g_vertex_buffer.num_vertices();
        
    real_t* offset = &vertices[type * 18];

    vec3_t normal = normals[type];

    vec3_t a(offset[0], offset[1], offset[2]);
    vec3_t b(offset[3], offset[4], offset[5]);
    vec3_t c(offset[6], offset[7], offset[8]);

    g_vertex_buffer.add_triangle(a, color, normal, 
				                         b, color, normal,
                                 c, color, normal);

    vec3_t d(offset[9], offset[10], offset[11]);
    vec3_t e(offset[12], offset[13], offset[14]);
    vec3_t f(offset[15], offset[16], offset[17]);

    g_vertex_buffer.add_triangle(d, color, normal,
				                         e, color, normal,
				                         f, color, normal);

#if 0
    module_geom::bvol vol;

    vol.radius = glm::length(scale);
    vol.center = position;
#endif   
    return new_model(model_quad,
                     vbo_offset,
                     6 // vertex count 
                     );
  }

  auto new_cube(const vec4_t& color = vec4_t(1.0f)) {
    std::array<real_t, 36 * 3> vertices = {
      // positions          
      -1.0f, 1.0f, -1.0f,
      -1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,

      -1.0f, -1.0f, 1.0f,
      -1.0f, -1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, 1.0f,
      -1.0f, -1.0f, 1.0f,

      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,

      -1.0f, -1.0f, 1.0f,
      -1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, -1.0f, 1.0f,
      -1.0f, -1.0f, 1.0f,

      -1.0f, 1.0f, -1.0f,
      1.0f, 1.0f, -1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      -1.0f, 1.0f, 1.0f,
      -1.0f, 1.0f, -1.0f,

      -1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f, 1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f, 1.0f,
      1.0f, -1.0f, 1.0f
    };

    auto offset = g_vertex_buffer.num_vertices();

    for (auto i = 0; i < vertices.size(); i += 9) {
      vec3_t a(vertices[i + 0], vertices[i + 1], vertices[i + 2]);
      vec3_t b(vertices[i + 3], vertices[i + 4], vertices[i + 5]);
      vec3_t c(vertices[i + 6], vertices[i + 7], vertices[i + 8]);

      g_vertex_buffer.add_triangle(a, color,
				   b, color,
				   c, color);
    }

    return new_model(model_cube, offset, 36);
  }

  void render(index_type model, const mat4_t& world) const {
    mat4_t mv = g_view.view() * world;

    if (g_m.programs->uniform("unif_Model") != -1) {
      g_m.programs->up_mat4x4("unif_Model", world);
    }
	    
    g_m.programs->up_mat4x4("unif_ModelView", mv);
    g_m.programs->up_mat4x4("unif_Projection",
			  (model == modind_skybox
			   ? g_view.skyproj
			   : (framebuffer_pinned
			      ? g_view.cubeproj
			      : g_view.proj)));
      
    auto ofs = vertex_offsets[model];
    auto count = vertex_counts[model];

    GL_FN(glDrawArrays(GL_TRIANGLES,
		       ofs,
		       count));
  }

  model_type type(index_type i) const {
    return model_types[i];
  }

} static g_models;

struct frame_model {    
    framebuffer_ops::index_type render_cube_id{framebuffer_ops::k_uninit};
    bool needs_render{true};
};

struct node_id;

#define scene_graph_select(n, expr) [](const scene_graph::index_type& n) -> bool { return expr; }

struct scene_graph {
  using index_type = int16_t;

  using predicate_fn_type = std::function<bool(const index_type& n)>;
  
  darray<darray<index_type>> child_lists;
  darray<module_geom::bvol> bound_volumes;
  darray<vec3_t> positions;
  darray<vec3_t> angles;
  darray<vec3_t> scales;
  darray<boolvec3_t> accum; // x -> pos, y -> orient, z -> scale
  darray<node_id> node_ids;
  darray<models::index_type> model_indices;
  darray<index_type> parent_nodes;
  darray<bool> draw;
  darray<bool> pickable; // can be selected by the mouse

  struct test_indices_s {
    index_type sphere{unset<index_type>()};
    index_type skybox{unset<index_type>()};
    index_type area_sphere{unset<index_type>()};
    index_type floor{unset<index_type>()};
  };

  test_indices_s test_indices;
  
  struct init_info {
    module_geom::bvol bvol;
    vec3_t position, angle, scale;
    boolvec3_t accum;
    models::index_type model;
    index_type parent;
    bool draw;
    bool pickable;

    init_info()
      : position(R(0)), angle(R(0)), scale(R(1)),
        accum(true, true, false),
        model(unset<models::index_type>()),
        parent(0),
        draw(true),
        pickable(false)
    {}
  };

  scene_graph();

  index_type new_node(const scene_graph::init_info& info);
  
  bool is_root(index_type node) const { return parent_nodes[node] == unset<index_type>(); }
  
  void make_node_id(index_type node, int depth);

  mat4_t scale(index_type node) const;

  mat4_t translate(index_type node) const;

  mat4_t rotate(index_type node) const;

  mat4_t model_transform(scene_graph::index_type node) const;

  mat4_t modaccum_transform(scene_graph::index_type node) const;
  
  void draw_node(index_type node);

  void draw_node(scene_graph::index_type draw_node,
	    scene_graph::index_type traverse_node,
	    node_id* id,
	    const mat4_t& world);

  void draw_all(index_type current, const mat4_t& world) const;
  
  void draw_all() const;

  int depth(index_type node) const;

  void select_draw(predicate_fn_type func);
  darray<index_type> select(predicate_fn_type func) const;
};

struct node_id {
  using offset_type = scene_graph::index_type;
  using index_type = uint8_t;
  
  darray<offset_type> levels; // levels[0] = child node of graph root
  index_type ptr;
  bool root;
  
  node_id()
    : ptr{0},
      root(true)
  {}
  
  node_id(int depth)
    : levels(depth, unset<offset_type>()),
      ptr{0},
      root(false)
  {}

  bool finished() const {
    ASSERT(levels.size() < std::numeric_limits<index_type>::max());
    return ptr == levels.size();
  }

  offset_type peek() const {
    ASSERT(ptr < levels.size());
    return levels[ptr];
  }
  
  void pop() {
    ASSERT(!finished());
    ptr++;
  }

  void reset() {
    ptr = 0;
  }
};

scene_graph::scene_graph()
  : test_indices()
{
  // root initialization
  bound_volumes.push_back(module_geom::bvol{});
  child_lists.push_back(darray<index_type>());
  positions.push_back(vec3_t{R(0)});
  scales.push_back(vec3_t{R(1)});
  angles.push_back(vec3_t{R(0)});
  accum.push_back(boolvec3_t{false});
  node_ids.push_back(node_id());
  model_indices.push_back(unset<models::index_type>());
  parent_nodes.push_back(unset<index_type>());
  draw.push_back(false);
  pickable.push_back(false);

  #if 0
  test_indices.sphere = unset<index_type>();
  test_indices.area_sphere = unset<index_type>();
  test_indices.skybox = unset<index_type>();
  #endif
}

scene_graph::index_type scene_graph::new_node(const scene_graph::init_info& info) {
  auto index = child_lists.size();

  bound_volumes.push_back(info.bvol);
  child_lists.push_back(darray<index_type>());
  positions.push_back(info.position);
  scales.push_back(info.scale);
  angles.push_back(info.angle);
  accum.push_back(info.accum);
  node_ids.push_back(node_id());
  model_indices.push_back(info.model);
  parent_nodes.push_back(info.parent);
  draw.push_back(info.draw);
  pickable.push_back(info.pickable);

  ASSERT(info.parent != unset<index_type>());
  ASSERT(info.parent < child_lists.size());

  child_lists[info.parent].push_back(index);
  
  make_node_id(index, depth(index));

  return index;
} 

void scene_graph::make_node_id(scene_graph::index_type node, int depth) {
  ASSERT(!is_root(node));
  
  node_id nid(depth);
  int counter = depth - 1;

  auto inode = node;
  
  while (!is_root(inode)) {
    ASSERT(counter >= 0);

    auto parent = parent_nodes[inode]; 
    
    auto offset = 0;
    {
      const auto& children = child_lists[parent];
      while (offset < children.size() && children[offset] != inode) offset++;
      ASSERT(children[offset] == inode);
    }
    nid.levels[counter] = offset;

    inode = parent;
    counter--;
  }

  node_ids[node] = std::move(nid);
}

mat4_t scene_graph::scale(index_type node) const {
  return glm::scale(mat4_t(1.0f), scales.at(node));
}

mat4_t scene_graph::translate(index_type node) const {
  return glm::translate(mat4_t(1.0f), positions.at(node));
}

mat4_t scene_graph::rotate(index_type node) const {
  mat4_t rot(1.0f);

  rot = glm::rotate(mat4_t(1.0f), angles.at(node).x, vec3_t(1.0f, 0.0f, 0.0f));
  rot = glm::rotate(mat4_t(1.0f), angles.at(node).y, vec3_t(0.0f, 1.0f, 0.0f)) * rot;
  rot = glm::rotate(mat4_t(1.0f), angles.at(node).z, vec3_t(0.0f, 0.0f, 1.0f)) * rot;

  return rot;
}

mat4_t scene_graph::model_transform(scene_graph::index_type node) const {
  return translate(node) * rotate(node) * scale(node);
}

mat4_t scene_graph::modaccum_transform(scene_graph::index_type node) const {
  mat4_t m{m4i()};
  
  if (accum[node][0]) m *= translate(node);
  if (accum[node][1]) m *= rotate(node);
  if (accum[node][2]) m *= scale(node);

  return m;
}

void scene_graph::draw_node(scene_graph::index_type node,
			    scene_graph::index_type traverse_node,
			    node_id* id,
			    const mat4_t& world) {  
  if (id->finished()) {
    ASSERT(traverse_node == node);
    
    mat4_t world_accum{world * model_transform(node)};
    
    g_models.render(model_indices[node], world_accum);
    
  } else {
    mat4_t world_accum{world * modaccum_transform(traverse_node)};
    
    auto traverse_next = child_lists[traverse_node][id->peek()];
    id->pop();

    draw_node(node, traverse_next, id, world_accum);
  }
}

void scene_graph::draw_node(scene_graph::index_type node) {
  if (draw[node]) {
    node_id* id = node_ids.data() + node;
    ASSERT(id->ptr == 0);
  
    auto traverse = child_lists[0][id->peek()];
    id->pop();

    draw_node(node, traverse, id, modaccum_transform(0));
  
    id->reset();
  }
}

void scene_graph::draw_all(index_type current, const mat4_t& world) const {
  mat4_t accum{world * modaccum_transform(current)};

  if (draw[current]) {
    mat4_t raccum{world * model_transform(current)};
    g_models.render(model_indices[current], raccum);
  }

  for (auto child: child_lists[current]) {
    draw_all(child, accum);
  }
}
  
void scene_graph::draw_all() const {
  draw_all(0, m4i());
}

int scene_graph::depth(scene_graph::index_type node) const {
  ASSERT(!is_root(node));
  
  int d = 0;
  auto n = node;

  while (!is_root(n)) {
    n = parent_nodes[n];
    d++;
  }

  return d;
}

void scene_graph::select_draw(predicate_fn_type func) {
  for (auto i = 0; i < child_lists.size(); ++i) {
    draw[i] = func(i);
  }
}

darray<scene_graph::index_type> scene_graph::select(predicate_fn_type func) const {
  darray<index_type> ret;
  for (auto i = 0; i < child_lists.size(); ++i) {
    auto e = static_cast<index_type>(i);
    if (func(e)) {
      ret.push_back(e);
    }
  }
  return ret;
}

static std::unique_ptr<scene_graph> g_graph{nullptr};

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
    entity_select_reset_pos = g_graph->positions[entity];
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
    auto OBJECT_SELECT_MOVE_STEP = g_graph->bound_volumes[entity_selected].radius;

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
    case mop_add: g_graph->positions[entity] += position; break;
    case mop_sub: g_graph->positions[entity] -= position; break;
    case mop_set: g_graph->positions[entity] = position; break;
    }

    g_graph->bound_volumes[entity].center = g_graph->positions[entity];
  }

  // Place model 'a' ontop of model 'b'.
  // Right now we only care about bounding spheres
  void place_above(index_type a, index_type b) {
    ASSERT(g_graph->bound_volumes[a].type == module_geom::bvol::type_sphere);
    ASSERT(g_graph->bound_volumes[b].type == module_geom::bvol::type_sphere);

    vec3_t a_position{g_graph->positions[b]};

    auto arad = g_graph->bound_volumes[a].radius;
    auto brad = g_graph->bound_volumes[b].radius;

    a_position.y += arad + brad;

    move(a, a_position, mop_set);
  }
};

static std::unique_ptr<object_manip> g_obj_manip{new object_manip()};

static std::unique_ptr<shader_uniform_storage> g_uniform_storage{new shader_uniform_storage()};

struct pass_info {
  using ptr_type = std::unique_ptr<pass_info>;
  
  enum frame_type {
    frame_user = 0,
    frame_envmap
  };
  
  using draw_fn_type = std::function<void()>;

  std::string name;
  
  gl_state state{};
  
  darray<duniform> uniforms; // cleared after initial upload

  darray<bind_texture> tex_bindings;
  
  frame_type frametype{frame_user};
  
  module_programs::id_type shader;

  std::function<void()> init_fn;

  scene_graph::predicate_fn_type select_draw_predicate; // determines which objects are to be rendered

  framebuffer_ops::index_type envmap_id{framebuffer_ops::k_uninit}; // optional

  bool active{true};
  
  darray<std::string> uniform_names; // no need to set this.

  darray<pass_info> subpasses;
  
  void draw() const {
    g_graph->draw_all();
  }

  void add_pointlight(const dpointlight& pl, int which) {
    ASSERT(which < NUM_LIGHTS);
    std::string name = "unif_Lights[" + std::to_string(which) + "]";
    uniforms.push_back(duniform{pl, name});
  }
  
  void apply() {
    if (active) {
      write_logf("pass: %s", name.c_str());
      
      g_vertex_buffer.bind();
      use_program u(shader);
      
      for (const auto& bind: tex_bindings) {
        g_m.textures->bind(bind.id, bind.slot);
      }
      
      if (!uniforms.empty()) {
        for (const auto& unif: uniforms) {
          switch (unif.type) {
            case shader_uniform_storage::uniform_mat4x4:
              g_uniform_storage->set_uniform(unif.name, unif.m4);
              break;
            case shader_uniform_storage::uniform_pointlight:
              g_uniform_storage->set_uniform(unif.name, unif.pl);
              break;
            case shader_uniform_storage::uniform_vec3:
              g_uniform_storage->set_uniform(unif.name, unif.v3);
              break;
            case shader_uniform_storage::uniform_int32:
              g_uniform_storage->set_uniform(unif.name, unif.i32);
              break;	  
          }

          uniform_names.push_back(unif.name);
        }
      }

	    uniforms.clear();

      init_fn();
      
      for (const auto& name: uniform_names) {
	      g_uniform_storage->upload_uniform(name);
      }

      g_graph->select_draw(select_draw_predicate);
      
      switch (frametype) {
        case frame_user: {
	        GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	        state.apply();
	        draw();
        } break;

        case frame_envmap: {
          ASSERT(envmap_id != framebuffer_ops::k_uninit);
          g_models.framebuffer_pinned = true;
          g_m.framebuffer->rcube->bind(envmap_id);
         
          for (auto i = 0; i < 6; ++i) {
            g_view.bind_view(g_m.framebuffer->rcube->set_face(envmap_id,
                              static_cast<framebuffer_ops::render_cube::axis>(i)));
            state.apply();
            draw();
          }
          g_m.framebuffer->rcube->unbind();
          g_view.unbind_view();
          g_models.framebuffer_pinned = false;
        } break;
      }
      for (const auto& bind: tex_bindings) {
        g_m.textures->unbind(bind.id);
      }

      g_vertex_buffer.unbind();
    }
  }
};

darray<pass_info> g_render_passes{};

std::unordered_map<models::index_type, frame_model> g_frame_model_map{};

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

static const dpointlight g_pointlight{
  R3v(5.0, 5.0, 0),
  R3v(0, 1, 0)
};

void add_pointlights(pass_info& p) {
  if (p.name == "floor" || p.name == "room" || p.name == "envmap") {
    p.add_pointlight(g_pointlight, 0);
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
      g_frame_model_map[g_models.modind_sphere].needs_render = true;
    };

    auto select = scene_graph_select(n, n != g_graph->test_indices.sphere);

    auto envmap_id = g_frame_model_map[g_models.modind_sphere].render_cube_id;
    
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
      envmap_id,
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

    auto init = []() {};

    auto select = scene_graph_select(n,
				     g_models.type(g_graph->model_indices[n]) ==
				     models::model_quad);
    
    auto envmap_id = framebuffer_ops::k_uninit;
    
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
      envmap_id,
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
          g_frame_model_map[g_models.modind_sphere].render_cube_id),
	      0
      }
    };

    auto ft = pass_info::frame_user;

    auto shader = g_m.programs->sphere_cubemap;

    auto init = []() {
      g_uniform_storage->set_uniform("unif_CameraPosition", g_view.position);
    };
    
    auto select = scene_graph_select(n,
				     n == g_graph->test_indices.sphere);

    auto envmap_id = framebuffer_ops::k_uninit;
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
      envmap_id,
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
      mat4_t model{g_graph->model_transform(g_graph->test_indices.area_sphere)};
      unifs.push_back(duniform(model,
                               "unif_Model"));
    }

    darray<bind_texture> tex_bindings = {
      { g_checkerboard_cubemap, 0 }
    };
    
    auto ft = pass_info::frame_user;

    auto shader = g_m.programs->skybox;

    auto init = []() {};
    
    auto select = scene_graph_select(n, n == g_graph->test_indices.area_sphere);

    auto envmap_id = framebuffer_ops::k_uninit;
    
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
      envmap_id,
      active
    };

    add_pointlights(room);

    g_render_passes.push_back(room);
  }
};

static void init_api_data() {
    g_view.reset_proj();

    g_m.programs->load();

    GL_FN(glGenVertexArrays(1, &g_vao));
    GL_FN(glBindVertexArray(g_vao));

    g_models.modind_sphere = g_models.new_sphere( );
    g_models.modind_area_sphere = g_models.new_sphere( );
    
    frame_model fmod{};
    fmod.render_cube_id = g_m.framebuffer->add_render_cube(TEST_SPHERE_POS,
                                                  TEST_SPHERE_RADIUS);
    
    g_frame_model_map[g_models.modind_sphere] = fmod;
    
    g_checkerboard_cubemap = g_m.textures->new_cubemap(256, 256, GL_RGBA);
    
    real_t wall_size = R(15.0);
    
    g_vertex_buffer.reset();

    g_graph.reset(new scene_graph{});

    {
      scene_graph::init_info sphere;

      sphere.position = vec3_t{ROOM_SPHERE_POS};
      sphere.scale = vec3_t{ROOM_SPHERE_RADIUS};
      sphere.angle = vec3_t{0};

      sphere.model = g_models.modind_area_sphere;
      sphere.parent = 0;
      sphere.bvol = g_m.geom->make_bsphere(ROOM_SPHERE_RADIUS, ROOM_SPHERE_POS);

      g_graph->test_indices.area_sphere = g_graph->new_node(sphere);
    }
    
    {
      scene_graph::init_info sphere;

      sphere.position = vec3_t{TEST_SPHERE_POS};
      sphere.scale = vec3_t{TEST_SPHERE_RADIUS};
      sphere.angle = vec3_t{0};

      sphere.model = g_models.modind_sphere;
      sphere.parent = g_graph->test_indices.area_sphere;
      sphere.pickable = true;
      sphere.bvol = g_m.geom->make_bsphere(TEST_SPHERE_RADIUS, TEST_SPHERE_POS);

      g_graph->test_indices.sphere = g_graph->new_node(sphere);
    }
    
    {
      scene_graph::init_info floor;

      floor.position = vec3_t{0};
      floor.scale = R3v(1.0, 1.0, 1.0);
      floor.angle = vec3_t{0};

      floor.model = g_models.new_wall(models::wall_bottom, R4v(1.0, 1.0, 1.0, 1.0));
      
      floor.parent = g_graph->test_indices.area_sphere;
      
      g_graph->test_indices.floor = g_graph->new_node(floor);
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
                      g_models.new_sphere());

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
        screen_sp[0][0] = R(g_view.view_width);
        screen_sp[1][1] = R(g_view.view_height);

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

        return glm::inverse(g_view.proj * g_view.view()) * mouse;
    }
    
    bool cast_ray(scene_graph::index_type entity) const {
        vec3_t nearp = screen_out();
        module_geom::ray world_raycast{}; 
        world_raycast.dir = glm::normalize(nearp - g_view.position);
        world_raycast.orig = g_view.position;

        auto bvol = g_graph->bound_volumes[entity];

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
            
            vec3_t Po{g_graph->positions[g_obj_manip->entity_selected]}; 

            vec3_t Fo{Po - g_view.position}; // negated z-axis of transform defined by the plane of interest

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
          g_graph->select(scene_graph_select(entity, 
                                             g_models.type(g_graph->model_indices[entity]) == 
                                              models::model_sphere && 
                                              g_graph->pickable[entity] == true));
        
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

        g_view.orient = mat3_t(yRot * xRot) * g_view.orient;
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

    g_m.init();

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

    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    maybe_enable_cursor(window);
    
    init_api_data();
    init_render_passes();

    GL_FN(glEnable(GL_FRAMEBUFFER_SRGB));
    
    //    toggle_framebuffer_srgb();
    
    while (!glfwWindowShouldClose(window)) {
        g_view(g_cam_move_state);

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