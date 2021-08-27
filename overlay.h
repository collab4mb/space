typedef struct {
  sg_buffer ibuf, vbuf;
  size_t index_count;
} _ol_Shape;

static struct {
  _ol_Shape quad_shape;
  sg_pipeline pip;
  sg_bindings bind;
} _ol_state;

void ol_init() {
  const float vertices[] = {
    0, -1,
    1, -1,
    1, 0,
    0, 0
  };
  const uint16_t indices[] = {
    0, 1, 2,
    2, 3, 0
  };
  _ol_state.quad_shape.vbuf = sg_make_buffer(&(sg_buffer_desc) {
    .type = SG_BUFFERTYPE_VERTEXBUFFER,
    .data = SG_RANGE(vertices)
  });
  _ol_state.quad_shape.ibuf = sg_make_buffer(&(sg_buffer_desc) {
    .type = SG_BUFFERTYPE_INDEXBUFFER,
    .data = SG_RANGE(indices)
  });
  _ol_state.quad_shape.index_count = 6;
  _ol_state.pip = sg_make_pipeline(&(sg_pipeline_desc) {
    .layout = {
      .attrs = {
        [ATTR_overlay_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
      }
    },
    .shader = sg_make_shader(overlay_shader_desc(sg_query_backend())),
    .index_type = SG_INDEXTYPE_UINT16,
    .cull_mode = SG_CULLMODE_NONE,
    .depth = {
      .write_enabled = true,
      .compare = SG_COMPAREFUNC_LESS_EQUAL,
    },
  });
}

void ol_begin() {
  sg_apply_pipeline(_ol_state.pip);
}

void ol_draw_rect(Vec4 color, int x, int y, int w, int h) {
  _ol_state.bind.index_buffer = _ol_state.quad_shape.ibuf;
  _ol_state.bind.vertex_buffers[0] = _ol_state.quad_shape.vbuf;
  sg_apply_bindings(&_ol_state.bind); 
  const float sw = sapp_widthf();
  const float sh = sapp_heightf();
  overlay_vs_params_t overlay_vs_params = { 
    .color = color,
    .mvp = mul4x4(mul4x4(ortho4x4(sw, sh, 0.01, 100.0), translate4x4(vec3(-1.0+(float)x*2/sw, 1.0-(float)y*2/sh, 0.0))), scale4x4(vec3(w, h, 1.0)))
  };
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_overlay_vs_params, &SG_RANGE(overlay_vs_params));
  sg_draw(0, _ol_state.quad_shape.index_count, 1);
}

