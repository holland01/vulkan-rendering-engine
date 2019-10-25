#pragma once

#include "common.hpp"

struct module_gui {
  struct internal_state;

  std::unique_ptr<internal_state> state;

  module_gui();
  ~module_gui();
};