#include "render_loop.hpp"
#include "device_context.hpp"

void render_loop::post_init() {
  set_running(true);
}

bool render_loop::running() const {
  return
    g_m.device_ctx->ok() &&
    !glfwWindowShouldClose(g_m.device_ctx->window()) &&
    m_running;
}

void render_loop::set_running(bool v) {
  if (m_running) {
    m_running = v;
  }
}

