#define ENABLE_TECHTREE
#define SOKOL_IMPL
#if defined(_MSC_VER)
#define SOKOL_D3D11
#define SOKOL_LOG(str) OutputDebugStringA(str)
#elif defined(__EMSCRIPTEN__)
#define SOKOL_GLES2
#elif defined(__APPLE__)
// NOTE: on macOS, sokol.c is compiled explicitly as ObjC 
#define SOKOL_METAL
#else
#define SOKOL_GLCORE33
#endif

#ifndef __GNUC__
#define __attribute__(unused)
#endif

#define SOKOL_WIN32_FORCE_MAIN
#include "sokol/sokol_app.h"
#define CUTE_PNG_IMPLEMENTATION
#include "cute_png.h"
#include "sokol/sokol_time.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define PREMULTIPLIED_BLEND ((sg_blend_state) {          \
  .enabled = true,                                       \
  .src_factor_rgb = SG_BLENDFACTOR_ONE,                  \
  .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,  \
  .src_factor_alpha = SG_BLENDFACTOR_ONE,                \
  .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,\
})

/* corresponds to 16ms of elapsed frame time */
typedef uint64_t Tick;

#include <math.h>
#include "math.h"
#include "fio.h"
#include "obj.h"

#include "input.h"

#include "build/shaders.glsl.h"
#include "overlay.h"

//Forward declaration of types for use in function arguments
typedef struct Ent Ent;
typedef struct GenDex GenDex;

//The list of enums/states will be getting bigger as we add more entities,
//so move it to its own file
#include "ai_type.h"

/* EntProps enable codepaths for game entities.
   These aren't boolean fields because that makes it more difficult to deal with
   them dynamically; this way you can have a function which gives you all Ents
   with a certain property within a certain distance of a certain point in space,
   for example. */
typedef enum {
  /* Prevents the Ent's memory from being reused, enables all other codepaths. */
  EntProp_Active,

  /* A bit of motion makes asteroids & co. feel more alive */
  EntProp_PassiveRotate,

  /* Projectiles disappear on collision and are exempt from friction */
  EntProp_Projectile,
  
  /* Allows entities to lose health and be destroyed */
  EntProp_Destructible,

  /* Drags this entity toward the player, and makes it disappear when it gets there */
  EntProp_PickUp,

  /* runs the ai statemachine on the entity*/
  EntProp_HasAI,

  /* split into smaller and smaller asteroids and eventually minerals upon death */
  EntProp_AsteroidSplit,

  /* Knowing how many EntProps there are facilitates allocating just enough memory */
  EntProp_COUNT,
} EntProp;

typedef enum { Art_Ship, Art_Asteroid, Art_Plane, Art_Pillar, Art_Laser, Art_Mineral, Art_COUNT } Art;

typedef enum { Shape_Circle, Shape_Line } Shape;
typedef struct {
  Shape shape;
  float size, weight;
} Collider;

struct GenDex{
  uint64_t generation;
  size_t index;
};

typedef struct {
  int tick;
  const AI_state *state;
  GenDex target;
  uint64_t tick_end;
} AI;

typedef struct {
  uint64_t bundled[(EntProp_COUNT + 63) / 64];
} EntPropBundle;

/* A game entity. Usually, it is rendered somewhere and has some sort of dynamic behavior */
struct Ent {
  /* packed into 64 bit sections for alignment */
  EntPropBundle props;

  uint64_t generation;

  Vec2 pos, vel;
  float height;
  Vec3 scale;

  float angle;
  float x_rot;

  /* used for animations */
  Tick last_collision;

  /* tied to EntProp_PassiveRotate */
  Vec3 passive_rotate_axis;

  /* tied to EntProp_PickUp */
  Tick pick_up_after_tick;

  /* tied to EntProp_Destructible */
  int32_t health, max_hp;
  Tick last_hit;

  int32_t damage;

  /* Currently used for projectile source */
  GenDex parent;

  Art art;
  float bloom, transparency;

  Collider collider;
  AI ai;
};

static inline bool bundle_has_prop(EntPropBundle *bdl, EntProp prop) {
  return !!(bdl->bundled[prop/64] & ((uint64_t)1 << (prop%64)));
}
static inline EntPropBundle bundle_prop(EntProp prop, EntPropBundle bdl) {
  bdl.bundled[prop/64] |= (uint64_t)1 << (prop%64);
  return bdl;
}
static inline EntPropBundle unbundle_prop(EntProp prop, EntPropBundle bdl) {
  bdl.bundled[prop/64] &= ~((uint64_t)1 << (prop%64));
  return bdl;
}
static inline EntPropBundle new_bundle(EntProp prop) {
  return bundle_prop(prop, (EntPropBundle){0});
}

