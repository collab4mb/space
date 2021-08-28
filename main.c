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
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <math.h>
#include "math.h"
#include "fio.h"
#include "obj.h"

#include "input.h"

#include "build/shaders.glsl.h"
#include "overlay.h"

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

  /* Knowing how many EntProps there are facilitates allocating just enough memory */
  EntProp_COUNT,
} EntProp;

typedef enum { Art_Ship, Art_Asteroid, Art_Plane, Art_Mineral, Art_COUNT } Art;

typedef enum { Shape_Circle, Shape_Line } Shape;
typedef struct {
  Shape shape;
  float size, weight;
} Collider;

/* A game entity. Usually, it is rendered somewhere and has some sort of dynamic behavior */
typedef struct {
  /* packed into 64 bit sections for alignment */
  uint64_t props[(EntProp_COUNT + 63) / 64];

  Vec2 pos, vel;
  float height;
  Vec3 scale;

  float angle;

  /* used for animations */
  float time_since_last_collision;

  /* tied to EntProp_PassiveRotate */
  Vec3 passive_rotate_axis;
  float passive_rotate_angle;

  /* tied to EntProp_PickUp */
  uint64_t pick_up_after_tick;

  Art art;

  Collider collider;
} Ent;
static inline bool has_ent_prop(Ent *ent, EntProp prop) {
  return !!(ent->props[prop/64] & ((uint64_t)1 << (prop%64)));
}
static inline bool take_ent_prop(Ent *ent, EntProp prop) {
  bool before = has_ent_prop(ent, prop);
  ent->props[prop/64] &= ~((uint64_t)1 << (prop%64));
  return before;
}
static inline bool give_ent_prop(Ent *ent, EntProp prop) {
  bool before = has_ent_prop(ent, prop);
  ent->props[prop/64] |= (uint64_t)1 << (prop%64);
  return before;
}

typedef enum { Shader_Standard, Shader_ForceField, Shader_COUNT } Shader;

typedef struct {
  sg_buffer ibuf, vbuf;
  sg_image texture;
  Shader shader;
  size_t id;
  size_t index_count;
} Mesh;

#define STATE_MAX_ENTS (1 << 12)
typedef struct {
  sg_pass_action pass_action;
  sg_pipeline pip[Shader_COUNT];
  Mesh meshes[Art_COUNT];
  Ent ents[STATE_MAX_ENTS];
  Ent *player;
  float player_turn_accel;
  uint64_t tick;
} State;
static State *state;

/* Calling this function will find some unoccupied memory for an Ent if available, 
   returning NULL otherwise. If the memory is located, it will be assigned to the
   argument, given the EntProp_Active, and a pointer to this memory will be returned. */
static Ent *add_ent(Ent ent) {
  for (int i = 0; i < STATE_MAX_ENTS; i++) {
    Ent *slot = state->ents + i;
    if (!has_ent_prop(slot, EntProp_Active)) {
      *slot = ent;
      give_ent_prop(slot, EntProp_Active);
      return slot;
    }
  }
  return NULL;
}

/* Use this function to iterate over all of the Ents in the game.
   ex:
        for (Ent *e = 0; e = ent_all_iter(e); )
            draw_ent(e);
*/
static inline Ent *ent_all_iter(Ent *ent) {
  if (ent == NULL) ent = state->ents - 1;

  while (!has_ent_prop((++ent), EntProp_Active))
    if ((ent - state->ents) == STATE_MAX_ENTS)
      return NULL;

  if (has_ent_prop(ent, EntProp_Active)) return ent;
  return NULL;
}

#include "collision.h"
#include "player.h"

ol_Image test_image;
ol_Font test_font;

