#include "math.h"
#include <stdint.h>
typedef struct {
  sg_buffer ibuf, vbuf;
  size_t index_count;
} _ol_Shape;

static struct {
  _ol_Shape quad_shape;
  sg_pipeline pip;
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

typedef struct {
  stbtt_packedchar pc[256];
  int size;
  int ascent;
  float scale;
  ol_Image img;
} ol_Font;

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

#define ATLAS_SIZE 1024

ol_Font ol_load_font(const char *path) {
  static uint8_t ttf_buffer[1 << 25];
  static uint8_t atlas[1024*1024];
  FILE *f = fopen(path, "rb");
  fread(ttf_buffer, 1, 1 << 25, f);

  ol_Font font;
  stbtt_fontinfo info;
  int ascent;
  float scale;
  font.size = 32;

  stbtt_InitFont(&info, ttf_buffer, 0);
  scale = stbtt_ScaleForPixelHeight(&info, (float)font.size);
  stbtt_GetFontVMetrics(&info, &ascent, NULL, NULL); 

  font.ascent = ascent;
  font.scale = scale;

  stbtt_pack_context context;
  assert(stbtt_PackBegin(&context, atlas, ATLAS_SIZE, ATLAS_SIZE, ATLAS_SIZE, 1, NULL) && "Failed to load font");
  stbtt_PackSetOversampling(&context, 2, 2);
  assert(stbtt_PackFontRange(&context, ttf_buffer, 0, (float)font.size, 0, 256, font.pc) && "Failed font packing");
  font.img = ol_image_from_sg(sg_make_image(&(sg_image_desc) {
    .pixel_format = SG_PIXELFORMAT_R8,
    .width = ATLAS_SIZE,
    .height = ATLAS_SIZE,
    .data.subimage[0][0] = (sg_range){atlas, ATLAS_SIZE*ATLAS_SIZE*sizeof(uint8_t)}
  }), ATLAS_SIZE, ATLAS_SIZE);
  return font;
}

void ol_begin() {
  sg_apply_pipeline(_ol_state.pip);
}

void _ol_draw_tex_part(ol_Image *img, ol_Rect r, ol_Rect part, Vec4 modulate, bool istxt) {
  _ol_state.bind.index_buffer = _ol_state.quad_shape.ibuf;
  _ol_state.bind.vertex_buffers[0] = _ol_state.quad_shape.vbuf;
  _ol_state.bind.fs_images[SLOT_tex] = img->sg;
  sg_apply_bindings(&_ol_state.bind); 
  const float sw = sapp_widthf();
  const float sh = sapp_heightf();
  overlay_vs_params_t overlay_vs_params = { 
    .modulate = modulate,
    .istxt = istxt ? 1.0 : 0.0,
    .minuv = vec2((float)part.x/(float)img->width, (float)part.y/(float)img->height),
    .sizuv = vec2((float)part.w/(float)img->width, (float)part.h/(float)img->height),
    .mvp = mul4x4(
      mul4x4(
        ortho4x4(sw, sh, 0.01f, 100.0f), 
        translate4x4(vec3(-1.0f+(float)r.x*2.0f/sw, 1.0f-(float)r.y*2.0f/sh, 0.0))
      ), 
      scale4x4(vec3((float)r.w, (float)r.h, 1.0))
    )
  };
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_overlay_vs_params, &SG_RANGE(overlay_vs_params));
  sg_draw(0, (int)_ol_state.quad_shape.index_count, 1);
}

void ol_draw_tex_part_ex(ol_Image *img, ol_Rect r, ol_Rect part, Vec4 modulate) {
  _ol_draw_tex_part(img, r, part, modulate, false);
}

void ol_draw_tex_part(ol_Image *img, ol_Rect r, ol_Rect part) {
  _ol_draw_tex_part(img, r, part, vec4(1.0, 1.0, 1.0, 1.0), false);
}

typedef struct {
  ol_Rect dest;
  ol_Rect part;
  int stride;
} ol_Glyph;

ol_Glyph ol_glyph_info(ol_Font *font, int glyph, int x, int y) {
  float startx = (float )x;
  float px = (float) x, py = (float) y;
  stbtt_aligned_quad quad;
  stbtt_GetPackedQuad(font->pc, ATLAS_SIZE, ATLAS_SIZE, glyph, &px, &py, &quad, 1);
  return (ol_Glyph) {
    .dest = { (int)quad.x0, (int)(quad.y0+((float)font->ascent*font->scale)), (int)quad.x1-(int)quad.x0, (int)quad.y1-(int)quad.y0 },
    .part = { 
      (int)(quad.s0*(float)font->img.width),
      (int)(quad.t0*(float)font->img.height),
      (int)((quad.s1-quad.s0)*(float)font->img.width),
      (int)((quad.t1-quad.t0)*(float)font->img.height) 
    },
    .stride =  (int)(px-startx)
  };
}

int ol_draw_glyph(ol_Font *font, int glyph, int x, int y, Vec4 modulate) {
  ol_Glyph glyph_value = ol_glyph_info(font, glyph, x, y);
  _ol_draw_tex_part(&font->img, glyph_value.dest, glyph_value.part, modulate, true);
  return glyph_value.stride;
}

ol_Rect ol_measure_text(ol_Font *font, const char *text, int x, int y) {
  int stride = 0;
  for (;*text; text += 1) {
    stride += ol_glyph_info(font, *text, x+stride, y).stride;
  }
  return (ol_Rect) { x, y, stride, font->size };
}

int ol_draw_text(ol_Font *font, const char *text, int x, int y, Vec4 modulate) {
  int stride = 0;
  for (;*text; text += 1) {
    stride += ol_draw_glyph(font, *text, x+stride, y, modulate);
  }
}

#undef ATLAS_SIZE
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


