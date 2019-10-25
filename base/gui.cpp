#include "gui.hpp"

#define NK_IMPLEMENTATION
#include "nuklear.h"

static constexpr size_t MAX_MEMORY = 1024 * 512;

struct module_gui::internal_state {
  struct nk_context ctx;

  void init() {
    #if 0
    nk_init_fixed(&ctx, 
                  calloc(1, MAX_MEMORY), 
                  MAX_MEMORY,
                  );
    #endif
  }

};

module_gui::module_gui()
  : state{new internal_state()} {
  state->init();
}

module_gui::~module_gui() { 

}