#include "render_pipeline.hpp"

template <class uniformType, shader_uniform_storage::uniform_type unif_type>
void shader_uniform_storage::set_uniform(const std::string& name,
     const uniformType& v,
     darray<uniformType>& store) {
  auto it = datum_store.find(name);

  if (it == datum_store.end()) {
    size_t buff_offset = store.size();

    ASSERT(buff_offset <= MAX_BUFFER_OFFSET);

    store.push_back(v);

    datum d {};

    d.uniform_buffer = unif_type;
    d.uniform_buffer_offset = static_cast<buffer_offset_t>(buff_offset);

    datum_store[name] = d;
  }
  else {
    store[it->second.uniform_buffer_offset] = v;
  }
}

void shader_uniform_storage::set_uniform(const duniform& unif) {
  switch (unif.type) {
  case shader_uniform_storage::uniform_mat4x4:
    set_uniform(unif.name, unif.m4);
    break;
  case shader_uniform_storage::uniform_pointlight:
    set_uniform(unif.name, unif.pl);
    break;
  case shader_uniform_storage::uniform_material:
    set_uniform(unif.name, unif.mat);
    break;
  case shader_uniform_storage::uniform_vec2:
    set_uniform(unif.name, unif.v2);
    break;
  case shader_uniform_storage::uniform_vec3:
    set_uniform(unif.name, unif.v3);
    break;
  case shader_uniform_storage::uniform_vec4:
    set_uniform(unif.name, unif.v4);
    break;
  case shader_uniform_storage::uniform_int32:
    set_uniform(unif.name, unif.i32);
    break;
  case shader_uniform_storage::uniform_float32:
    set_uniform(unif.name, unif.f32);
    break;
  default:
    __FATAL__("unif.type hasn't been implemented here (it needs to be).");
    break;
  }
}

void shader_uniform_storage::set_uniform(const std::string& name, const mat4_t& m) {
  set_uniform<mat4_t, shader_uniform_storage::uniform_mat4x4>(name, m, mat4x4_store);
}

void shader_uniform_storage::set_uniform(const std::string& name, const vec2_t& v) {
  set_uniform<vec2_t, shader_uniform_storage::uniform_vec2>(name, v, vec2_store);
}

void shader_uniform_storage::set_uniform(const std::string& name, const vec3_t& v) {
  set_uniform<vec3_t, shader_uniform_storage::uniform_vec3>(name, v, vec3_store);
}

void shader_uniform_storage::set_uniform(const std::string& name, const vec4_t& v) {
  set_uniform<vec4_t, shader_uniform_storage::uniform_vec4>(name, v, vec4_store);
}

void shader_uniform_storage::set_uniform(const std::string& name, int32_t i) {
  set_uniform<int32_t, shader_uniform_storage::uniform_int32>(name, i, int32_store);
}

void shader_uniform_storage::set_uniform(const std::string& name, const dpointlight& pl) {
  set_uniform<dpointlight, shader_uniform_storage::uniform_pointlight>(name, pl, pointlight_store);
}

void shader_uniform_storage::set_uniform(const std::string& name, const dmaterial& m) {
  set_uniform<dmaterial, shader_uniform_storage::uniform_material>(name, m, material_store);
}

void shader_uniform_storage::set_uniform(const std::string& name, float f) {
  set_uniform<float, shader_uniform_storage::uniform_float32>(name, f, float32_store);
}

void shader_uniform_storage::upload_uniform(const std::string& name) const {
  const datum& d = datum_store.at(name);

  switch (d.uniform_buffer) {
  case uniform_mat4x4:
    g_m.programs->up_mat4x4(name, mat4x4_store.at(d.uniform_buffer_offset));
    break;

  case uniform_pointlight:
    g_m.programs->up_pointlight(name, pointlight_store.at(d.uniform_buffer_offset));
    break;

  case uniform_material:
    g_m.programs->up_material(name, material_store.at(d.uniform_buffer_offset));
    break;

  case uniform_vec2:
    g_m.programs->up_vec2(name, vec2_store.at(d.uniform_buffer_offset));
    break;

  case uniform_vec3:
    g_m.programs->up_vec3(name, vec3_store.at(d.uniform_buffer_offset));
    break;

  case uniform_vec4:
    g_m.programs->up_vec4(name, vec4_store.at(d.uniform_buffer_offset));
    break;

  case uniform_int32:
    g_m.programs->up_int(name, int32_store.at(d.uniform_buffer_offset));
    break;

  case uniform_float32:
    g_m.programs->up_float(name, float32_store.at(d.uniform_buffer_offset));
    break;
  default:
    __FATAL__("d.uniform_buffer hasn't been implemented here (it needs to be).");
    break;
  }

}
