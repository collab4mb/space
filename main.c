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

#define SOKOL_WIN32_FORCE_MAIN
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"

#include <math.h>
#include "math.h"
#include "fio.h"
#include "obj.h"

#include "input.h"

#include "build/shaders.glsl.h"

/* EntProps enable codepaths for game entities.
   These aren't boolean fields because that makes it more difficult to deal with
   them dynamically; this way you can have a function which gives you all Ents
   with a certain property within a certain distance of a certain point in space,
   for example. */
typedef enum {
  /* Prevents the Ent's memory from being reused, enables all other codepaths. */
  EntProp_Active,

  /* Knowing how many EntProps there are facilitates allocating just enough memory */
  EntProp_COUNT,
} EntProp;

typedef enum { Art_Ship, Art_Asteroid, Art_COUNT } Art;

/* A game entity. Usually, it is rendered somewhere and has some sort of dynamic behavior */
typedef struct {
  /* packed into 64 bit sections for alignment */
  uint64_t props[(EntProp_COUNT + 63) / 64];

  Vec2 pos;
  Art art;
  float angle;
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

static struct {
  sg_buffer ibuf, vbuf;
  size_t id;
  size_t index_count;
} meshes[Art_COUNT] = { 0 };

void load_mesh(const char *path, Art art) {
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
  state->bind.index_buffer = ibuf;
  meshes[art].ibuf = ibuf; meshes[art].vbuf = vbuf; 
  meshes[art].id = art; meshes[art].index_count = res.index_count;
  obj_dispose_unrolled(&unrolled);
  free((void*)input);
}

void init(void) {
  state = calloc(sizeof(State), 1);

  state->player = add_ent((Ent) { .art = Art_Ship, .pos = {  0,  2.5 } });
  add_ent((Ent) { .art = Art_Asteroid, .pos = {  2, -3.0 } });
  add_ent((Ent) { .art = Art_Asteroid, .pos = {  5, -2.0 } });
  add_ent((Ent) { .art = Art_Asteroid, .pos = { -3, -4.0 } });


  sg_setup(&(sg_desc){
    .context = sapp_sgcontext()
  });

  load_mesh("./Bob.obj", Art_Ship);
  load_mesh("./Asteroid.obj", Art_Asteroid);
 

  /* a shader (use separate shader sources here */
  sg_shader shd = sg_make_shader(cube_shader_desc(sg_query_backend()));

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
  state->bind.index_buffer = meshes[art].ibuf;
  state->bind.vertex_buffers[0] = meshes[art].vbuf;
  sg_apply_bindings(&state->bind);
  vs_params_t vs_params = { .mvp = mul4x4(vp, model) };
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
  sg_draw(0, meshes[art].index_count, 1);
}


static void frame(void) {
  //Player input
  if(input_key_down(SAPP_KEYCODE_LEFT))
    state->player->angle-=0.05f;
  if(input_key_down(SAPP_KEYCODE_RIGHT))
    state->player->angle+=0.05f;
  Vec2 p_dir = vec2_rot(state->player->angle);
  float t = p_dir.x;
  p_dir.x = p_dir.y;
  p_dir.y = t;
  if(input_key_down(SAPP_KEYCODE_UP))
  {
    state->player->pos.x+=p_dir.x*0.2f;
    state->player->pos.y+=p_dir.y*0.2f;
  }
  if(input_key_down(SAPP_KEYCODE_DOWN))
  {
    state->player->pos.x-=p_dir.x*0.2f;
    state->player->pos.y-=p_dir.y*0.2f;
  }

  const float w = sapp_widthf();
  const float h = sapp_heightf();
  Mat4 proj = perspective4x4(1.047f, w/h, 0.01f, 50.0f);
  Mat4 view = look_at4x4(vec3(state->player->pos.x-p_dir.x*5.0f, 5.0f,state->player->pos.y-p_dir.y*10.0f), vec3(state->player->pos.x, 0.0f, state->player->pos.y), vec3(0.0f, 1.0f, 0.0f));
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
    m = mul4x4(m, rotate4x4(vec3_y, ent->angle));
    draw_mesh(vp, m, ent->art);
  }
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
