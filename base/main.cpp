#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <GL/glew.h>

#include <GLFW/glfw3.h>
#include <memory>
#include <functional>
#include <array>

#include <iostream>

#include "common.h"
#include "textures.h"
#include "util.h"
#include "programs.h"
#include "geom.h"
#include "frame.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>


#define OBJECT_SELECT_MOVE_STEP real_t(0.01)

frame g_frame {SCREEN_WIDTH, SCREEN_HEIGHT};

std::vector<type_module*> g_modules;

textures::index_type g_skybox_texture {textures::k_uninit};

GLuint g_vao = 0;
//
//

struct move_state {
    uint8_t up : 1;
    uint8_t down : 1;
    uint8_t right : 1;
    uint8_t left : 1;
    uint8_t front : 1;
    uint8_t back : 1;
    uint8_t __rem : 2;
};

static move_state  g_cam_move_state = {
    0, 0, 0, 0, 0, 0, 0
};

static move_state  g_select_move_state = {
    0, 0, 0, 0, 0, 0, 0
};

// key facts about transformations:
// - a matrix transform represents the source space from the perspective
// of the destination space

struct view_data {
    mat4_t proj, skyproj;    // proj: camera to clip space transform

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

    view_data(uint16_t width, uint16_t height)
        : proj(1.0f),
        orient(1.0f),
        position(0.0f),
        step(0.1f),
        skynearp(10.0f),
        nearp(1.0f),
        skyfarp(1000.0f),
        farp(1000.0f),
        view_width(width),
        view_height(height) {
    }

    auto calc_aspect() const {
        return static_cast<real_t>(view_width) / static_cast<real_t>(view_height);
    }

