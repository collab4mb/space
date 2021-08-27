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

  /* Knowing how many EntProps there are facilitates allocating just enough memory */
  EntProp_COUNT,
} EntProp;

typedef enum { Art_Ship, Art_Asteroid, Art_COUNT } Art;
/* probably a better way to do this? we'll see */
static Mat4 art_tweaks[Art_COUNT] = {
  [Art_Ship] = {{
    0.3f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.3f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.3f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
  }},
  [Art_Asteroid] = {{
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
  }}
};

/* A game entity. Usually, it is rendered somewhere and has some sort of dynamic behavior */
typedef struct {
  /* packed into 64 bit sections for alignment */
  uint64_t props[(EntProp_COUNT + 63) / 64];

  Vec2 pos, vel;

  float angle;

  /* tied to EntProp_PassiveRotate */
  Vec3 passive_rotate_axis;
  float passive_rotate_angle;

  Art art;

  float size, weight;
  float scale_delta;
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

#define STATE_MAX_ENTS (1 << 12)
typedef struct {
  sg_pass_action pass_action;
  sg_pipeline pip;
  sg_bindings bind;
  Ent ents[STATE_MAX_ENTS];
  Ent *player;
  float player_turn_accel;
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

static struct {
  sg_buffer ibuf, vbuf;
  sg_image texture;
  size_t id;
  size_t index_count;
} meshes[Art_COUNT] = { 0 };

void load_mesh(const char *path, const char *texture, Art art) {
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
  state->bind.vertex_buffers[0] = vbuf;

  sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
    .type = SG_BUFFERTYPE_INDEXBUFFER,
    .data = (sg_range){unrolled.indices, res.index_count*sizeof(uint16_t)},
  });


  cp_image_t player_png = cp_load_png(texture);
  cp_flip_image_horizontal(&player_png);
  state->bind.index_buffer = ibuf;
  int w = player_png.w;
  int h = player_png.h;
  if (art == Art_Ship)
    meshes[art].texture = sg_make_image_with_mipmaps(&(sg_image_desc){
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
    meshes[art].texture = sg_make_image(&(sg_image_desc){
      .width = w,
      .height = h,
      .max_anisotropy = 8,
      .min_filter = SG_FILTER_LINEAR,
      .mag_filter = SG_FILTER_LINEAR,
      .data.subimage[0][0] = (sg_range){ player_png.pix, w*h*sizeof(cp_pixel_t) } ,
    });
  cp_free_png(&player_png);
  meshes[art].ibuf = ibuf; meshes[art].vbuf = vbuf; 
  meshes[art].id = art; meshes[art].index_count = res.index_count;
  obj_dispose_unrolled(&unrolled);
  free((void*)input);
}

void init(void) {
  seed_rand(9, 12, 32, 10);

  state = calloc(sizeof(State), 1);

  state->player = add_ent((Ent) {
    .art = Art_Ship,
    .pos = { -1, 2.5 },
    .size = 2.0f,
    .weight = 0.4f,
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
        .scale_delta = -r * (0.1f + 0.2f * randf()),
        .size = 1.0f,
        .weight = 1.0f,
      });
      ast->passive_rotate_axis = rand3();
      give_ent_prop(ast, EntProp_PassiveRotate);
    }

  sg_setup(&(sg_desc){
    .context = sapp_sgcontext()
  });


  load_mesh("./Bob.obj", "./Bob_Orange.png", Art_Ship);
  load_mesh("./Asteroid.obj", "./Moon.png", Art_Asteroid);
 

  /* a shader use separate shader sources here */
  sg_shader shd = sg_make_shader(mesh_shader_desc(sg_query_backend()));

  ol_init();

  /* a pipeline state object */
  state->pip = sg_make_pipeline(&(sg_pipeline_desc){
    .layout = {
      .attrs = {
        [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
        [ATTR_vs_uv].format      = SG_VERTEXFORMAT_FLOAT2,
        [ATTR_vs_normal].format   = SG_VERTEXFORMAT_FLOAT3,
      }
    },
    .shader = shd,
    .index_type = SG_INDEXTYPE_UINT16,
    .cull_mode = SG_CULLMODE_BACK,
    .depth = {
      .write_enabled = true,
      .compare = SG_COMPAREFUNC_LESS_EQUAL,
    },
  }); 
}

static void draw_mesh(Mat4 vp, Mat4 model, Art art) {
  state->bind.fs_images[SLOT_tex] = meshes[art].texture;
  state->bind.index_buffer = meshes[art].ibuf;
  state->bind.vertex_buffers[0] = meshes[art].vbuf;
  sg_apply_bindings(&state->bind);
  vs_params_t vs_params = { .view_proj = vp, .model = model };
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
  sg_draw(0, meshes[art].index_count, 1);
}


static void frame(void) {
  player_update(state->player);
  for (Ent *ent = 0; (ent = ent_all_iter(ent));)
    collision(ent);
  for (Ent *ent = 0; (ent = ent_all_iter(ent));)
    collision_movement_update(ent);

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

  sg_apply_pipeline(state->pip);
  /* naively render with n draw calls per entity
   * TODO: optimize for fewer draw calls */
  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
    Mat4 m = translate4x4(vec3(ent->pos.x, 0.0f, ent->pos.y));

    m = mul4x4(m, y_rotate4x4(ent->angle));
    if (ent == state->player)
      m = mul4x4(m, z_rotate4x4(state->player_turn_accel*-2.0f));
    if (has_ent_prop(ent, EntProp_PassiveRotate))
      m = mul4x4(m, rotate4x4(ent->passive_rotate_axis,
                              ent->passive_rotate_angle += 0.01));

    m = mul4x4(m, scale4x4(vec3_f(1.0f+ent->scale_delta)));

    m = mul4x4(m, art_tweaks[ent->art]);
    draw_mesh(vp, m, ent->art);
  }

  ol_begin();
  char *txt = 
    "..#..#.."
    "..#..#.."
    "..#..#.."
    "#......#"
    ".######.";
  
  size_t l = strlen(txt);

  for (size_t i = 0; i < l; i += 1)
    if (txt[i] == '#')
      ol_draw_rect(vec4(1.0, i/(float)l, 0.0, 1.0), (i%8)*10, (i/8)*10, 10, 10);

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