void load_texture(Art art, const char *texture) {
  cp_image_t player_png = cp_load_png(texture);
  cp_flip_image_horizontal(&player_png);
  int w = player_png.w;
  int h = player_png.h;
  if (art == Art_Ship)
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
      .data.subimage[0][0] = (sg_range){ player_png.pix,w*h*sizeof(cp_pixel_t) } ,
    });
  else 
    state->meshes[art].texture = sg_make_image(&(sg_image_desc){
      .width = w,
      .height = h,
      .max_anisotropy = 8,
      .min_filter = SG_FILTER_LINEAR,
      .mag_filter = SG_FILTER_LINEAR,
      .data.subimage[0][0] = (sg_range){ player_png.pix, w*h*sizeof(cp_pixel_t) } ,
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

void init(void) {
  seed_rand(9, 12, 32, 10);

  state = calloc(sizeof(State), 1);

  state->player = add_ent((Ent) {
    .art = Art_Ship,
    .pos = { -1, 2.5 },
    .collider.size = 2.0f,
    .collider.weight = 0.4f,
  });

  add_ent((Ent) {
    .art = Art_Plane,
    .pos = { -1.5, 6.5 },
    .scale = { 5.0f, 3.0f, 1.0f },
    .height = -1.0f,
    .collider.size = 9.0f,
    .collider.shape = Shape_Line,
    .collider.weight = 1000.0f,
  });

  #define ASTEROIDS_PER_RING (7)
  #define ASTEROID_RINGS (3)
  #define ASTEROID_RING_SPACING (20.0f)
  #define ASTEROID_FIRST_RING_START_DIST (12.0f)
  #define ASTEROID_DIST_RANDOMIZER (3.0f)
  for (int r = 0; r < ASTEROID_RINGS; r++)
    for (int i = 0; i < ASTEROIDS_PER_RING; i++) {
      float dist = ASTEROID_FIRST_RING_START_DIST;
      dist += r*ASTEROID_RING_SPACING;

      float t = (float)i / (float)ASTEROIDS_PER_RING * PI_f * 2.0f;
      t += randf() * (PI_f * 2.0f * dist) / (float) ASTEROIDS_PER_RING * 0.8;

      dist += ASTEROID_DIST_RANDOMIZER * (0.5f - randf()) * 2.0f;

      Ent *ast = add_ent((Ent) {
        .art = Art_Asteroid,
        .pos = mul2_f(vec2_rot(t), dist),
        .scale = vec3_f(1.0 + r * (0.1f + 0.2f * randf())),
        .collider.size = 1.0f,
        .collider.weight = 1.0f,
        .passive_rotate_axis = rand3(),
      });
      give_ent_prop(ast, EntProp_PassiveRotate);
      give_ent_prop(ast, EntProp_Destructible);
    }

  sg_setup(&(sg_desc){
    .context = sapp_sgcontext()
  });


  load_mesh(    Art_Ship,   Shader_Standard,     "./Bob.obj", "./Bob_Orange.png");
  load_mesh(Art_Asteroid,   Shader_Standard,"./Asteroid.obj",       "./Moon.png");
  load_mesh(   Art_Plane, Shader_ForceField,   "./Plane.obj",               NULL);
  load_mesh( Art_Mineral,   Shader_Standard, "./Mineral.obj",    "./Mineral.png");

  test_font = ol_load_font("./Orbitron-Regular.ttf");


  ol_init();

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
      .compare = SG_COMPAREFUNC_LESS_EQUAL,
    },
    .colors[0].blend = (sg_blend_state) {
      .enabled = true,
      .src_factor_rgb = SG_BLENDFACTOR_ONE, 
      .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, 
      .src_factor_alpha = SG_BLENDFACTOR_ONE, 
      .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    },
  }; 

  desc.shader = sg_make_shader(mesh_shader_desc(sg_query_backend()));
  state->pip[Shader_Standard] = sg_make_pipeline(&desc);

  desc.shader = sg_make_shader(force_field_shader_desc(sg_query_backend()));
  state->pip[Shader_ForceField] = sg_make_pipeline(&desc);

  test_image = ol_load_image("./test_tex.png");
}

/* naively renders with n draw calls per entity
 * TODO: optimize for fewer draw calls */