static inline bool has_ent_prop(Ent *ent, EntProp prop) {
  return bundle_has_prop(&ent->props, prop);
}
static inline bool take_ent_prop(Ent *ent, EntProp prop) {
  bool before = has_ent_prop(ent, prop);
  ent->props = unbundle_prop(prop, ent->props);
  return before;
}
static inline bool give_ent_prop(Ent *ent, EntProp prop) {
  bool before = has_ent_prop(ent, prop);
  ent->props = bundle_prop(prop, ent->props);
  return before;
}

typedef enum { Shader_Standard, Shader_Laser, Shader_ForceField, Shader_COUNT } Shader;

typedef struct {
  sg_buffer ibuf, vbuf;
  sg_image texture;
  Shader shader;
  size_t id;
  size_t index_count;
} Mesh;

typedef struct {
  /* the index of the actual Ent this CamEnt corresponds to */
  size_t index;
  /* its distance from the camera */
  float cam_dist;
} CamEnt;

/* Function to be passed to qsort to aid in sorting entities
 * based on their distance from the camera */
static int cam_ent_cmp(const void *av, const void *bv) {
  float af = ((CamEnt *) av)->cam_dist;
  float bf = ((CamEnt *) bv)->cam_dist;

  return (af < bf) - (af > bf);
}

#define OFFSCREEN_SAMPLE_COUNT (4)
#define STATE_MAX_ENTS (1 << 12)
#define BLUR_PASSES (4)
typedef struct State {
  bool paused;
  float pause_anim;
  const char *pause_message;
  sg_pipeline pip[Shader_COUNT];
  Mesh meshes[Art_COUNT];
  Ent ents[STATE_MAX_ENTS];
  CamEnt cam_ents[STATE_MAX_ENTS];
  GenDex player;
  float player_turn_accel;
  size_t gem_count;
  uint64_t frame; /* a sokol_time tick, not one of our game ticks */
  Tick tick;
  double fixed_tick_accumulator;
  struct {
    sg_image color_img, bright_img, depth_img;
    sg_pass pass;
  } offscreen;
  struct {
    sg_buffer quad_vbuf;
    sg_pipeline pip;
  } fsq;
  struct {
    sg_image imgs[BLUR_PASSES][2];
    sg_pass passes[BLUR_PASSES][2];
    sg_pipeline pip;
  } blur;
} State;
static State *state;

#include "ui.h"
#include "graphview.h"
graphview_State test_graph___ = { 0 };

static GenDex get_gendex(Ent *ent) {
  // C pointer arithmetic is weird,
  // If you subtract a pointer from a pointer
  // You get an aligned difference, but if the difference is misaligned, UB!!!
  ptrdiff_t offs = (char*)ent-(char*)state->ents;
  assert((offs >= 0 && (size_t)offs < sizeof(state->ents)) && "Ent is not present in system");
  assert((size_t)offs % sizeof(Ent) == 0 && "Invalid ent alignment");
  return (GenDex) { .generation = ent->generation, .index = (size_t)(ent-state->ents)+1 };
}

/* returns NULL if gendex is stale */
static Ent* try_gendex(GenDex gd) {
  /* Gendex is 1-indexed, this allows for tricks with C zero-init */
  if(gd.index == 0)
    return NULL;
  return (state->ents[gd.index-1].generation == gd.generation) ? &state->ents[gd.index-1] : NULL;
}

/* Calling this function will find some unoccupied memory for an Ent if available, 
   returning NULL otherwise. If the memory is located, it will be assigned to the
   argument, given the EntProp_Active, and a pointer to this memory will be returned. */
static Ent *add_ent(Ent ent) {
  for (int i = 0; i < STATE_MAX_ENTS; i++) {
    Ent *slot = state->ents + i;
    if (!has_ent_prop(slot, EntProp_Active)) {
      uint64_t gen = slot->generation;
      *slot = ent;
      slot->generation = gen;
      give_ent_prop(slot, EntProp_Active);
      return slot;
    }
  }
  return NULL;
}

/* ends `count` copies of Ent off into different directions */
static void split_into(int count, Ent ent) {
  float offset = randf() * PI_f;
  for (int i = 0; i < count; i++) {
    float t = (float)i / (float)count;
    Vec2 dir = vec2_rot(randf() * 0.3f + offset + t * PI_f * 2.0f);
    ent.vel = mul2_f(dir, 0.2f);
    ent.pos = add2(ent.pos, mul2_f(dir, 2.0f * ent.collider.size));
    add_ent(ent);
  }
}

