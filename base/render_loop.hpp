#pragma once

#include "common.hpp"

struct render_loop {
private:
  double m_frame_start_s{0.0};
  bool m_running{false};

public:
  virtual ~render_loop() {}
  virtual void init() = 0;
  virtual void update() {
    m_frame_start_s = glfwGetTime();
  };
  virtual void render() = 0;

  void show_fps(GLFWwindow* w) const {
    double end_time_s = glfwGetTime() - m_frame_start_s;
    double fps = 1.0 / end_time_s;
    std::string str_fps{std::to_string(fps)};
    glfwSetWindowTitle(w, str_fps.c_str());
  }

  void post_init();
  bool running() const;
  void set_running(bool v);
};




