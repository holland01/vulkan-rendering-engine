#pragma once

#include "common.h"

struct frame {
    uint32_t count;
    uint32_t width;
    uint32_t height;
    
frame(uint32_t w, uint32_t h)
: count(0),
        width(w),
        height(h) {}
    
    void screenshot();
} extern g_frame;