static void remove_ent(Ent *ent) {
  if (has_ent_prop(ent, EntProp_AsteroidSplit)) {
    if (ent->collider.size > 0.5f) {
      ent->collider.weight -= 0.3f;
      ent->collider.size -= 0.3f;
      ent->scale = sub3_f(ent->scale, 0.3f);
      ent->health = 1;
      ent->passive_rotate_axis = rand3();
      split_into(2, *ent);
    } else {
      split_into(2, (Ent) {
        .props = new_bundle(EntProp_PickUp),
        .pick_up_after_tick = state->tick + 10,
        .art = Art_Mineral,
        .pos = ent->pos,
        .bloom = 0.35f,
      });
    }
  }
  ent->generation++;
  take_ent_prop(ent,EntProp_Active);
}

/* Use this function to iterate over all of the Ents in the game.
   ex:
        for (Ent *e = 0; e = ent_all_iter(e); )
            draw_ent(e);
*/
static inline Ent *ent_all_iter(Ent *ent) {
  ent = ent ? (ent+1) : state->ents;
  for (; ent < state->ents + STATE_MAX_ENTS; ent++)
    if (has_ent_prop(ent, EntProp_Active)) return ent;
  return NULL;
}

static void fire_laser(Ent *ent) {
  Vec2 e_dir = vec2_swap(vec2_rot(ent->angle));
  add_ent((Ent) {
    .props = new_bundle(EntProp_Projectile),
    .art = Art_Laser,
    .bloom = 1.0,
    .pos = add2(ent->pos, mul2_f(e_dir, 2.5f)),
    .vel = add2(ent->vel, mul2_f(e_dir, 0.8f)),
    .scale = vec3(1.0f, 1.0f, 3.0f),
    .angle = ent->angle,
    .height = -0.8f,
    .collider.size = 0.2f,
    .collider.weight = 1.0f,
    .damage = ent->damage,
    .parent = get_gendex(ent),
  });
}

ol_Image gem_image;

#include "build.h"
#include "collision.h"
#include "player.h"
#include "ai.h"
#include "savestate.h"
#include "generated_tree.h"

void load_texture(Art art, const char *texture) {
  cp_image_t player_png = cp_load_png(texture);
  cp_flip_image_horizontal(&player_png);
  int w = player_png.w;
  int h = player_png.h;
  if (art == Art_Ship || art == Art_Pillar)
    state->meshes[art].texture = sg_make_image_with_mipmaps(&(sg_image_desc){
      .width = w,
      .height = h,
      .pixel_format = SG_PIXELFORMAT_RGBA8,
      .num_mipmaps = 10,
      .min_filter = SG_FILTER_LINEAR_MIPMAP_LINEAR,
      .mag_filter = SG_FILTER_LINEAR,
      .max_anisotropy = 8,
      .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
      .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
      .data.subimage[0][0] = (sg_range){ player_png.pix, (size_t)(w*h)*sizeof(cp_pixel_t) } ,
    });
  else 
    state->meshes[art].texture = sg_make_image(&(sg_image_desc){
      .width = w,
      .height = h,
      .max_anisotropy = 8,
      .min_filter = SG_FILTER_LINEAR,
      .mag_filter = SG_FILTER_LINEAR,
      .data.subimage[0][0] = (sg_range){ player_png.pix, (size_t)(w*h)*sizeof(cp_pixel_t) } ,
    });
  cp_free_png(&player_png);
}

void load_mesh(Art art, Shader shader, const char *path, const char *texture) {
  char *input = fio_read_text(path);
  if (input == NULL) {
    fprintf(stderr, "Could not load asset %s, file inaccessible\n", path);
    exit(1);
  }
  obj_Result res = obj_parse(input);
  size_t vertex_count;
  obj_Unrolled unrolled = obj_unroll_pun(&res, &vertex_count);
  obj_dispose(&res);
  

  /* a vertex buffer */
  sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
    .data = (sg_range){unrolled.vertices, vertex_count*8*sizeof(float)},
  });
  sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
    .type = SG_BUFFERTYPE_INDEXBUFFER,
    .data = (sg_range){unrolled.indices, res.index_count*sizeof(uint16_t)},
  });

  state->meshes[art] = (Mesh) {
    .ibuf = ibuf,
    .vbuf = vbuf, 
    .id = art,
    .shader = shader,
    .index_count = res.index_count,
  };

  if (texture)
    load_texture(art, texture);

  obj_dispose_unrolled(&unrolled);
  free((void*)input);
}

