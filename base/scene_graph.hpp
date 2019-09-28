#pragma once
#include "common.hpp"
#include "models.hpp"
#include "geom.hpp"

#include <glm/gtc/constants.hpp>

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
  darray<module_models::index_type> model_indices;
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
    module_models::index_type model;
    index_type parent;
    bool draw;
    bool pickable;

    init_info()
      : position(R(0)), angle(R(0)), scale(R(1)),
        accum(true, true, false),
        model(unset<module_models::index_type>()),
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