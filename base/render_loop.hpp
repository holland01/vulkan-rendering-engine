#pragma once

#include "common.hpp"
#include <iostream>


struct render_loop {
protected:
  static inline constexpr double k_time_delta_seconds = 5.0;
  static inline constexpr double k_inv_time_delta_seconds = 1.0 / k_time_delta_seconds;
  
  double m_frame_start_s{0.0};
  double m_fps_ema{60.0};
  uint32_t m_frame_index{0};
  double m_dtime{0.0};
  double m_atime{0.0}; // accum
  uint64_t m_present_count{0};
  
  bool m_running{true};

public:
  virtual ~render_loop() {}
  virtual void init() = 0;
  virtual void update() {
    m_frame_start_s = glfwGetTime();
  };
  virtual void render() = 0;

  void post_update() {
    m_present_count++;
    
    m_atime += glfwGetTime() - m_frame_start_s;
    
    if (m_atime >= k_time_delta_seconds) {
      double fps = static_cast<double>(m_present_count) * k_inv_time_delta_seconds;
      std::cout << "FPS: " << fps << std::endl;
      
      m_atime = 0.0;
      m_present_count = 0;
    }
  }

  void post_init();
  bool running() const;
  void set_running(bool v);
};