void resize_framebuffers(void) {
  /* destroy previous resource (can be called for invalid id) */
  sg_destroy_pass(state->offscreen.pass);
  sg_destroy_image(state->offscreen.color_img);
  sg_destroy_image(state->offscreen.depth_img);
  sg_destroy_image(state->offscreen.bright_img);
  for (int i = 0; i < BLUR_PASSES; i++)
    for (int h = 0; h < 2; h++) {
      sg_destroy_image(state->blur.imgs[i][h]);
      sg_destroy_pass(state->blur.passes[i][h]);
    }

  /* a render pass with one color- and one depth-attachment image */
  sg_image_desc img_desc = {
    .render_target = true,
    .width = sapp_width(),
    .height = sapp_height(),
    .pixel_format = SG_PIXELFORMAT_RGBA8,
    .min_filter = SG_FILTER_LINEAR,
    .mag_filter = SG_FILTER_LINEAR,
    .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
    .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    .sample_count = OFFSCREEN_SAMPLE_COUNT,
  };
  state->offscreen.color_img = sg_make_image(&img_desc);
  state->offscreen.bright_img = sg_make_image(&img_desc);

  sg_image_desc blur_img_desc = img_desc;
  for (int i = 0; i < BLUR_PASSES; i++)
    for (int h = 0; h < 2; h++) {
      state->blur.imgs[i][h] = sg_make_image(&blur_img_desc);
      state->blur.passes[i][h] = sg_make_pass(&(sg_pass_desc) {
        .color_attachments[0].image = state->blur.imgs[i][h]
      });
      blur_img_desc.width /= 2;
      blur_img_desc.height /= 2;
    }

  img_desc.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
  state->offscreen.depth_img = sg_make_image(&img_desc);
  state->offscreen.pass = sg_make_pass(&(sg_pass_desc){
    .color_attachments[0].image = state->offscreen.color_img,
    .color_attachments[1].image = state->offscreen.bright_img,
    .depth_stencil_attachment.image = state->offscreen.depth_img,
    .label = "offscreen-pass"
  });
}

