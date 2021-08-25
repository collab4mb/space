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
#include "build/shaders.glsl.h"

static struct {
  sg_pass_action pass_action;
  sg_pipeline pip;
  sg_bindings bind;
} state;

void init(void) {
  sg_setup(&(sg_desc){
    .context = sapp_sgcontext()
  });

  /* a vertex buffer */
  float vertices[] = {
    // positions            colors
    -0.5f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
     0.5f,  0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
     0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f,
    -0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 0.0f, 1.0f,
  };
  state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
    .data = SG_RANGE(vertices),
    .label = "quad-vertices"
  });

  /* an index buffer with 2 triangles */
  uint16_t indices[] = { 0, 1, 2,  0, 2, 3 };
  state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
    .type = SG_BUFFERTYPE_INDEXBUFFER,
    .data = SG_RANGE(indices),
    .label = "quad-indices"
  });

  /* a shader (use separate shader sources here */
  sg_shader shd = sg_make_shader(quad_shader_desc(sg_query_backend()));

  /* a pipeline state object */
  state.pip = sg_make_pipeline(&(sg_pipeline_desc){
    .shader = shd,
    .index_type = SG_INDEXTYPE_UINT16,
    .layout = {
      .attrs = {
        [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
        [ATTR_vs_color0].format   = SG_VERTEXFORMAT_FLOAT4
      }
    },
    .label = "quad-pipeline"
  });

  /* default pass action */
  state.pass_action = (sg_pass_action) {
    .colors[0] = { .action=SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f } }
  };
}

void frame(void) {
  sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
  sg_apply_pipeline(state.pip);
  sg_apply_bindings(&state.bind);
  sg_draw(0, 6, 1);
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
    .gl_force_gles2 = true,
    .window_title = "Quad (sokol-app)",
    .icon.sokol_default = true,
  };
}