static void draw_ent(Mat4 vp, Ent *ent) {
  Mat4 m = translate4x4(vec3(ent->pos.x, ent->height, ent->pos.y));

  m = mul4x4(m, y_rotate4x4(ent->angle));
  if (ent == state->player)
    m = mul4x4(m, z_rotate4x4(state->player_turn_accel*-2.0f));
  if (has_ent_prop(ent, EntProp_PassiveRotate))
    m = mul4x4(m, rotate4x4(ent->passive_rotate_axis,
                            ent->passive_rotate_angle += 0.01f));

  Vec3 scale = (magmag3(ent->scale) == 0.0f) ? vec3_f(1.0f) : ent->scale;
  if (ent->art == Art_Ship) scale = mul3_f(scale, 0.3f);
  m = mul4x4(m, scale4x4(scale));

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
      .time = ent->time_since_last_collision,
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_force_field_fs_params, &SG_RANGE(fs_params));
  }
  vs_params_t vs_params = { .view_proj = vp, .model = m };
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));

  sg_draw(0, mesh->index_count, 1);
}


static void frame(void) {
  state->tick++;

  player_update(state->player);
  for (Ent *ent = 0; (ent = ent_all_iter(ent));)
    collision(ent);
  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
    collision_movement_update(ent);

    if (has_ent_prop(ent, EntProp_PickUp)) {
      ent->height = sinf(ent->pos.x + ent->pos.y + (float) state->tick / 14.0) * 0.3f;
      if (ent->pick_up_after_tick <= state->tick) {
        Ent *p = state->player;

        #define SUCK_DIST (6.0f)
        Vec2 delta = sub2(p->pos, ent->pos);
        float dist = mag2(delta);

        if (dist < 0.3f)
          take_ent_prop(ent, EntProp_Active);
        else if (dist < SUCK_DIST)
          ent->pos = add2(
            ent->pos,
            mul2_f(norm2(delta), (SUCK_DIST - dist) / 20.0f)
          );
      }
    }
  }

  const float w = sapp_widthf();
  const float h = sapp_heightf();
  Mat4 proj = perspective4x4(1.047f, w/h, 0.01f, 100.0f);

  static float cam_angle = 0.0f;
  cam_angle = lerp(cam_angle, state->player->angle, 0.08);
  Vec2 cam_dir = vec2_swap(vec2_rot(cam_angle));

  Vec3 plr_p = {state->player->pos.x, 0.0f, state->player->pos.y};
  Vec3 cam_o = {           cam_dir.x, 1.0f,          cam_dir.y};
  Mat4 view = look_at4x4(
    add3(plr_p, mul3(vec3(-5,  3, -5), cam_o)),
    add3(plr_p, mul3(vec3( 5,  0,  5), cam_o)),
    vec3_y
  );

  Mat4 vp = mul4x4(proj, view);

  sg_pass_action pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f/20.0, 0.25f/20.0, 0.75f/20.0, 1.0f } }
  };
  sg_begin_default_pass(&pass_action, (int)w, (int)h);

  for (Shader shd = 0; shd < Shader_COUNT; shd++) {
    sg_apply_pipeline(state->pip[shd]);
    for (Ent *ent = 0; (ent = ent_all_iter(ent));)
      if (state->meshes[ent->art].shader == shd)
        draw_ent(vp, ent);
  }

  ol_begin();

  const char *text = "To begin, approach an asteroid and press [SPACE]";
  ol_Rect rect = ol_measure_text(&test_font, text, 20, 30);
  rect.x -= 40;
  rect.y -= 40;
  rect.w += 80;
  rect.h += 80;
  rect.x += 40;
  rect.y += 40;
  ol_draw_rect(vec4(0.1, 0.2, 0.3, 0.4), rect);
  rect.x += 10;
  rect.y += 10;
  rect.w -= 20;
  rect.h -= 20;
  ol_draw_rect(vec4(0.1, 0.2, 0.3, 0.4), rect);
  ol_draw_text(&test_font, text, 60, 70, vec4(0.4, 0.8, 1.0, 1.0));

  sg_end_pass();
  sg_commit();

  input_update();
}

static void cleanup(void) {
  sg_shutdown();
}

static void event(const sapp_event *ev) {
  switch (ev->type) {
    case SAPP_EVENTTYPE_KEY_DOWN: {
        input_key_update(ev->key_code,1);
      #ifndef NDEBUG
        if (ev->key_code == SAPP_KEYCODE_ESCAPE)
          sapp_request_quit();
      #endif
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
    .width = 800,
    .height = 600,
    .sample_count = 4,
    .gl_force_gles2 = true,
    .window_title = "Space Game (collab4mb)",
    .icon.sokol_default = true,
  };
}