void init(void) {
  seed_rand(9, 12, 32, 10);

  state = calloc(sizeof(State), 1);

  state->player = get_gendex(add_ent((Ent) {
    .art = Art_Ship,
    .props = new_bundle(EntProp_Destructible),
    .pos = { -1, 2.5 },
    .collider.size = 2.0f,
    .collider.weight = 0.4f,
    .health = 10,
    .max_hp = 10,
    .damage = 1,
  }));

  add_ent((Ent) {
    .art = Art_Plane,
    .pos = { -1.5, 6.5 },
    .scale = { 5.0f, 4.0f, 1.0f },
    .height = -1.0f,
    .collider.size = 10.0f,
    .collider.shape = Shape_Line,
    .collider.weight = 1000.0f,
  });

  for (int i = -1; i < 2; i += 2)
    add_ent((Ent) {
      .props = new_bundle(EntProp_PassiveRotate),
      .art = Art_Pillar,
      .pos = { -1.5f + (float)i * 4.2f, 6.5f },
      .height = 0.0,
      .x_rot = 0,
      .passive_rotate_axis = vec3_y,
    });

  #define ASTEROIDS_PER_RING (7)
  #define ASTEROID_RINGS (3)
  #define ASTEROID_RING_SPACING (20.0f)
  #define ASTEROID_FIRST_RING_START_DIST (12.0f)
  #define ASTEROID_DIST_RANDOMIZER (3.0f)
  for (int r = 0; r < ASTEROID_RINGS; r++)
    for (int i = 0; i < ASTEROIDS_PER_RING; i++) {
      float dist = ASTEROID_FIRST_RING_START_DIST;
      dist += (float)r*ASTEROID_RING_SPACING;

      float t = (float)i / (float)ASTEROIDS_PER_RING * PI_f * 2.0f;
      t += randf() * (PI_f * 2.0f * dist) / (float) ASTEROIDS_PER_RING * 0.8f;

      dist += ASTEROID_DIST_RANDOMIZER * (0.5f - randf()) * 2.0f;

      float size = 1.0f + (float) r * (0.1f + 0.2f * randf());
      add_ent((Ent) {
        .props = bundle_prop(EntProp_AsteroidSplit,
                 bundle_prop(EntProp_PassiveRotate,
                  new_bundle(EntProp_Destructible))),
        .art = Art_Asteroid,
        .pos = mul2_f(vec2_rot(t), dist),
        .scale = vec3_f(size),
        .collider.size = size,
        .collider.weight = size,
        .passive_rotate_axis = rand3(),
        .health = 1,
        .max_hp = 1,
      });
    }

  stm_setup();
  sg_setup(&(sg_desc){
    .context = sapp_sgcontext()
  });
  Ent *en = add_ent((Ent) {
    .art = Art_Ship,
    .pos = { -1, 12.5 },
    .collider.size = 2.0f,
    .collider.weight = 0.4f,
    .health = 3,
    .max_hp = 3,
    .damage = 1,
  });
  give_ent_prop(en,EntProp_HasAI);
  give_ent_prop(en,EntProp_Destructible);
  ai_init(en,AI_STATE_IDLE);
  en = add_ent((Ent) {
    .art = Art_Ship,
    .pos = { -10, 12.5 },
    .collider.size = 2.0f,
    .collider.weight = 0.4f,
    .health = 3,
    .max_hp = 3,
    .damage = 1,
  });
  give_ent_prop(en,EntProp_HasAI);
  give_ent_prop(en,EntProp_Destructible);
  ai_init(en,AI_STATE_IDLE);


  load_mesh(    Art_Ship,   Shader_Standard,     "./Bob.obj", "./Bob_Orange.png");
  load_mesh(Art_Asteroid,   Shader_Standard,"./Asteroid.obj",       "./Moon.png");
  load_mesh(   Art_Plane, Shader_ForceField,   "./Plane.obj",               NULL);
  load_mesh(   Art_Laser,      Shader_Laser,   "./LASER.obj",    "./Mineral.png");
  load_mesh( Art_Mineral,   Shader_Standard, "./Mineral.obj",    "./Mineral.png");
  load_mesh(  Art_Pillar,   Shader_Standard,  "./Pillar.obj",     "./Pillar.png");

  ui_init();
  ol_init();

  test_graph___.atlas = ol_load_image("./techtree.png");

  /* a pipeline state object */
  sg_pipeline_desc desc = {
    .layout = {
      .attrs = {
        [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
        [ATTR_vs_uv].format       = SG_VERTEXFORMAT_FLOAT2,
        [ATTR_vs_normal].format   = SG_VERTEXFORMAT_FLOAT3,
      }
    },
    .index_type = SG_INDEXTYPE_UINT16,
    .cull_mode = SG_CULLMODE_BACK,
    .depth = {
      .write_enabled = true,
      .pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL,
      .compare = SG_COMPAREFUNC_LESS_EQUAL,
    },
    .color_count = 2,
    .colors[0].blend = PREMULTIPLIED_BLEND,
  }; 

  desc.shader = sg_make_shader(mesh_shader_desc(sg_query_backend()));
  state->pip[Shader_Standard] = sg_make_pipeline(&desc);

  desc.shader = sg_make_shader(laser_shader_desc(sg_query_backend()));
  state->pip[Shader_Laser] = sg_make_pipeline(&desc);

  desc.shader = sg_make_shader(force_field_shader_desc(sg_query_backend()));
  state->pip[Shader_ForceField] = sg_make_pipeline(&desc);

  gem_image = ol_load_image("./Gem.png");

  /* a vertex buffer to render a fullscreen rectangle */
  state->fsq.quad_vbuf = sg_make_buffer(&(sg_buffer_desc){
    .data = SG_RANGE(((float[]) { 0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f })),
    .label = "quad vertices"
  });

  /* create pipeline object */
  state->fsq.pip = sg_make_pipeline(&(sg_pipeline_desc){
    .layout = {
      .attrs[ATTR_fsq_vs_pos].format = SG_VERTEXFORMAT_FLOAT2
    },
    .shader = sg_make_shader(fsq_shader_desc(sg_query_backend())),
    .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    .label = "fullscreen quad pipeline"
  });

  state->blur.pip = sg_make_pipeline(&(sg_pipeline_desc){
    .layout = {
      .attrs[ATTR_fsq_vs_pos].format = SG_VERTEXFORMAT_FLOAT2
    },
    .shader = sg_make_shader(blur_shader_desc(sg_query_backend())),
    .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    .depth.pixel_format = SG_PIXELFORMAT_NONE,
    .label = "blur pipeline"
  });

  resize_framebuffers();
}

static float ent_health_frac(Ent *ent) {
  float t = (float)(state->tick - ent->last_hit) / 30.0f;
  return lerp((float)(ent->health+1), (float)ent->health, fminf(1, t)) / fmaxf((float)ent->max_hp, 1);
}

static Mat4 ent_model_mat(Ent *ent) {
  Mat4 m = translate4x4(vec3(ent->pos.x, ent->height, ent->pos.y));

  m = mul4x4(m, y_rotate4x4(ent->angle));
  if (ent->x_rot != 0.0f)
    m = mul4x4(m, x_rotate4x4(ent->x_rot));
  if (ent == try_gendex(state->player))
    m = mul4x4(m, z_rotate4x4(state->player_turn_accel*-2.0f));
  if (has_ent_prop(ent, EntProp_PassiveRotate))
    m = mul4x4(m, rotate4x4(ent->passive_rotate_axis,
                            (float)state->tick/70.0f));

  Vec3 scale = (magmag3(ent->scale) == 0.0f) ? vec3_f(1.0f) : ent->scale;
  if (ent->art == Art_Ship) scale = mul3_f(scale, 0.3f);
  m = mul4x4(m, scale4x4(scale));

  return m;
}

/* naively renders with n draw calls per entity
 * TODO: optimize for fewer draw calls */
static void draw_ent_internal(Mat4 vp, Ent *ent) {
  Mat4 m = ent_model_mat(ent);

  Mesh *mesh = state->meshes + ent->art;

  /* set up bindings for this mesh */
  sg_bindings bind = {
    .index_buffer = mesh->ibuf,
    .vertex_buffers[0] = mesh->vbuf,
  };
  if (mesh->texture.id)
    bind.fs_images[SLOT_tex] = mesh->texture;
  sg_apply_bindings(&bind);

  /* upload uniforms appropriate for this mesh */
  if (mesh->shader == Shader_ForceField) {
    force_field_fs_params_t fs_params = {
      .stretch = { ent->scale.y, ent->scale.x },
      .time = (float)(state->tick - ent->last_collision) / 600.0f,
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_force_field_fs_params, &SG_RANGE(fs_params));
  } else if (mesh->shader == Shader_Standard) {
    mesh_fs_params_t fs_params = {
      .bloom = ent->bloom,
      .transparency = (ent->transparency == 0.0f) ? 1.0f : ent->transparency,
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_mesh_fs_params, &SG_RANGE(fs_params));
  }
  vs_params_t vs_params = { .view_proj = vp, .model = m };
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));

  sg_draw(0, (int)mesh->index_count, 1);
}

