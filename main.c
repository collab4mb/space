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

#include <math.h>
#include "math.h"
#include "fio.h"
#include "obj.h"

#include "input.h"

#include "build/shaders.glsl.h"
#include "overlay.h"

#include "ui.h"
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

  /* Knowing how many EntProps there are facilitates allocating just enough memory */
  EntProp_COUNT,
} EntProp;

typedef enum { Art_Ship, Art_Asteroid, Art_Plane, Art_Laser, Art_Mineral, Art_COUNT } Art;

typedef enum { Shape_Circle, Shape_Line } Shape;
typedef struct {
  Shape shape;
  float size, weight;
} Collider;

typedef struct {
  uint64_t generation;
  struct Ent *index;
}GenDex;

typedef struct {
  int tick;
  const AI_state *state;
  AI_type type;
  GenDex target;
  uint64_t tick_end;
}AI;

/* A game entity. Usually, it is rendered somewhere and has some sort of dynamic behavior */
typedef struct Ent{
  /* packed into 64 bit sections for alignment */
  uint64_t props[(EntProp_COUNT + 63) / 64];

  uint64_t generation;

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
  float bloom;

  Collider collider;
  AI ai;
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

static GenDex get_gendex(Ent *ent) {
  return (GenDex) { .generation = ent->generation, .index = ent };
}

/* returns NULL if gendex is stale */
static Ent* try_gendex(GenDex gd) {
  return (gd.index->generation == gd.generation) ? gd.index : NULL;
}

typedef enum { Shader_Standard, Shader_Laser, Shader_ForceField, Shader_COUNT } Shader;

typedef struct {
  sg_buffer ibuf, vbuf;
  sg_image texture;
  Shader shader;
  size_t id;
  size_t index_count;
} Mesh;

#define OFFSCREEN_SAMPLE_COUNT (4)
#define STATE_MAX_ENTS (1 << 12)
typedef struct {
  sg_pipeline pip[Shader_COUNT];
  Mesh meshes[Art_COUNT];
  Ent ents[STATE_MAX_ENTS];
  Ent *player;
  float player_turn_accel;
  size_t gem_count;
  uint64_t frame, tick;
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
    sg_image imgs[2];
    sg_pass passes[2];
    sg_pipeline pip;
  } blur;
} State;
static State *state;

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

static void remove_ent(Ent *ent) {
  ent->generation++;
  take_ent_prop(ent,EntProp_Active);
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
#include "ai.h"

ol_Image test_image;
ol_Image healthbar_image;
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

void resize_framebuffers(void) {
  /* destroy previous resource (can be called for invalid id) */
  sg_destroy_pass(state->offscreen.pass);
  sg_destroy_image(state->offscreen.color_img);
  sg_destroy_image(state->offscreen.depth_img);
  sg_destroy_image(state->offscreen.bright_img);
  for (int i = 0; i < 2; i++) {
    sg_destroy_image(state->blur.imgs[i]);
    sg_destroy_pass(state->blur.passes[i]);
  }

  /* a render pass with one color- and one depth-attachment image */
  sg_image_desc img_desc = {
    .render_target = true,
    .width = sapp_widthf(),
    .height = sapp_heightf(),
    .pixel_format = SG_PIXELFORMAT_RGBA8,
    .min_filter = SG_FILTER_LINEAR,
    .mag_filter = SG_FILTER_LINEAR,
    .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
    .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    .sample_count = OFFSCREEN_SAMPLE_COUNT,
  };
  state->offscreen.color_img = sg_make_image(&img_desc);
  state->offscreen.bright_img = sg_make_image(&img_desc);

  for (int i = 0; i < 2; i++) {
    state->blur.imgs[i] = sg_make_image(&img_desc);
    state->blur.passes[i] = sg_make_pass(&(sg_pass_desc) {
      .color_attachments[0].image = state->blur.imgs[i]
    });
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

  stm_setup();
  sg_setup(&(sg_desc){
    .context = sapp_sgcontext()
  });

  Ent *en = add_ent((Ent) {
    .art = Art_Ship,
    .pos = { -1, 12.5 },
    .collider.size = 2.0f,
    .collider.weight = 0.4f,
  });
  give_ent_prop(en,EntProp_HasAI);
  ai_init(en,AI_TYPE_DSHIP);
  en = add_ent((Ent) {
    .art = Art_Ship,
    .pos = { -10, 12.5 },
    .collider.size = 2.0f,
    .collider.weight = 0.4f,
  });
  give_ent_prop(en,EntProp_HasAI);
  ai_init(en,AI_TYPE_DSHIP);


  load_mesh(    Art_Ship,   Shader_Standard,     "./Bob.obj", "./Bob_Orange.png");
  load_mesh(Art_Asteroid,   Shader_Standard,"./Asteroid.obj",       "./Moon.png");
  load_mesh(   Art_Plane, Shader_ForceField,   "./Plane.obj",               NULL);
  load_mesh(   Art_Laser,      Shader_Laser,   "./LASER.obj",    "./Mineral.png");
  load_mesh( Art_Mineral,   Shader_Standard, "./Mineral.obj",    "./Mineral.png");

  test_font = ol_load_font("./Orbitron-Regular.ttf");
  ui_init(&test_font);


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
      .pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL,
      .compare = SG_COMPAREFUNC_LESS_EQUAL,
    },
    .color_count = 2,
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

  desc.shader = sg_make_shader(laser_shader_desc(sg_query_backend()));
  state->pip[Shader_Laser] = sg_make_pipeline(&desc);

  desc.shader = sg_make_shader(force_field_shader_desc(sg_query_backend()));
  state->pip[Shader_ForceField] = sg_make_pipeline(&desc);

  test_image = ol_load_image("./Gem.png");
  healthbar_image = ol_load_image("./healthbar.png");

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
  } else {
    mesh_fs_params_t fs_params = { .bloom = 0.0f }; // ent->bloom };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_mesh_fs_params, &SG_RANGE(fs_params));
  }
  vs_params_t vs_params = { .view_proj = vp, .model = m };
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));


  sg_draw(0, mesh->index_count, 1);
}