    void reset_proj() {
        set_proj_from_fovy(45.0f);
        skyproj = glm::perspective(120.0f, calc_aspect(), skynearp, skyfarp);
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
        return mat4_t(orient) * glm::translate(mat4_t(1.0f),
                                                  -position);
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

static view_data g_view(SCREEN_WIDTH, SCREEN_HEIGHT);

struct vertex_buffer {

    std::vector<vertex> data;

    mutable GLuint vbo;

    vertex_buffer()
        : vbo(0) {
    }

    void bind() const {
        GL_FN(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    }

    void unbind() const {
        GL_FN(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }

    void push(const vertex& v) {
        data.push_back(v);
    }

    void reset() const {
        if (vbo == 0) {
            GL_FN(glGenBuffers(1, &vbo));
        }

        bind();

        GL_FN(glBufferData(GL_ARRAY_BUFFER,
                           sizeof(data[0]) * data.size(),
                           &data[0],
                           GL_STATIC_DRAW));

        unbind();
    }

    auto num_vertices() const {
        return static_cast<int>(data.size());
    }

    auto add_triangle(const vec3_t& a_position, const vec4_t& a_color, const vec2_t& a_uv,
                      const vec3_t& b_position, const vec4_t& b_color, const vec2_t& b_uv,
                      const vec3_t& c_position, const vec4_t& c_color, const vec2_t& c_uv) {
        vertex a = {
            a_position,
            a_color,
            a_uv
        };

        vertex b = {
            b_position,
            b_color,
            b_uv
        };

        vertex c = {
            c_position,
            c_color,
            c_uv
        };

        auto offset = num_vertices();

        data.push_back(a);
        data.push_back(b);
        data.push_back(c);

        return offset;

    }

    auto add_triangle(const vec3_t& a_position, const vec4_t& a_color,
                      const vec3_t& b_position, const vec4_t& b_color,
                      const vec3_t& c_position, const vec4_t& c_color) {
        vec2_t defaultuv(glm::zero<vec2_t>());

        return add_triangle(a_position, a_color, defaultuv,
                            b_position, b_color, defaultuv,
                            c_position, c_color, defaultuv);
    }

} static g_vertex_buffer;

struct models {
    enum transformorder {
        // srt -> translate first, then rotate, then scale. Order is backwards
        // on purpose, since mathematically this is how transforms work with the current
        // conventions used.
        transformorder_srt = 0,
        transformorder_trs, 
        transformorder_lookat,
        transformorder_skybox,
        transformorder_count
    };

    enum model_type {
        model_unknown = 0,
        model_tri,
        model_sphere,
        model_cube
    };

    using index_type = int32_t;
    using transform_fn_type = std::function<mat4_t(index_type model)>;
    using index_list_type = std::vector<index_type>;
    using predicate_fn_type = std::function<bool(const index_type&)>;
    
    static const inline index_type k_uninit = -1;

    
    std::array<transform_fn_type, transformorder_count> __table = {
        [this](int model) { return scale(model) * rotate(model) * translate(model); },
        [this](int model) { return translate(model) * rotate(model) * scale(model); },
        [this](int model) { return glm::inverse(g_view.view())
        * glm::lookAt(look_at.eye, look_at.center, look_at.up); },
        [this](int model) {
        return glm::translate(mat4_t(1.0f), g_view.position) * scale(model);
    }
    };

    std::vector<vec3_t> positions;
    std::vector<vec3_t> scales;
    std::vector<vec3_t> angles;
    std::vector<model_type> model_types;
    std::vector<index_type> vertex_offsets;
    std::vector<index_type> vertex_counts;
    std::vector<geom::bvol> bound_volumes;
    std::vector<bool> draw;
    

    struct {
        vec3_t eye;
        vec3_t center;
        vec3_t up;
    } look_at = {
        glm::zero<vec3_t>(),
        glm::zero<vec3_t>(),
        glm::zero<vec3_t>()
    };

    index_type model_count = 0;
    
    index_type modind_tri = 0;
    index_type modind_sphere = 1;
    index_type modind_skybox = 2;

    index_type modind_selected = k_uninit;

    // It's assumed that vertices
    // have been already added to the vertex
    // buffer when this function is called,
    // so we explicitly reallocate the needed
    // VBO memory every time we add new
    // model data with this function.
    auto new_model(model_type mt,
                   index_type vbo_offset = 0,
                   index_type num_vertices = 0,
                   vec3_t position = glm::zero<vec3_t>(),
                   vec3_t scale = vec3_t(1.0f),
                   vec3_t angle = vec3_t(0.0f),
                   geom::bvol bvol = geom::bvol()) {

        index_type id = static_cast<index_type>(model_count);

        model_types.push_back(mt);
        
        positions.push_back(position);
        scales.push_back(scale);
        angles.push_back(angle);

        vertex_offsets.push_back(vbo_offset);
        vertex_counts.push_back(num_vertices);

        draw.push_back(true);       

        bound_volumes.push_back(bvol);
        
        model_count++;

        g_vertex_buffer.reset();

        return id;
    }    
    
    void clear_select_model_state() {
        if (modind_selected != k_uninit) {
            // TODO: cleanup select state for previous model here
        }

        modind_selected = k_uninit;
    }
    
    void set_select_model_state(index_type model) {
        clear_select_model_state();

        modind_selected = model;
    }

    bool has_select_model_state() const {
        return modind_selected != k_uninit;
    }

    enum _move_op {
        mop_add,
        mop_sub,
        mop_set
    };
    
    void move(index_type model, const vec3_t& position, _move_op mop) {
        switch (mop) {
        case mop_add: positions[model] += position; break;
        case mop_sub: positions[model] -= position; break;
        case mop_set: positions[model] = position; break;
        }

        bound_volumes[model].center = positions[model];
    }

    // Place model 'a' ontop of model 'b'.
    // Right now we only care about bounding spheres
    void place_above(index_type a, index_type b) {
        ASSERT(bound_volumes[a].type == geom::bvol::type_sphere);
        ASSERT(bound_volumes[b].type == geom::bvol::type_sphere);

        vec3_t a_position{positions[b]};

        auto arad = bound_volumes[a].radius;
        auto brad = bound_volumes[b].radius;

        a_position.y += arad + brad;

        move(a, a_position, mop_set);
    }
    
#define MAP_UPDATE_SELECT_MODEL_STATE(dir, axis, amount)\
    if (g_select_move_state.dir) { update.axis += amount;  }

    // This is continuously called in the main loop,
    // so it's important that we don't assume
    // that this function is alaways called when
    // a valid index hits.
    void update_select_model_state() {
        ASSERT(has_select_model_state());

        vec3_t update{R(0.0)};
        
        MAP_UPDATE_SELECT_MODEL_STATE(front, z, -OBJECT_SELECT_MOVE_STEP);
        MAP_UPDATE_SELECT_MODEL_STATE(back, z, OBJECT_SELECT_MOVE_STEP);

        MAP_UPDATE_SELECT_MODEL_STATE(right, x, OBJECT_SELECT_MOVE_STEP);
        MAP_UPDATE_SELECT_MODEL_STATE(left, x, -OBJECT_SELECT_MOVE_STEP);

        MAP_UPDATE_SELECT_MODEL_STATE(up, y, OBJECT_SELECT_MOVE_STEP);
        MAP_UPDATE_SELECT_MODEL_STATE(down, y, -OBJECT_SELECT_MOVE_STEP);

        move(modind_selected, update, mop_add);
    }
    
#undef MAP_UPDATE_SELECT_MODEL_STATE

    auto new_sphere(const vec3_t& position = glm::zero<vec3_t>(), real_t scale = real_t(1)) {
        auto offset = g_vertex_buffer.num_vertices();

        real_t step = 0.1f;

        auto count = 0;

        auto cart = [](real_t phi, real_t theta) {
            vec3_t ret;
            ret.x = glm::cos(theta) * glm::cos(phi);
            ret.y = glm::sin(phi);
            ret.z = glm::sin(theta) * glm::cos(phi);
            return ret;
        };

        for (real_t phi = -glm::half_pi<real_t>(); phi <= glm::half_pi<real_t>(); phi += step) {
            for (real_t theta = 0.0f; theta <= glm::two_pi<real_t>(); theta += step) {
                auto color = vec4_t(1.0f);

                auto a = cart(phi, theta);
                auto b = cart(phi, theta + step);
                auto c = cart(phi + step, theta + step);
                auto d = cart(phi + step, theta);

                g_vertex_buffer.add_triangle(a, color,
                                             b, color,
                                             c, color);

                g_vertex_buffer.add_triangle(c, color,
                                             d, color,
                                             a, color);

                count += 6;
            }
        }

        geom::bvol bvol {};

        bvol.type = geom::bvol::type_sphere;
        bvol.radius = scale;
        bvol.center = position;

        return new_model(model_sphere,
                         offset,
                         count,
                         position,
                         vec3_t(scale),
                         glm::zero<vec3_t>(),
                         bvol);
    }

    auto new_cube(const vec3_t& position = glm::zero<vec3_t>(), const vec3_t& scale = vec3_t(1.0f), const vec4_t& color = vec4_t(1.0f)) {
        std::array<real_t, 36 * 3> vertices = {
            // positions          
            -1.0f, 1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, -1.0f,

            -1.0f, -1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f, 1.0f,
            -1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, -1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f,

            -1.0f, 1.0f, -1.0f,
            1.0f, 1.0f, -1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, 1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, 1.0f,
            1.0f, -1.0f, 1.0f
        };

        auto offset = g_vertex_buffer.num_vertices();

        for (auto i = 0; i < vertices.size(); i += 9) {
            vec3_t a(vertices[i + 0], vertices[i + 1], vertices[i + 2]);
            vec3_t b(vertices[i + 3], vertices[i + 4], vertices[i + 5]);
            vec3_t c(vertices[i + 6], vertices[i + 7], vertices[i + 8]);

            g_vertex_buffer.add_triangle(a, color,
                                         b, color,
                                         c, color);
        }

        return new_model(model_cube,
                         offset,
                         36,
                         position,
                         scale);
    }

    mat4_t scale(index_type model) const {
        return glm::scale(mat4_t(1.0f), scales.at(model));
    }

    mat4_t translate(index_type model) const {
        return glm::translate(mat4_t(1.0f), positions.at(model));
    }

    mat4_t rotate(index_type model) const {
        mat4_t rot(1.0f);

        rot = glm::rotate(mat4_t(1.0f), angles.at(model).x, vec3_t(1.0f, 0.0f, 0.0f));
        rot = glm::rotate(mat4_t(1.0f), angles.at(model).y, vec3_t(0.0f, 1.0f, 0.0f)) * rot;
        rot = glm::rotate(mat4_t(1.0f), angles.at(model).z, vec3_t(0.0f, 0.0f, 1.0f)) * rot;

        return rot;
    }

    void render(index_type model, transformorder to) const {
        if (draw.at(model) == true) {
            mat4_t T = __table[to](model);
            mat4_t mv = g_view.view() * T;

            g_programs.up_mat4x4("unif_ModelView", mv);
            g_programs.up_mat4x4("unif_Projection", model == modind_skybox ? g_view.skyproj : g_view.proj);

            auto ofs = vertex_offsets[model];
            auto count = vertex_counts[model];

            GL_FN(glDrawArrays(GL_TRIANGLES,
                               ofs,
                               count));
        }
    }

    index_list_type select(predicate_fn_type func) const {
        index_list_type members;

        for (auto i = 0; i < model_count; ++i) {
            if (func(i)) {
                members.push_back(i);
            }
        }

        return members;
    }

    model_type type(index_type i) const {
        return model_types[i];
    }
} static g_models;

struct gl_depth_func {
    GLint prev_depth;

    gl_depth_func(GLenum func) {
        GL_FN(glGetIntegerv(GL_DEPTH_FUNC, &prev_depth));
        GL_FN(glDepthFunc(func));
    }

    ~gl_depth_func() {
        GL_FN(glDepthFunc(prev_depth));
    }
};

struct gl_clear_depth {
    real_t prev_depth;

    gl_clear_depth(real_t new_depth) {
        GL_FN(glGetFloatv(GL_DEPTH_CLEAR_VALUE, &prev_depth));
        GL_FN(glClearDepth(new_depth));
    }

    ~gl_clear_depth() {
        GL_FN(glClearDepth(prev_depth));
    }
};

struct capture {
    GLuint fbo, tex, rbo;
    int width, height;

    capture()
        :
        fbo(0),
        tex(0),
        rbo(0),
        width(SCREEN_WIDTH),
        height(SCREEN_HEIGHT){
    }

    void load() {
        GL_FN(glGenFramebuffers(1, &fbo));
        bind();

        GL_FN(glGenTextures(1, &tex));
        GL_FN(glBindTexture(GL_TEXTURE_2D, tex));
        GL_FN(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL));
        GL_FN(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL_FN(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL_FN(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL_FN(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));


        GL_FN(glBindTexture(GL_TEXTURE_2D, 0));

        // attach it to currently bound framebuffer object
        GL_FN(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0));

        GL_FN(glGenRenderbuffers(1, &rbo));
        GL_FN(glBindRenderbuffer(GL_RENDERBUFFER, rbo));
        GL_FN(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height));
        GL_FN(glBindRenderbuffer(GL_RENDERBUFFER, 0));

        GL_FN(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo));

        ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        unbind();
    }

    void bind() const {
        GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    }

    void unbind() const {
        GL_FN(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }

    void sample_begin(const std::string& sampler, int slot = 0) const {
        GL_FN(glActiveTexture(GL_TEXTURE0 + static_cast<decltype(GL_TEXTURE0)>(slot)));
        GL_FN(glBindTexture(GL_TEXTURE_2D, tex));
        g_programs.up_int(sampler, slot);
    }

    void sample_end(int slot = 0) const {
        GL_FN(glActiveTexture(GL_TEXTURE0 + static_cast<decltype(GL_TEXTURE0)>(slot)));
        GL_FN(glBindTexture(GL_TEXTURE_2D, 0));
    }

} static g_capture;

static void init_api_data() {
    g_view.reset_proj();

    g_programs.load();
    g_capture.load();

    GL_FN(glGenVertexArrays(1, &g_vao));
    GL_FN(glBindVertexArray(g_vao));

    {
        real_t s = 1.0f;
        real_t t = -3.5f;

        // returns 0; used for the model id
        auto offset = g_vertex_buffer.add_triangle(vec3_t(-s, 0.0f, t), // a
                                                   vec4_t(1.0f, 0.0f, 0.0f, 1.0f),

                                                   vec3_t(s, 0.0f, t), // b
                                                   vec4_t(1.0f, 1.0f, 1.0f, 1.0f),

                                                   vec3_t(0.0f, s, t), // c
                                                   vec4_t(0.0f, 0.0f, 1.0f, 1.0f));


        g_models.modind_tri = g_models.new_model(models::model_tri, offset, 3);
    }

    g_models.modind_sphere = g_models.new_sphere(vec3_t(0.0f, 5.0f, -10.0f), R(1.5));

    auto sb = g_models.new_sphere(vec3_t{R(0.0)}, R(1.0));
    auto sc = g_models.new_sphere(vec3_t{R(0.0)}, R(1.0));
    auto sd = g_models.new_sphere(vec3_t{R(0.0)}, R(1.0));

    g_models.place_above(sb, g_models.modind_sphere);
    g_models.place_above(sc, sb);
    g_models.place_above(sd, sc);
    
    g_models.modind_skybox = g_models.new_cube();

    g_vertex_buffer.reset();

#define p(path__) fs::path("skybox0") / fs::path(path__ ".jpg")
    textures::cubemap_paths_type paths = {
        p("right"),
        p("left"),
        p("top"),
        p("bottom"),
        p("front"),
        p("back")
    };
#undef p

    g_models.scales[g_models.modind_skybox] = vec3_t(500.0f);

    g_skybox_texture = g_textures.new_cubemap(paths);

    GL_FN(glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT));

    GL_FN(glDisable(GL_CULL_FACE));
    GL_FN(glEnable(GL_DEPTH_TEST));
    GL_FN(glDepthFunc(GL_LEQUAL));
    GL_FN(glClearDepth(1.0f));
}

#define DRAW_MODELS(transform_order) for (auto i = 0; i < g_models.model_count; ++i) { g_models.render(i, transform_order); }

#define SET_CLEAR_COLOR_V4(v) GL_FN(glClearColor((v).r, (v).g, (v).b, (v).a)) 
#define CLEAR_COLOR_DEPTH GL_FN(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT))

void test_draw_skybox_scene();

void test_sphere_fbo_pass(const vec4_t& background) {
    g_capture.bind();

    SET_CLEAR_COLOR_V4(background);
    CLEAR_COLOR_DEPTH;

    g_vertex_buffer.bind();

    {
        use_program u(g_programs.default_fb);

        g_models.look_at.eye = g_models.positions[g_models.modind_sphere];
        g_models.look_at.center = g_models.positions[g_models.modind_tri];
        g_models.look_at.up = vec3_t(0.0f, 1.0f, 0.0f);

        // we're rendering from the sphere's perspective
        g_models.draw[g_models.modind_sphere] = false;

        DRAW_MODELS(models::transformorder_lookat);
    }
    g_vertex_buffer.unbind();


    test_draw_skybox_scene();

    g_capture.unbind();
}

void test_render_to_quad(const vec4_t& background) {
    SET_CLEAR_COLOR_V4(background);
    CLEAR_COLOR_DEPTH;

    {
        use_program u(g_programs.default_rtq);

        g_capture.sample_begin("unif_TexSampler", 0);

        GL_FN(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

        g_capture.sample_end();
    }
}

void test_draw_reflect_sphere(const vec4_t& background) {

    g_vertex_buffer.bind();
    {
        use_program u(g_programs.default_mir);

        g_capture.sample_begin("unif_TexFramebuffer", 0);

        g_programs.up_vec3("unif_LookCenter", g_models.look_at.center - g_models.look_at.eye);

        g_models.draw[g_models.modind_sphere] = true;

        DRAW_MODELS(models::transformorder_srt);

        g_capture.sample_end();
    }
    g_vertex_buffer.unbind();
}

void test_draw_cubemap_reflect() {
    g_vertex_buffer.bind();

    g_models.draw[g_models.modind_sphere] = true;
    g_models.draw[g_models.modind_tri] = true;
    g_models.draw[g_models.modind_skybox] = false;

    {
        use_program u(g_programs.sphere_cubemap);

        int slot = 0;

        g_textures.bind(g_skybox_texture, slot);
        g_programs.up_int("unif_TexCubeMap", slot);

        g_programs.up_mat4x4("unif_InverseView", glm::inverse(g_view.view()));
        g_programs.up_vec3("unif_CameraPosition", g_view.position);

        DRAW_MODELS(models::transformorder_trs);
    }

    g_vertex_buffer.unbind();
}

void test_draw_skybox_scene() {

    g_vertex_buffer.bind();

    g_models.draw[g_models.modind_skybox] = true;
    {
        use_program u(g_programs.skybox);

        int slot = 0;

        g_textures.bind(g_skybox_texture, slot);

        g_programs.up_int("unif_TexCubeMap", slot);

        g_models.render(g_models.modind_skybox,
                        models::transformorder_skybox);

        g_textures.unbind(g_skybox_texture, slot);
    }
    g_vertex_buffer.unbind();
}

void test_main_0() {
    vec4_t background(0.0f, 0.3f, 0.0f, 1.0f);

    GL_FN(glDepthFunc(GL_LESS));
    g_models.draw[g_models.modind_skybox] = false;
    test_sphere_fbo_pass(background);

    g_models.draw[g_models.modind_skybox] = false;
    SET_CLEAR_COLOR_V4(background);
    CLEAR_COLOR_DEPTH;

    test_draw_reflect_sphere(background);
    test_draw_skybox_scene();
}

static void render() {
    vec4_t background(0.0f, 0.3f, 0.0f, 1.0f);
    GL_FN(glDepthFunc(GL_LESS));

    SET_CLEAR_COLOR_V4(background);
    CLEAR_COLOR_DEPTH;

    test_draw_skybox_scene();
    test_draw_cubemap_reflect();
}

static void error_callback(int error, const char* description) {
    fputs(description, stdout);
}

// origin for coordinates is the top left
// of the window
struct camera_orientation {
    double prev_xpos;
    double prev_ypos;

    double dx;
    double dy;

    bool active;
} static g_cam_orient = {
    0.0, 0.0,
    0.0, 0.0,
    false
};

// approximately 400 virtual key identifiers
// defined by GLFW
static std::array<bool, 400> g_key_states;

void maybe_enable_cursor(GLFWwindow* w) {
    if (g_cam_orient.active) {
        glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else {
        glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

// we only check once here since
// a keydown can trigger an action,
// and most of the time we don't want
// that action to repeat for
// the entire duration of the key press
bool keydown_if_not(int key) {
    bool isnotdown = !g_key_states[key];

    if (isnotdown) {
        g_key_states[key] = true;
    }

    return isnotdown;
}

// this callback is a slew of macros to make changes and adaptations easier
// to materialize: much of this is likely to be altered as new needs are met,
// and there are many situations that call for redundant expressions that may
// as well be abstracted away
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
#define MAP_MOVE_STATE_TRUE(key, dir) case key: g_cam_move_state.dir = true; break
#define MAP_MOVE_STATE_FALSE(key, dir) case key: g_cam_move_state.dir = false; break

#define MAP_MOVE_SELECT_STATE_TRUE(key, dir)    \
    case key: {                                 \
        if (g_models.has_select_model_state()){ \
            g_select_move_state.dir = true;     \
        }                                       \
    } break

#define MAP_MOVE_SELECT_STATE_FALSE(key, dir) case key: g_select_move_state.dir = false; break
    
#define KEY_BLOCK(key, expr)                    \
  case key: {                                   \
      if (keydown_if_not(key)) {                \
          expr;                                 \
      }                                         \
  } break

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;

            KEY_BLOCK(GLFW_KEY_F1,
                      g_cam_orient.active = !g_cam_orient.active;
                      maybe_enable_cursor(window));

            KEY_BLOCK(GLFW_KEY_N,
                      g_models.new_sphere());

            KEY_BLOCK(GLFW_KEY_F2,
                      g_frame.screenshot());
            
            MAP_MOVE_STATE_TRUE(GLFW_KEY_W, front);
            MAP_MOVE_STATE_TRUE(GLFW_KEY_S, back);
            MAP_MOVE_STATE_TRUE(GLFW_KEY_A, left);
            MAP_MOVE_STATE_TRUE(GLFW_KEY_D, right);
            MAP_MOVE_STATE_TRUE(GLFW_KEY_SPACE, up);
            MAP_MOVE_STATE_TRUE(GLFW_KEY_LEFT_SHIFT, down);

            // These select movement key binds will eventually
            // be replaced with a more user friendly movement scheme involving
            // mouse drag selection. For  now, the goal is simply to
            // have a driver that allows for ease of testing.
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_UP, front);
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_DOWN, back);
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_RIGHT, right);
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_LEFT, left);
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_RIGHT_SHIFT, up);
            MAP_MOVE_SELECT_STATE_TRUE(GLFW_KEY_RIGHT_CONTROL, down);
            
