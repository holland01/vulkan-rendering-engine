#pragma once

#include "common.hpp"
#include <glm/gtc/matrix_transform.hpp>

struct move_state {
    uint8_t up : 1;
    uint8_t down : 1;
    uint8_t right : 1;
    uint8_t left : 1;
    uint8_t front : 1;
    uint8_t back : 1;
    uint8_t __rem : 2;
};

// key facts about transformations:
// - a matrix transform represents the source space from the perspective
// of the destination space

struct view_data {
    mat4_t proj, skyproj, cubeproj, ortho;    // proj: camera to clip space transform

    mat4_t view_mat;

    mat3_t orient;  // orient: camera orientation

    vec3_t position;  // position: theoretical position of the camera: technically,
                         // view space is always centered at a specific location that's
                         // k units of distance before the near plane. Thus, it's always
                         // looking down -z. In other words, the viewing volume (frustum)
                         // is always in the same space, and the camera facing its near plane is also
                         // always at the same space. The "position" vector we have here
                         // is used to provide the illusion of a moving camera. More on this later.

    real_t step;          // step: move step amount

    real_t skynearp;
    real_t nearp;
    real_t skyfarp;
    real_t farp;

    uint16_t view_width;
    uint16_t view_height;

    bool view_bound;
    
    view_data(uint16_t width, uint16_t height)
        : proj(1.0f), skyproj(R(1.0)), cubeproj(R(1.0)), ortho(R(1.0)),
          view_mat(R(1.0)),
          orient(1.0f),
          position(0.0f),
          step(0.1f),
          skynearp(10.0f),
          nearp(1.0f),
          skyfarp(1000.0f),
          farp(1000.0f),
          view_width(width),
          view_height(height),
          view_bound(false)
    {
    }

    auto calc_aspect() const {
        return static_cast<real_t>(view_width) / static_cast<real_t>(view_height);
    }

    void reset_proj() {
        set_proj_from_fovy(45.0f);
        skyproj = glm::perspective(45.0f, calc_aspect(), skynearp, skyfarp);
        cubeproj = glm::perspective(45.0f, calc_aspect(), nearp, farp);

        real_t w = view_width * 0.5;
        real_t h = view_height * 0.5;

        ortho = glm::orthoRH(-w, w, -h, h, nearp, farp);
    }

    void set_proj_from_fovy(real_t fovy) {
        proj = glm::perspective(fovy, calc_aspect(), nearp, farp);
    }

    //
    // view_<dir>(): series of functions used for "moving" the camera in a particular direction.
    //
    // Pick a translation direction, and RET_VIEW_DIR will produce a transformed vector offset
    // such that any object's within the viewing volume will be oriented/moved
    // to provide the illusion that the viewer is moving in the requested direction.
    //
#define RET_VIEW_DIR(x, y, z) vec3_t dir(glm::inverse(orient) * vec3_t(x, y, z)); return dir

    vec3_t view_up() const { RET_VIEW_DIR(0.0f, 1.0f, 0.0f); }
    vec3_t view_down() const { RET_VIEW_DIR(0.0f, -1.0f, 0.0f); }

    vec3_t view_right() const { RET_VIEW_DIR(1.0f, 0.0f, 0.0f); }
    vec3_t view_left() const { RET_VIEW_DIR(-1.0f, 0.0f, 0.0f); }

    vec3_t view_front() const { RET_VIEW_DIR(0.0f, 0.0f, -1.0f); }
    vec3_t view_back() const { RET_VIEW_DIR(0.0f, 0.0f, 1.0f); }

#undef RET_VIEW_DIR

    //
    // view(): compute the model to camera transform.
    //
    // We need two proper transforms for this:
    // the first is our translation, the second is 
    // the orientation for the view.
    // Let T be the translation matrix and O
    // be our orientation.
    // 
    // Back to the idea of the "position",
    // mentioned earlier: we're performing an illusion
    // in terms of how we express the camera's movement
    // relative to the world. In other words, the camera _isn't_ moving,
    // but the world around it is.
    //
    // Suppose we have some model with a corresponding transform M
    // that we wish to render. We know that M is going to have its own
    // separate orientation and position offset within our world. 
    // Thus, the translation we're creating here is designed to move
    // the model represented by M a series of units _closer_ to the viewing volume.
    //
    // That series of units is our camera's "position".
    //
    // Suppose p is our position vector, then T equals:
    //
    // T = | 1 0 0 -p.x |
    //     | 0 1 0 -p.y |
    //     | 0 0 1 -p.z |
    //     | 0 0 0   1  |
    //
    // Now, suppose we don't care about orientation for the moment.
    // Ignoring the camera to clip transform as well,
    // when the model M transform is used with T, we can express the resulting
    // transform Q as:
    //
    // Q = TM
    //
    // If our resulting transform Q contains a translation whose coordinates
    // lie _outside_ of the viewing volume, then the object will be clipped,
    // and we can think of the viewer simply not having moved far enough to spot
    // the object.
    //
    // However, the further they "move" in the direction of the object, the more the
    // object itself will actually move toward the viewing volume.
    // 
    // Now, as far as the orientation, what we want to do is maintain that same principle,
    // but ensure that objects in our viewing volume maintain their distance irrespective
    // of the orientation of the viewer.
    //
    // A mouse move to the left is supposed to produce a clockwise rotation
    // about the vertical axis of any object in the viewing volume that we wish to render.
    //
    // We don't want the vertices of an object to be rotated about the object's center of origin though;
    // rather, we want to them to be rotated about the center of the origin of the _viewer_.
    // Thus, we perform the translation _first_, and then orientation second.
    //
    mat4_t view() const {
        if (view_bound) {
            return view_mat;
        } else {
            return mat4_t(orient) * glm::translate(mat4_t(1.0f), -position);
        }
    }

    void bind_view(const mat4_t& view) {
        view_mat = view;
        view_bound = true;
    }

    void unbind_view() {
        view_bound = false;
    }
    
    //
    // operator (): updates position
    //
    // move struct is  because we're using bitsets here.
    //
    // very simple: if the corresponding direction is set to 1, 
    // we compute the appropriate offset for that direction and add its
    // scaled value to our position vector.
    //
    // Our goal is to let whatever key-input methods are used
    // be handled separately from this class.
    //


#define TESTDIR(move, dir)                        \
  if ((move).dir == true) {                       \
    this->position += view_##dir() * this->step;  \
  }
  
  void operator ()(move_state m) {
    TESTDIR(m, up);
    TESTDIR(m, down);
    TESTDIR(m, right);
    TESTDIR(m, left);
    TESTDIR(m, front);
    TESTDIR(m, back);
  }

#undef TESTDIR
};