static void tick(void) {
  state->tick++;

  player_update(state->player);
  for (Ent *ent = 0; (ent = ent_all_iter(ent));)
    if(has_ent_prop(ent,EntProp_HasAI))
      ai_run(ent);
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

  sg_begin_pass(state->offscreen.pass, &(sg_pass_action) {
    .colors[0] = {
      .action = SG_ACTION_CLEAR,
      .value = { 0.25f/20.0, 0.25f/20.0, 0.75f/20.0, 1.0f }
    },
    .colors[1] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
  });

  for (Shader shd = 0; shd < Shader_COUNT; shd++) {
    sg_apply_pipeline(state->pip[shd]);
    for (Ent *ent = 0; (ent = ent_all_iter(ent));)
      if (state->meshes[ent->art].shader == shd)
        draw_ent(vp, ent);
  }

  ol_begin();
 
  ui_screen((int)w, (int)h);
    ui_screen_anchor_xy(0.98, 0.02);
    ui_column(healthbar_image.width, 0);
      ui_screen(0, healthbar_image.height/2);
        ui_image_part(&healthbar_image, (ol_Rect) { 0, healthbar_image.height/2, healthbar_image.width/1.5, healthbar_image.height/2 });
        ui_image_part(&healthbar_image, (ol_Rect) { 0, 0, healthbar_image.width, healthbar_image.height/2 });
      ui_screen_end();

      // Measure out the row
      ui_measuremode();
        ui_row(0, 0);
          ui_textf("%d", state->gem_count);
          ui_image_ratio(&test_image, 0.5, 0.5);
        ol_Rect size = ui_row_end();
      ui_end_measuremode();

      ui_screen(ui_rel_x(1.0), 32);
        ui_screen_anchor_xy(1.0, 0);
        // Finally render it
        ui_row(size.w, 32);
          ui_textf("%d", state->gem_count);
          ui_image_ratio(&test_image, 0.5, 0.5);
        ui_row_end();
      ui_screen_end();
    ui_column_end();
    ui_screen_anchor_xy(0.02, 0.02);
    ui_textf("FPS: %.0lf", round(1000/elapsed));
  ui_screen_end();

  for (ui_Command *cmd = ui_command_next(); cmd != NULL; cmd = ui_command_next()) {
    switch (cmd->kind) {
      case Ui_Cmd_Text:
        ol_draw_text(&test_font, cmd->data.text.text, cmd->rect.x, cmd->rect.y, vec4(1.0, 1.0, 1.0, 1.0));
      break;
      case Ui_Cmd_Image:
        ol_draw_tex_part(cmd->data.image.img, cmd->rect, cmd->data.image.part);
      break;
      default: {
        assert(false);
      }
    }
  }
  ui_end_pass();

  sg_end_pass();

  int horizontal = 1;
  for (int i = 0; i < 2; i++) {
    sg_begin_pass(state->blur.passes[horizontal], &(sg_pass_action) {
      .colors[0] = { .action = SG_ACTION_LOAD }
    });
    sg_apply_pipeline(state->blur.pip);
    sg_apply_bindings(&(sg_bindings) {
      .vertex_buffers[0] = state->fsq.quad_vbuf,
      .fs_images = {
        [SLOT_tex] = i ? state->blur.imgs[!horizontal] : state->offscreen.bright_img
      }
    });
    blur_fs_params_t fs_params = { .hori = vec2(horizontal, !horizontal) };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_blur_fs_params, &SG_RANGE(fs_params));
    sg_draw(0, 4, 1);
    sg_end_pass();
    horizontal = !horizontal;
  }

  sg_pass_action pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
  };
  sg_begin_default_pass(&pass_action, (int)w, (int)h);
  sg_apply_pipeline(state->fsq.pip);
  sg_apply_bindings(&(sg_bindings) {
    .vertex_buffers[0] = state->fsq.quad_vbuf,
    .fs_images = {
      [SLOT_tex] = state->offscreen.color_img,
      [SLOT_bloomed] = state->blur.imgs[!horizontal]
    }
  });
  sg_draw(0, 4, 1);
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
    .width = 800,
    .height = 600,
    .sample_count = OFFSCREEN_SAMPLE_COUNT,
    .gl_force_gles2 = true,
    .window_title = "Space Game (collab4mb)",
    .icon.sokol_default = true,
  };
}