        default:
            keydown_if_not(key);
            break;
        }
    }
    else if (action == GLFW_RELEASE) {
        switch (key) {                        
            
            MAP_MOVE_STATE_FALSE(GLFW_KEY_W, front);
            MAP_MOVE_STATE_FALSE(GLFW_KEY_S, back);
            MAP_MOVE_STATE_FALSE(GLFW_KEY_A, left);
            MAP_MOVE_STATE_FALSE(GLFW_KEY_D, right);
            MAP_MOVE_STATE_FALSE(GLFW_KEY_SPACE, up);
            MAP_MOVE_STATE_FALSE(GLFW_KEY_LEFT_SHIFT, down);

            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_UP, front);
            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_DOWN, back);
            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_RIGHT, right);
            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_LEFT, left);
            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_RIGHT_SHIFT, up);
            MAP_MOVE_SELECT_STATE_FALSE(GLFW_KEY_RIGHT_CONTROL, down);

        default:
            g_key_states[key] = false;          
            break;
        }
    }

#undef MAP_MOVE_STATE_TRUE
#undef MAP_MOVE_STATE_FALSE
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (g_cam_orient.active) {
        double scale = 0.01;

        g_cam_orient.dx = xpos - g_cam_orient.prev_xpos;
        g_cam_orient.dy = g_cam_orient.prev_ypos - ypos;

        g_cam_orient.dx *= scale;
        g_cam_orient.dy *= scale;

        g_cam_orient.prev_xpos = xpos;
        g_cam_orient.prev_ypos = ypos;

        mat4_t xRot = glm::rotate(mat4_t(1.0f), static_cast<real_t>(g_cam_orient.dy), vec3_t(1.0f, 0.0f, 0.0f));
        mat4_t yRot = glm::rotate(mat4_t(1.0f), static_cast<real_t>(g_cam_orient.dx), vec3_t(0.0f, 1.0f, 0.0f));

        g_view.orient = mat3_t(yRot * xRot) * g_view.orient;
    } else {
        g_cam_orient.prev_xpos = xpos;
        g_cam_orient.prev_ypos = ypos;
    }
}