static void draw_ent(Mat4 vp, Ent *ent) {
  if (ent->art == Art_Pillar) {
    Ent copy = *ent;
    copy.height = 2;
    draw_ent_internal(vp, &copy);
    copy.x_rot = PI_f;
    copy.height = -4;
    copy.angle = -copy.angle;
    draw_ent_internal(vp, &copy);
  }
  else
    draw_ent_internal(vp, ent);
}

static void tick(void) {
  if (state->paused) return;
  state->tick++;

  Ent *player = try_gendex(state->player);
  if(player!=NULL)
    player_update(player);

  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
    if(has_ent_prop(ent,EntProp_HasAI))
      ai_run(ent);

    if(has_ent_prop(ent, EntProp_Destructible)&&ent->health <= 0) {
      remove_ent(ent);
      //TODO: handle loot and asteroid splitting
    }
  }

  for (Ent *ent = 0; (ent = ent_all_iter(ent));)
    collision(ent);
  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
    collision_movement_update(ent);

    if (has_ent_prop(ent, EntProp_PickUp)) {
      ent->height = sinf(ent->pos.x + ent->pos.y + (float) state->tick / 14.0f) * 0.3f;
      Ent *p = try_gendex(state->player);
      if (p!=NULL&&ent->pick_up_after_tick <= state->tick) {

        #define SUCK_DIST (6.0f)
        Vec2 delta = sub2(p->pos, ent->pos);
        float dist = mag2(delta);
        

        if (dist < 0.3f) {
          state->gem_count += 1;
          take_ent_prop(ent, EntProp_Active);
        }
        if (dist < 0.3f)
          remove_ent(ent);
        else if (dist < SUCK_DIST)
          ent->pos = add2(
            ent->pos,
            mul2_f(norm2(delta), (SUCK_DIST - dist) / 20.0f)
          );
        #undef SUCK_DIST
      }
    }
  }
}

