#include <string>
#include <sstream>


#include "frame.hpp"
#include "util.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void framebuffer_ops::screenshot() {
  std::stringstream ss;

  ss << "screenshoot_" << count << ".png";

  std::vector<uint8_t> framebuffer(width * height * 4, 0);

  GL_FN(glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &framebuffer[0]));

  // STB uses an inverted y axis
  {
    decltype(height) row = 0;
    auto endrow = height / 2;
    std::vector<uint8_t> rowmem(width * 4, 0);
    while (row < endrow) {
      auto tail = height - row -1;

      memcpy(&rowmem[0],
             &framebuffer[width * 4 * row],
             width * 4);

      memcpy(&framebuffer[width * 4 * row],
             &framebuffer[width * 4 * tail],
             width * 4);

      memcpy(&framebuffer[width * 4 * tail],
             &rowmem[0],
             width * 4);

      row++;
    }
  }

  std::string filename = ss.str();

  stbi_write_png(filename.c_str(), width, height, 4, static_cast<void*>(framebuffer.data()), 0);
}
