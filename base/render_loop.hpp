#pragma once

#include "common.hpp"

struct render_loop {
protected:
  double m_frame_start_s{0.0};
  double m_fps_ema{60.0};
  uint32_t m_frame_index{0};
  double m_dtime{0.0};
  
  bool m_running{false};

public:
  virtual ~render_loop() {}
  virtual void init() = 0;
  virtual void update() {
    m_frame_start_s = glfwGetTime();
  };
  virtual void render() = 0;

  void show_fps(GLFWwindow* w) {
    double dtime_s = m_dtime;

    constexpr double k_smooth{0.25};   
    
    real_t fps = 1.0 / dtime_s;
    m_fps_ema = k_smooth * fps + (1 - k_smooth) * m_fps_ema;
    
    std::string str_fps{std::to_string(fps)};
    glfwSetWindowTitle(w, str_fps.c_str());
  }

  void post_init();
  bool running() const;
  void set_running(bool v);
};




