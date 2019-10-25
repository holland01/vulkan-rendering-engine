#include "scene_graph.hpp"

#include <iostream>

scene_graph::scene_graph()
  : test_indices(),
  pickfbo(g_m.framebuffer->add_fbo(g_m.framebuffer->width, g_m.framebuffer->height))
{
  // root initialization
  bound_volumes.push_back(module_geom::bvol {});
  child_lists.push_back(darray<index_type>());
  positions.push_back(vec3_t {R(0)});
  scales.push_back(vec3_t {R(1)});
  angles.push_back(vec3_t {R(0)});
  accum.push_back(boolvec3_t {false});
  node_ids.push_back(node_id());
  model_indices.push_back(unset<module_models::index_type>());
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
  auto index = static_cast<scene_graph::index_type>(child_lists.size());

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

  if (info.pickable) {
    ASSERT(index < 25);
    vec4_t color {R(index) * R(10) * k_to_rgba8, R(0), R(0), 1};
    color.a = R(1);
    std::cout << "(" << index << ") " << AS_STRING_GLM_SS(color) << std::endl;
    pickmap[index] = color;
  }

  return index;
}

scene_graph::index_type scene_graph::trypick(int32_t x, int32_t y) {

  ASSERT_CODE(
    // Important that we ensure pickbufferdata
    // is also not empty. A segfault is guaranteed,
    // but more importantly we're calling trypick()
    // with unexpected input.
    ASSERT(!pickbufferdata.empty());
  u8vec4_t clear_pixel(0, 0, 0, 255);

  // If is_clear_color() returns true, then we know that
  // the framebuffer only contains whatever the color buffer 
  // attachment was cleared with. This means that whatever the user is
  // seeing isn't being copied into the buffer properly.
  ASSERT(!pickbufferdata.is_clear_color(clear_pixel))
    );

  u8vec4_t pixel = pickbufferdata.get(x, y);

  vec4_t fpixel {R(pixel.r), R(pixel.g), R(pixel.b), R(pixel.a)};
  fpixel *= k_to_rgba8;

  scene_graph::index_type ret {unset<scene_graph::index_type>()};

  for (auto [id, color]: pickmap) {
    if (color == fpixel) {
      ret = id;
      break;
    }
  }

  ASSERT_CODE(
    // This helps verify a few things things:
    // 1) we haven't drawn anything into the pickbuffer that we failed to account for
    //    throughout other areas of the system
    // 2) the fragment shader isn't writing out unexpected color values,
    //    e.g. via delinearization or some kind of unaccounted for
    //    post processing.
    // 3) If we did change the clear color (yes, this should be made into a constant),
    //    we need to update the value here, as well as 
    //    in a few other areas.
    if (ret == unset<scene_graph::index_type>()) {
      ASSERT(fpixel == R4v(0, 0, 0, 1));
    }
  );

  return ret;
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
  mat4_t m {m4i()};

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

    mat4_t world_accum {world * model_transform(node)};

    g_m.models->render(model_indices[node], world_accum);

  }
  else {
    mat4_t world_accum {world * modaccum_transform(traverse_node)};

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

    draw_node(node,
              traverse,
              id,
              modaccum_transform(0));

    id->reset();
  }
}

void scene_graph::draw_all(index_type current, const mat4_t& world) const {
  mat4_t accum {world * modaccum_transform(current)};

  if (draw[current]) {
    if (permodel_unif_set_fn) {
      permodel_unif_set_fn(current);
    }

    mat4_t raccum {world * model_transform(current)};
    g_m.models->render(model_indices[current], raccum);
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
