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

#include "build/shaders.glsl.h"

static struct {
   float rx, ry;
  sg_pass_action pass_action;
  sg_pipeline pip;
  sg_bindings bind;
} state;

static size_t _indices = 0;
void init(void) {
  sg_setup(&(sg_desc){
    .context = sapp_sgcontext()
  });
 
  char *input = fio_read_text("./Cube.obj");

  obj_Result res = obj_parse(input);
  size_t vertex_count;
  _indices = res.index_count;
  obj_Unrolled unrolled = obj_unroll_pun(&res, &vertex_count);
  obj_dispose(&res);
  

  /* a vertex buffer */
  sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
    .data = (sg_range){unrolled.vertices, vertex_count*8*sizeof(float)},
    .label = "cube-vertices"
  });
  state.bind.vertex_buffers[0] = vbuf;

  sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
    .type = SG_BUFFERTYPE_INDEXBUFFER,
    .data = (sg_range){unrolled.indices, _indices*sizeof(uint16_t)},
    .label = "cube-indices"
  });
  obj_dispose_unrolled(&unrolled);
  free((void*)input);
  exit(-1);
  state.bind.index_buffer = ibuf;

  /* a shader (use separate shader sources here */
  //sg_shader shd = sg_make_shader(quad_shader_desc(sg_query_backend()));
  sg_shader shd = sg_make_shader(cube_shader_desc(sg_query_backend()));

  /* a pipeline state object */
  state.pip = sg_make_pipeline(&(sg_pipeline_desc){
    .layout = {
      .attrs = {
        [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
        [ATTR_vs_uv].format      = SG_VERTEXFORMAT_FLOAT2,
        [ATTR_vs_normal].format   = SG_VERTEXFORMAT_FLOAT3,
      }
    },
    .shader = shd,
    .index_type = SG_INDEXTYPE_UINT16,
    // .cull_mode = SG_CULLMODE_BACK,
    .depth = {
      .write_enabled = true,
      .compare = SG_COMPAREFUNC_LESS_EQUAL,
    },
    .label = "cube-pipeline"
  }); 

  /* default pass action */
  state.bind = (sg_bindings) {
    .vertex_buffers[0] = vbuf,
    .index_buffer = ibuf
  };
  state.pass_action = (sg_pass_action) {
    .colors[0] = { .action=SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
  };
}

void frame(void) {
  vs_params_t vs_params;
  const float w = sapp_widthf();
  const float h = sapp_heightf();
  Mat4 proj = perspective4x4(1.047f, w/h, 0.01f, 10.0f);
  Mat4 view = look_at4x4(vec3(0.0f, 1.5f, 6.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
  Mat4 view_proj = mul4x4(proj, view);
  state.rx += 0.01f; 
  state.ry += 0.02f;
  Mat4 rxm = rotate4x4(vec3(1.0f, 0.0f, 0.0f), state.rx);
  Mat4 rym = rotate4x4(vec3(0.0f, 1.0f, 0.0f), state.ry);
  Mat4 model = mul4x4(rxm, rym);
  vs_params.mvp = mul4x4(view_proj, model);

  sg_pass_action pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.5f, 0.75f, 1.0f } }
  };
  sg_begin_default_pass(&pass_action, (int)w, (int)h);
  sg_apply_pipeline(state.pip);
  sg_apply_bindings(&state.bind);
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
  sg_draw(0, _indices, 1);
  sg_end_pass();
  sg_commit();
}

void cleanup(void) {
  sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
  (void)argc; (void)argv;
  return (sapp_desc){
    .init_cb = init,
    .frame_cb = frame,
    .cleanup_cb = cleanup,
    .width = 800,
    .height = 600,
    .sample_count = 4,
    .gl_force_gles2 = true,
    .window_title = "Quad (sokol-app)",
    .icon.sokol_default = true,
  };
}