struct click_state {
    enum click_mode {
        mode_nothing = 0,
        mode_select
    };

    click_mode mode{mode_select};

private:
    static const int32_t DEPTH_SCAN_XY_RANGE = 5;

public:
    
    //
    // cast_ray - used for mouse picking
    //
    // Creates an inverse transform, designed to map coordinates in screen_space back to worldspace. The coordinates
    // being mapped are the current coordinates of the mouse cursor.
    //
    // A raycast is used to determine whether or not an intersection has occurred
    //
    // See https://www.glfw.org/docs/latest/input_guide.html#cursor_pos:
    // The documentation states that the screen coordinates are relative to the upper left
    // of the window's content area (not the entire desktop/viewing area). So, we don't have to perform any additional calculations
    // on the mouse coordinates themselves.
    
    vec3_t screen_out(real_t zplane) const {
     //   vec4_t ndcplane = g_view.proj * g_view.view() * glm::vec4(0.0f, 0.0f, -zplane, 1.0f);

        int32_t x_offset = 0;
        int32_t y_offset = 0;
        
        real_t depth{};

        // This will read from GL_BACK by default
        GL_FN(glReadPixels(static_cast<int32_t>(g_cam_orient.prev_xpos),
                           static_cast<int32_t>(g_cam_orient.prev_ypos),
                           1, 1,
                           GL_DEPTH_COMPONENT, OPENGL_REAL,
                           &depth));
        
        vec4_t mouse( R(g_cam_orient.prev_xpos) + R(x_offset), 
                      R(g_cam_orient.prev_ypos) + R(y_offset),
                      -depth,
                      R(1.0));

        // TODO: cache screen_sp and clip_to_ndc, since they're essentially static data
        mat4_t screen_sp{R(1.0)};
        screen_sp[0][0] = R(g_view.view_width);
        screen_sp[1][1] = R(g_view.view_height);

        // OpenGL viewport origin is lower left, GLFW's is upper left.
        mouse.y = screen_sp[1][1] - mouse.y;

        mouse.x /= screen_sp[0][0];
        mouse.y /= screen_sp[1][1];

          // In screen space the range is is [0, width] and [0, height],
        // so after normalization we're in [0, 1].
        // NDC is in [-1, 1], so we scale from [0, 1] to [-1, 1],
        // to complete the NDC transform
        mouse.x = R(2.0) * mouse.x - R(1.0);
        mouse.y = R(2.0) * mouse.y - R(1.0);

        std::cout << AS_STRING_SS(depth) SEP_SS AS_STRING_SS(x_offset) SEP_SS AS_STRING_SS(y_offset)
                  << std::endl;

        return glm::inverse(g_view.proj * g_view.view()) * mouse;
    }
    