static void frame(void) {
  #define TICK_MS (1000.0f / 60.0f)
  double elapsed = stm_ms(stm_laptime(&state->frame));
  state->fixed_tick_accumulator += elapsed;
  while (state->fixed_tick_accumulator > TICK_MS) {
    state->fixed_tick_accumulator -= TICK_MS;
    tick();
  }

  const float w = sapp_widthf();
  const float h = sapp_heightf();
  Mat4 proj = perspective4x4(1.047f, w/h, 0.01f, 100.0f);

  static float cam_angle = 0.0f;
  cam_angle = lerp(cam_angle, try_gendex(state->player)->angle, 0.08f);
  Vec2 cam_dir = vec2_swap(vec2_rot(cam_angle));

  Vec3 plr_p, cam_o, cam_eye;
  plr_p = (Vec3) {try_gendex(state->player)->pos.x, 0.0f, try_gendex(state->player)->pos.y};
  cam_o = (Vec3) {           cam_dir.x, 1.0f,          cam_dir.y};
  Mat4 view = look_at4x4(
    cam_eye = add3(plr_p, mul3(vec3(-5,  3, -5), cam_o)),
              add3(plr_p, mul3(vec3( 5,  0,  5), cam_o)),
    vec3_y
  );

  Mat4 vp = mul4x4(proj, view);

  sg_begin_pass(state->offscreen.pass, &(sg_pass_action) {
    .colors[0] = {
      .action = SG_ACTION_CLEAR,
      .value = { 0.25f/20.0f, 0.25f/20.0f, 0.75f/20.0f, 1.0f }
    },
    .colors[1] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
  });


  int cam_ent_count = 0;
  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
    Vec3 p = { ent->pos.x, ent->height, ent->pos.y };
    state->cam_ents[cam_ent_count++] = (CamEnt) {
      .index = ent - state->ents,
      .cam_dist = magmag3(sub3(p, cam_eye)),
    };
  }
  qsort(state->cam_ents, cam_ent_count, sizeof(CamEnt), cam_ent_cmp);

  Shader shd = -1;
  for (int i = 0; i < cam_ent_count; i++) {
    Ent *ent = state->ents + state->cam_ents[i].index;
    Shader ent_shd = state->meshes[ent->art].shader;
    if (ent_shd != shd) {
      shd = ent_shd;
      sg_apply_pipeline(state->pip[shd]);
    }
    draw_ent(vp, ent);
  }

  build_update((float)elapsed/1000.0f);

  float plr_hp = 0.0;
  Ent *plr = try_gendex(state->player);
  if (plr) plr_hp = ent_health_frac(plr);

  build_draw_3d(vp);

  ol_begin();
  ui_setmousepos((int)_input_mouse_x, (int)_input_mouse_y);

  
 
  ui_screen((int)w, (int)h);
    ui_screen_anchor_xy(0.5f, 0.5f);
    _ui_text("Hello, folks!");
    ui_screen_anchor_xy(0.98f, 0.0f);
    ui_column(250, 0);
      ui_healthbar(250, 50, plr_hp, ui_HealthbarShape_Fancy);

      // Measure out the row
      ui_measuremode();
        ui_row(0, 0);
          ui_textf("%d", state->gem_count);
          ui_image_ratio(&gem_image, 0.5, 0.5);
        ol_Rect size = ui_row_end();
      ui_end_measuremode();

      ui_screen(ui_rel_x(1.0), 32);
        ui_screen_anchor_xy(1.0, 0);
        // Finally render it
        ui_row(size.w, 32);
          ui_textf("%d", state->gem_count);
          ui_image_ratio(&gem_image, 0.5, 0.5);
        ui_row_end();
      ui_screen_end();
    ui_column_end();
    ui_screen_anchor_xy(0.02f, 0.02f);
    ui_textf("FPS: %.0lf", round(1000/elapsed));
    // Pause menu
      ui_screen_anchor_xy(0.0, 0.5);
      ui_column(0, 200);
        ui_setoffset((int)easeinout(-300, 20.0f, fmaxf(fminf(state->pause_anim+0.4f, 1.0f), 0.0f)), 0);
        if (ui_button("Resume")) {
          state->pause_message = NULL;
          state->pause_anim = fmaxf(fminf(state->pause_anim, 1.0f), 0.0f);
          state->paused = false;
        }
        ui_gap(10);
        ui_setoffset((int)easeinout(-300, 20.0f, fmaxf(fminf(state->pause_anim+0.2f, 1.0f), 0.0f)), 0);
        if (ui_button("Save")) {
          state->pause_message = "Saved into savestate.sav";
          if (!savestate_save()) {
            state->pause_message = "Failed to save to savestate.sav";
          }
        }
        ui_gap(10);
        ui_setoffset((int)easeinout(-300, 20.0f, fmaxf(fminf(state->pause_anim+0.1f, 1.0f), 0.0f)), 0);
        if (ui_button("Load")) {
          state->pause_message = "Loaded savestate.sav";
          if (!savestate_load()) {
            state->pause_message = "Failed to load savestate.sav";
          }
        }
        ui_gap(10);
        ui_setoffset((int)easeinout(-300, 20.0f, fmaxf(fminf(state->pause_anim, 1.0f), 0.0f)), 0);
        if (ui_button("Exit")) {
          sapp_request_quit();
        }
        ui_setoffset(0, 0);
      ui_column_end();
      ui_screen_anchor_xy(0.02f, 0.98f);
      if (state->pause_message) {
        ui_text(state->pause_message);
      }
    if (state->paused) {
      state->pause_anim += (float)elapsed/1000.0f;
    }
    else {
      state->pause_anim -= (float)elapsed/1000.0f;
    }
  ui_screen_end();


  build_draw();

#ifdef ENABLE_TECHTREE
  treeview_generate_techtree(&test_graph___);
  graphview_draw(&test_graph___);
