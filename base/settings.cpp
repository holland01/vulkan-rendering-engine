#include "settings.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using namespace nlohmann;

namespace {  
  vec3_t json2vec3(const json& j) {
    return
      {
       static_cast<real_t>(j.at("x")),
       static_cast<real_t>(j.at("y")),
       static_cast<real_t>(j.at("z"))
      };
  }

  vec4_t json2vec4(const json& j) {
    return
      {
       static_cast<real_t>(j.at("x")),
       static_cast<real_t>(j.at("y")),
       static_cast<real_t>(j.at("z")),
       static_cast<real_t>(j.at("w"))
      };
  }

  class query {
    struct node {
      std::string name;
      json j;
    };
    
    darray<node> m_nodes;
    
    node* m_ptr;

    const json& m_root;

    bool m_err;

    void search(const json& parent, const std::string& prop_name) {
      auto it = parent.find(prop_name);
      
      if (it != parent.end()) {
	m_nodes.push_back({prop_name, *it});
	m_ptr = m_nodes.data() + (m_nodes.size() - 1);
      }
      else {
	m_err = true;
      }
    }
    
  public:
    using this_type = query;

    query(const json& root)
      : m_ptr{nullptr},
	m_root{root},
	m_err{false} {}
    
    double get_double() const {
      double ret{};
      if (c_assert(exists())) {
	ret = m_ptr->j.get<double>();
      }
      return ret;
    }

    bool get_bool() const {
      bool ret{false};
      if (c_assert(exists())) {
	ret = m_ptr->j.get<bool>();
      }
      return ret;
    }

    std::string get_string() const {
      std::string ret{""};
      if (c_assert(exists())) {
	ret = m_ptr->j.get<std::string>();
      }
      return ret;
    }

    bool exists() const {
      return !m_err && m_ptr != nullptr;
    }

    this_type& operator[](const std::string& prop_name) {
      if (!m_err) {
	if (m_ptr == nullptr) {
	  ASSERT(m_nodes.empty());       
	  search(m_root, prop_name);
	}
	else {
	  search(m_ptr->j, prop_name);
	}
      }
      return *this;
    }
  };

  query make_query(const json& root, const std::string& prop_seq) {
    darray<std::string> v{string_split(prop_seq, '.')};

    query q{root};

    puts("make_query split results:");
    for (const std::string& s: v) {
      printf("\ts = %s\n", s.c_str());
      q[s];
    }

    return q;
  }
}

// is called after parsing and deserialization
// has taken place
bool settings::vk::ok() const {
  return
    c_assert((renderer.max_frames_in_flight == renderer.swapchain_image_count) ||
	     renderer.render.allow_more_frames_than_fences) &&
    c_assert(renderer.select_present_mode.select_method != settings::vk::present_mode_select::best_fit);
}

bool settings::read() {
  
  std::ifstream input("./settings.json");

  bool success{input.good()};
  
  if (success) {
    json j;
    input >> j;

    auto shapes = j.find("shapes");

    if (shapes != j.end()) {    
      for (const auto& shape: *shapes) {
	add_shape as{*this};

	auto center = shape.find("center");
	if (center != shape.end()) {
	  as.with_center(json2vec3(*center));
	  std::cout << "\tcenter: " << *center << std::endl; 
	}

	auto size = shape.find("size");
	if (size != shape.end()) {
	  as.with_size(json2vec3(*size));
	  std::cout << "\tsize: " << *size << std::endl;
	}

	auto color = shape.find("color");
	if (color != shape.end()) {
	  as.with_color(json2vec4(*color));
	  std::cout << "\tcolor: " << *color << std::endl;
	}

	as.insert();
      }
    }

    //
    // see settings.json for insight into how the data is laid out.
    // we already have defaults defined for the elements in the
    // vk struct.
    //
    // the swapchain_image_count_t relies on int constants since it's just simpler
    // for now, but we'll change that to something more intuitive later.
    //

    auto handle_swapchain_option =
      [this, &success](int8_t value) -> vk::swapchain_image_count_t {
	vk::swapchain_image_count_t count{};

	write_logf("value = %" PRId32, value);
	
	if (value < 0) {
	  switch (value) {
	  case -2:
	    count = vk::swapchain_option::max_image_count;
	    break;
	  case -1:
	    count = vk::swapchain_option::min_image_count;		
	    break;
	  default:
	    success = false;
	    write_logf("FATAL ERROR: invalid swapchain_option %" PRId32 " received for parameter\n",
		       value);
	    break;
	  }
	}
	else {
	  count = static_cast<uint8_t>(value);
	}

	return count;
      };

    
    
    auto handle_present_mode =
      [this, &success](const std::string& value) -> vk::present_mode_select {
	std::unordered_map<std::string, vk::present_mode_select> valids
	  {
	   { "fifo", vk::present_mode_select::fifo },
	   { "fifo_relaxed", vk::present_mode_select::fifo_relaxed }
	   /* { "best_fit", vk::present_mode_select::best_fit } <---- not yet implemented */ 
	  };
	
	vk::present_mode_select ret = vk::present_mode_select::fifo;

	if (valids.find(value) != valids.end()) {
	  ret = valids.at(value);
	}
	else {
	  success = false;
	  write_logf("FATAL ERROR: invalid present_mode_select %s received for parameter\n",
		     value.c_str());
	}
      
	return ret;
      };

    struct qentry {
      std::string name;
      std::function<void(const query& q)> expr;
    };

#define Q_entry(property, expr)			\
    {						\
      #property,				\
      [&](const query& q) {			\
	m_vk.property = expr;			\
      }						\
    }
    
    darray<qentry> entries =
      {
       Q_entry(renderer.max_frames_in_flight, handle_swapchain_option(static_cast<int8_t>(q.get_double()))),
       Q_entry(renderer.swapchain_image_count, handle_swapchain_option(static_cast<int8_t>(q.get_double()))),
       Q_entry(renderer.enable_validation_layers, q.get_bool()),

       Q_entry(renderer.render.use_frustum_culling, q.get_bool()),
       Q_entry(renderer.render.allow_more_frames_than_fences, q.get_bool()),

       Q_entry(renderer.setup_vertex_buffer.use_staging, q.get_bool()),

       Q_entry(renderer.setup.use_single_pass, q.get_bool()),

       Q_entry(renderer.select_present_mode.select_method, handle_present_mode(q.get_string())),

       Q_entry(image_pool.make_image.always_produce_optimal_images, q.get_bool())
      };

    printf("-----Reading config-----\n");
    
    for (const qentry& entry: entries) {
      if (success) {
	printf("Processing %s...\n", entry.name.c_str());
	query q = make_query(j, entry.name);
	if (q.exists()) {
	  printf("\t%s exists\n", entry.name.c_str());
	  entry.expr(q);
	}
	else {
	  printf("\t%s does not exist\n", entry.name.c_str());
	}
      }
    }

#undef Q_entry

    success = c_assert(success) && m_vk.ok();
  }

  return success;
}