    bool cast_ray(models::index_type model) const {
        vec3_t nearp = screen_out(g_view.nearp);
        geom::ray world_raycast{}; 
        world_raycast.dir = glm::normalize(nearp - g_view.position);
        world_raycast.orig = g_view.position;

        auto bvol = g_models.bound_volumes[model];

        bool success = g_geom.test_ray_sphere(world_raycast, bvol);
        
        if (success) {
            std::cout << "HIT\n";
            g_models.set_select_model_state(model);
        } else {
            std::cout << "NO HIT\n";
            g_models.clear_select_model_state();
        }
        
        std::cout << AS_STRING_GLM_SS(nearp) << "\n";
        std::cout << AS_STRING_GLM_SS(world_raycast.orig) << "\n";
        std::cout << AS_STRING_GLM_SS(world_raycast.dir) << "\n";

        std::cout << std::endl;

        return success;
    }

    void scan_object_selection() const {
        models::index_list_type filtered =
            g_models.select([](const models::index_type& id) -> bool {
                    return g_models.type(id) == models::model_sphere;
                });
        
        for (auto id: filtered) {
            if (cast_ray(id)) {
                break;
            }
        }
    }
} g_click_state;

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mmods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        switch (g_click_state.mode) {
        case click_state::mode_select: {
            g_click_state.scan_object_selection();
        }break;
        }
    }
}

int main(void) {
    g_key_states.fill(false);

    g_programs.registermod();
    g_textures.registermod();

    GLFWwindow* window;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OpenGL Boilerplate", NULL, NULL);
    if (!window) {
        goto error;
    }

    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;

    {
        GLenum r = glewInit();
        if (r != GLEW_OK) {
            printf("Glew ERROR: %s\n", glewGetErrorString(r));
            goto error;
        }
    }

    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    maybe_enable_cursor(window);

    init_api_data();

    while (!glfwWindowShouldClose(window)) {
        g_view(g_cam_move_state);

        if (g_models.has_select_model_state()) {
            g_models.update_select_model_state();
        }
        
        render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (auto module : g_modules) {
        module->free_mem();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
error:
    glfwTerminate();
    exit(EXIT_FAILURE);
}
