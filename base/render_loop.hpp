#pragma once

#include "common.hpp"

struct render_loop {
private:
  bool m_running{false};

public:
  virtual ~render_loop() {}
  virtual void init() = 0;
  virtual void update() = 0;
  virtual void render() = 0;

  void post_init();
  bool running() const;
  void set_running(bool v);
};




