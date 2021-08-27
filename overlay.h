#include "math.h"
typedef struct {
  sg_buffer ibuf, vbuf;
  size_t index_count;
} _ol_Shape;

static struct {
  _ol_Shape quad_shape;
  sg_pipeline pip;
  // For when you pass color but don't care about the texture
  sg_image white_pixel;
  sg_bindings bind;
} _ol_state;

typedef struct {
  int x;
  int y;
  int w;
  int h;
} ol_Rect;

typedef struct {
  sg_image sg;
  int width, height;
} ol_Image;

void ol_init() {
  const float vertices[] = {
    0, -1, 0, 1,
    1, -1, 1, 1,
    1, 0,  1, 0,
    0, 0,  0, 0
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
        [ATTR_overlay_vs_uv].format = SG_VERTEXFORMAT_FLOAT2,
      }
    },
    .shader = sg_make_shader(overlay_shader_desc(sg_query_backend())),
    .index_type = SG_INDEXTYPE_UINT16,
    .cull_mode = SG_CULLMODE_NONE,
    .depth = {
      .write_enabled = true,
      .compare = SG_COMPAREFUNC_LESS_EQUAL,
    },
    .colors[0].blend = (sg_blend_state){
      .enabled = true,
      .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
      .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
 //     .dst_factor_rgb = SG_,
      .src_factor_alpha = SG_BLENDFACTOR_ONE,
      .dst_factor_alpha = SG_BLENDFACTOR_ZERO
    }
  });
}

ol_Image ol_image_from_sg(sg_image sg, int w, int h) {
  return (ol_Image) {
    .width = w,
    .height = h,
    .sg = sg
  };
}

ol_Image ol_load_image(const char *path) {
  cp_image_t png = cp_load_png(path);
 // cp_flip_image_horizontal(&png);
  size_t w = png.w, h = png.h;
  sg_image img = sg_make_image(&(sg_image_desc){
    .width = png.w,
    .height = png.h,
    .data.subimage[0][0] = (sg_range){ png.pix,w*h*sizeof(cp_pixel_t) } ,
  });
  ol_Image res = {
    .width = png.w,
    .height = png.h,
    .sg = img
  };
  cp_free_png(&png);
  return res;
}

void ol_begin() {
  sg_apply_pipeline(_ol_state.pip);
}

void ol_draw_tex_part(ol_Image *img, ol_Rect r, ol_Rect part) {
  _ol_state.bind.index_buffer = _ol_state.quad_shape.ibuf;
  _ol_state.bind.vertex_buffers[0] = _ol_state.quad_shape.vbuf;
  _ol_state.bind.fs_images[SLOT_tex] = img->sg;
  sg_apply_bindings(&_ol_state.bind); 
  const float sw = sapp_widthf();
  const float sh = sapp_heightf();
  overlay_vs_params_t overlay_vs_params = { 
    .minuv = vec2(part.x/(float)img->width, part.y/(float)img->height),
    .sizuv = vec2(part.w/(float)img->width, part.h/(float)img->height),
    .mvp = mul4x4(mul4x4(ortho4x4(sw, sh, 0.01, 100.0), translate4x4(vec3(-1.0+(float)r.x*2/sw, 1.0-(float)r.y*2/sh, 0.0))), scale4x4(vec3(r.w, r.h, 1.0)))
  };
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_overlay_vs_params, &SG_RANGE(overlay_vs_params));
  sg_draw(0, _ol_state.quad_shape.index_count, 1);
}

void ol_draw_tex(ol_Image *img, ol_Rect r) {
  ol_draw_tex_part(img, r, (ol_Rect) { 0, 0, img->width, img->height });
}


void ol_draw_rect(Vec4 color, ol_Rect rect) {
  const uint8_t pixel[4] = { (uint8_t)(color.x*255), (uint8_t)(color.y*255), (uint8_t)(color.z*255), (uint8_t)(color.w*255) }; 
  sg_image pixel_img = sg_make_image(&(sg_image_desc){ 
    .width = 1,
    .height = 1,
    .data.subimage[0][0] = SG_RANGE(pixel)
  });
  ol_Image img = ol_image_from_sg(pixel_img, 1, 1);
  ol_draw_tex(&img, rect);
  sg_destroy_image(pixel_img);
}