#endif

  ui_render();
  for (Ent *ent = 0; (ent = ent_all_iter(ent));)
    if (has_ent_prop(ent,EntProp_HasAI) && ent->health < ent->max_hp) {
      Vec4 v = mul4x44(mul4x4(vp, ent_model_mat(ent)), vec4(0, -4, 0, 1));
      v = div4_f(v, v.w);
      v.x = (v.x + 1.0f)/2.0f * sapp_widthf();
      v.y = (v.y + 1.0f)/2.0f * sapp_heightf();
      v.y = sapp_heightf() - v.y;
      ui_render_healthbar((ol_Rect) {
        .x = (int)roundf(v.x) - 35,
        .y = (int)roundf(v.y) - 7,
        .w = 70,
        .h = 14,
      }, ent_health_frac(ent), ui_HealthbarShape_Minimal);
    }
  ui_end_pass();

  sg_end_pass();

  for (int i = 0; i < BLUR_PASSES; i++)
    for (int hori = 0; hori < 2; hori++) {
      sg_begin_pass(state->blur.passes[i][hori], &(sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_LOAD }
      });
      sg_apply_pipeline(state->blur.pip);
      sg_apply_bindings(&(sg_bindings) {
        .vertex_buffers[0] = state->fsq.quad_vbuf,
        .fs_images = {
          [SLOT_tex] = hori ? state->blur.imgs[i][!hori] : state->offscreen.bright_img
        }
      });
      blur_fs_params_t fs_params = { .hori = vec2((float)hori, (float)!hori) };
      sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_blur_fs_params, &SG_RANGE(fs_params));
      sg_draw(0, 4, 1);
      sg_end_pass();
    }

  sg_pass_action pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
  };
  sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
  sg_apply_pipeline(state->fsq.pip);
  sg_apply_bindings(&(sg_bindings) {
    .vertex_buffers[0] = state->fsq.quad_vbuf,
    .fs_images = {
      [SLOT_tex] = state->offscreen.color_img,
      [SLOT_blur0] = state->blur.imgs[0][1],
      [SLOT_blur1] = state->blur.imgs[1][1],
      [SLOT_blur2] = state->blur.imgs[2][1],
      [SLOT_blur3] = state->blur.imgs[3][1],
    }
  });
  sg_draw(0, 4, 1);
  sg_end_pass();

  sg_commit();

  input_update();
}

static void cleanup(void) {
  for (size_t i = 0; i < Art_COUNT; i += 1) 
    sg_destroy_image(state->meshes[i].texture);
  free(state);
  ol_unload_image(&gem_image);
  ui_deinit();
  sg_shutdown();
}

static void event(const sapp_event *ev) {
#ifdef ENABLE_TECHTREE
  graphview_process_events(&test_graph___, ev);
#endif
  if (ui_event(ev)) return;
  if (!state->paused && build_event(ev)) return;
  switch (ev->type) {
    case SAPP_EVENTTYPE_KEY_DOWN: {
      if (ev->key_code == SAPP_KEYCODE_BACKSPACE && ui_erase())
        break;
      input_key_update(ev->key_code,1);
      if (ev->key_code == SAPP_KEYCODE_ESCAPE)
        build_leave();
      if (ev->key_code == SAPP_KEYCODE_ESCAPE && !_build_state.appearing) {
        state->pause_message = NULL;
        state->pause_anim = fmaxf(fminf(state->pause_anim, 1.0f), 0.0f);
        state->paused = !state->paused;
      }
    } break;
    case SAPP_EVENTTYPE_CHAR: {
    } break;
    case SAPP_EVENTTYPE_MOUSE_MOVE: {
      _input_mouse_x = ev->mouse_x;
      _input_mouse_y = ev->mouse_y;
    } break;
    case SAPP_EVENTTYPE_KEY_UP: {
      input_key_update(ev->key_code,0);
    } break;
    case SAPP_EVENTTYPE_MOUSE_DOWN: {
      input_mouse_update(ev->mouse_button,1);
    } break;
    case SAPP_EVENTTYPE_MOUSE_UP: {
      input_mouse_update(ev->mouse_button,0);
    } break;
    case SAPP_EVENTTYPE_RESIZED: {
      resize_framebuffers();
    } break;
    default:;
  }
}

sapp_desc sokol_main(int argc, char* argv[]) {
  (void)argc; (void)argv;
  return (sapp_desc){
    .init_cb = init,
    .frame_cb = frame,
    .cleanup_cb = cleanup,
    .event_cb = event,
    .width = 1280,
    .height = 720,
    .sample_count = OFFSCREEN_SAMPLE_COUNT,
    .gl_force_gles2 = true,
    .window_title = "Space Game (collab4mb)",
    .icon.sokol_default = true,
  };
}
