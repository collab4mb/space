#include <stdarg.h>

typedef enum {
  ui_Frame_Hex,
  ui_Frame_Square,
  ui_Frame_HexSelected,
  ui_Frame_Healthbar,
  ui_Frame_COUNT
} ui_Frame;

const ol_Rect ui_Frame_spritesheet_area[ui_Frame_COUNT] = {
   [ui_Frame_Hex]         = { 0, 0, 48, 48 },
   [ui_Frame_HexSelected] = { 0, 0, 48, 48 },
   [ui_Frame_Square]      = { 48, 0, 48, 48 },
};

const Vec4 ui_Frame_color[ui_Frame_COUNT] = {
   [ui_Frame_Hex]         = { 1.0, 0.2, 0.3, 1.0 },
   [ui_Frame_Square]      = { 1.0, 0.2, 0.3, 1.0 },
   [ui_Frame_HexSelected] = { 3.0, 0.8, 0.8, 1.0 },
};

typedef enum { ui_HealthbarShape_Minimal, ui_HealthbarShape_Fancy } ui_HealthbarShape;

typedef struct {
  enum { 
    Ui_Cmd_Frame,
    Ui_Cmd_Image,
    Ui_Cmd_Text,
  } kind;
  ol_Rect rect;
  union {
    struct {
      /* used for ui_Frame_Healthbar */
      float hp;
      ui_HealthbarShape shape;

      ui_Frame frame;
    } frame;
    struct {
      ol_Image *img;
      ol_Rect part;
    } image;
    struct {
      const char *text;
    } text;
  } data;
} ui_Command;


typedef struct {
  float anchor_x;
  float anchor_y;
} ui_Screen;

typedef struct {
  int offset;
} ui_Column;

typedef struct {
  int offset;
} ui_Row;

typedef enum {
  Ui_Layout_Screen,
  Ui_Layout_Column,
  Ui_Layout_Row
} ui_LayoutKind;

typedef struct {
  ui_LayoutKind kind;
  ol_Rect bounds;
  ol_Rect measure;
  union {
    ui_Screen screen;
    ui_Column column;
    ui_Row row;
  } data;
} ui_Layout;

#define TEXBUF_SIZE (1 << 16)

#define MAX_HEALTHBARS (1 << 4)
typedef struct {
  Vec2 pos, uv;
  float hp, fancy_shape;
} HealthbarVert;

typedef struct {
  sg_buffer vbuf, ibuf;
  HealthbarVert verts[MAX_HEALTHBARS * 4];
  uint16_t      indxs[MAX_HEALTHBARS * 6];
  int to_render_this_frame;
  sg_pipeline pip;
} HealthbarState;

typedef struct {
  ol_Font font;
  ol_Image atlas;
  char textbuf[TEXBUF_SIZE];
  size_t textbuf_offs;
  ui_Command commands[1024];
  size_t command_count;
  ui_Layout layouts[32];
  size_t layout_count;
  size_t measuremode_counter;
  size_t command_iter;
  int margin;
  int offset_x, offset_y;
  HealthbarState healthbar;
  int mx, my;
} ui_State;

static ui_State _ui_state;

void ui_init() {
  _ui_state = (ui_State) {
    .font = ol_load_font("./Orbitron-Regular.ttf"),
    .atlas = ol_load_image("./ui.png"),
  };

  _ui_state.healthbar.pip = sg_make_pipeline(&(sg_pipeline_desc) {
    .layout = (sg_layout_desc) {
      .attrs[ATTR_healthbar_vs_pos].format          = SG_VERTEXFORMAT_FLOAT2,
      .attrs[ATTR_healthbar_vs_uv].format           = SG_VERTEXFORMAT_FLOAT2,
      .attrs[ATTR_healthbar_vs_hp].format           = SG_VERTEXFORMAT_FLOAT,
      .attrs[ATTR_healthbar_vs_fancy_shape].format  = SG_VERTEXFORMAT_FLOAT,
    },
    .shader = sg_make_shader(healthbar_shader_desc(sg_query_backend())),
    .index_type = SG_INDEXTYPE_UINT16,
    .cull_mode = SG_CULLMODE_BACK,
    .depth = {
      .write_enabled = true,
      .pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL,
      .compare = SG_COMPAREFUNC_LESS_EQUAL,
    },
    .color_count = 2,
    .colors[0].blend = PREMULTIPLIED_BLEND,
  });
  _ui_state.healthbar.vbuf = sg_make_buffer(&(sg_buffer_desc) {
    .size = sizeof(_ui_state.healthbar.verts),
    .usage = SG_USAGE_STREAM,
  });
  _ui_state.healthbar.ibuf = sg_make_buffer(&(sg_buffer_desc) {
    .size = sizeof(_ui_state.healthbar.indxs),
    .usage = SG_USAGE_STREAM,
    .type = SG_BUFFERTYPE_INDEXBUFFER,
  });
}

// -- Cutters --
_Thread_local ol_Rect _ui_rswap;

static ol_Rect* ui_cprect(ol_Rect rect) {
    _ui_rswap = rect;
    return &_ui_rswap;
}

static ol_Rect ui_mkrect(int x, int y, int w, int h) {
    return (ol_Rect) { .x = x, .y = y, .w = w, .h = h };
}

static void ui_setmousepos(int x, int y) {
  _ui_state.mx = x;
  _ui_state.my = y;
}

static ol_Rect ui_cut_top(ol_Rect *rect, int amount) {
    int init = rect->y;
    rect->y += amount;
    rect->h -= amount;

    return ui_mkrect(rect->x, init, rect->w, amount);
}

static ol_Rect ui_cut_bottom(ol_Rect *rect, int amount) {
    int init = rect->y+rect->h-amount;
    rect->h -= amount;

    return ui_mkrect(rect->x, init, rect->w, amount);
}

static ol_Rect ui_cut_left(ol_Rect *rect, int amount) {
    int init = rect->x;
    rect->x += amount;
    rect->w -= amount;

    return ui_mkrect(init, rect->y, amount, rect->h);
}

static ol_Rect ui_cut_right(ol_Rect *rect, int amount) {
    int init = rect->x+rect->w-amount;
    rect->w -= amount;

    return ui_mkrect(init, rect->y, amount, rect->h);
}

// Center relative to
static int _ui_crt(int y, int parent_size, int my_size) {
  return parent_size/2+my_size/2+y;
}

static ui_Layout* _ui_getlayout(size_t offs) {
  assert(_ui_state.layout_count > offs && "Illegal access on layout stack");
  return &_ui_state.layouts[_ui_state.layout_count-offs-1];
} 

static ui_Layout* _ui_addlayout() {
  assert(_ui_state.layout_count < 32 && "Layout stack overflow");
  return &_ui_state.layouts[_ui_state.layout_count];
} 

// Returns the size depending on the layout currently used
static ol_Rect _ui_query_bounds(int width, int height) {
  ol_Rect rect = { 0, 0, width, height };
  if (_ui_state.layout_count == 0) return rect;
  ui_Layout *curr = _ui_getlayout(0);
  switch (curr->kind) {
    case Ui_Layout_Screen:
      rect.x = (int)(curr->data.screen.anchor_x*(float)curr->bounds.w-curr->data.screen.anchor_x*(float)width); 
      rect.y = (int)(curr->data.screen.anchor_y*(float)curr->bounds.h-curr->data.screen.anchor_y*(float)height);
      break;
    case Ui_Layout_Column:
      rect.y += curr->data.column.offset;
      curr->data.column.offset += height;
      curr->measure.w = width > curr->measure.w ? width : curr->measure.w;
      curr->measure.h += height;
      break;
    case Ui_Layout_Row:
      rect.x += curr->data.column.offset;
      curr->data.column.offset += width;
      curr->measure.h = height > curr->measure.h ? height : curr->measure.h;
      curr->measure.w += width;
      break;
  }
  rect.x += curr->bounds.x;
  rect.y += curr->bounds.y;
  rect.x += _ui_state.margin;
  rect.y += _ui_state.margin;
  rect.w -= _ui_state.margin*2;
  rect.h -= _ui_state.margin*2;
  _ui_state.margin = 0;
  return rect;
}

static void ui_setoffset(int x, int y) {
  _ui_state.offset_x = x;
  _ui_state.offset_y = y;
}

static void ui_margin(int size) {
  _ui_state.margin += size;
}

static void ui_gap(int gap) {
  ui_Layout *layout = _ui_getlayout(0);
  if (layout->kind == Ui_Layout_Row)
    layout->data.row.offset += gap;
  if (layout->kind == Ui_Layout_Column)
    layout->data.column.offset += gap;
}

static void ui_screen(int width, int height) {
  ui_Layout layout = { 
    .kind = Ui_Layout_Screen,
    .bounds = _ui_query_bounds(width, height),
    .data.screen = (ui_Screen) {
      .anchor_x = 0,
      .anchor_y = 0,
    }
  };
  layout.measure = layout.bounds;
  *_ui_addlayout() = layout;
  _ui_state.layout_count += 1;
}

static ol_Rect ui_screen_end() {
  assert(_ui_state.layouts[_ui_state.layout_count-1].kind == Ui_Layout_Screen && "Called end_screen with previous layout not being a screen");
  ol_Rect r = _ui_getlayout(0)->measure;
  _ui_state.layout_count -= 1;
  return r;
}

static void ui_column(int width, int height) {
  ui_Layout layout = { 
    .kind = Ui_Layout_Column,
    .bounds = _ui_query_bounds(width, height),
    .data.column = (ui_Column) {
      .offset = 0, 
    }
  };
  layout.measure = (ol_Rect) { layout.bounds.x, layout.bounds.y, 0, 0 };
  *_ui_addlayout() = layout;
  _ui_state.layout_count += 1;
}

static ol_Rect ui_column_end() {
  assert(_ui_state.layouts[_ui_state.layout_count-1].kind == Ui_Layout_Column && "Called end_column with previous layout not being a column");
  ol_Rect r = _ui_getlayout(0)->measure;
  _ui_state.layout_count -= 1;
  return r;
}

static void ui_row(int width, int height) {
  ui_Layout layout = { 
    .kind = Ui_Layout_Row,
    .bounds = _ui_query_bounds(width, height),
    .data.row = (ui_Row) {
      .offset = 0, 
    }
  };
  layout.measure = (ol_Rect) { layout.bounds.x, layout.bounds.y, 0, 0 };
  *_ui_addlayout() = layout;
  _ui_state.layout_count += 1;
}

static ol_Rect ui_row_end() {
  assert(_ui_getlayout(0)->kind == Ui_Layout_Row && "Called end_row with previous layout not being a row");
  ol_Rect r = _ui_getlayout(0)->measure;
  _ui_state.layout_count -= 1;
  return r;
}

// Pass in a value between 0 and 1, where 0 is the leftmost position, and 1 is the rightmost
static void ui_screen_anchor_x(float anchor) {
  assert(_ui_getlayout(0)->kind == Ui_Layout_Screen && "Called end_screen with previous layout not being a screen");
  _ui_getlayout(0)->data.screen.anchor_x = anchor;
}

static void ui_screen_anchor_y(float anchor) {
  assert(_ui_getlayout(0)->kind == Ui_Layout_Screen && "Called end_screen with previous layout not being a screen");
  _ui_getlayout(0)->data.screen.anchor_y = anchor;
}

static void ui_screen_anchor_xy(float anchorx, float anchory) {
  assert(_ui_getlayout(0)->kind == Ui_Layout_Screen && "Called end_screen with previous layout not being a screen");
  _ui_getlayout(0)->data.screen.anchor_x = anchorx;
  _ui_getlayout(0)->data.screen.anchor_y = anchory;
}

static int ui_rel_x(float mul) {
  return (int)((float)_ui_getlayout(0)->bounds.w * mul);
}

static int ui_rel_y(float mul) {
  return (int)((float)_ui_getlayout(0)->bounds.h * mul);
}

static void ui_addcommand(ui_Command cmd) {
  cmd.rect.x += _ui_state.offset_x;
  cmd.rect.y += _ui_state.offset_y;
  if (_ui_state.measuremode_counter == 0) {
    _ui_state.commands[_ui_state.command_count] = cmd;
    _ui_state.command_count += 1;
  }
}

static void ui_text(const char *text) {
  ol_Rect rect = ol_measure_text(&_ui_state.font, text, 0, 0);
  rect = _ui_query_bounds(rect.w, rect.h);
  ui_addcommand((ui_Command) {
    .kind = Ui_Cmd_Text,
    .rect = rect,
    .data.text = text,
  });
}

static void ui_measuremode() {
  _ui_state.measuremode_counter += 1;
}

static void ui_end_measuremode() {
  assert(_ui_state.measuremode_counter > 0 && "No previous measure-mode was done");
  _ui_state.measuremode_counter -= 1;
}

static void ui_image(ol_Image *img) {
  ui_addcommand((ui_Command) {
    .kind = Ui_Cmd_Image,
    .rect = _ui_query_bounds(img->width, img->height),
    .data = {
      .image = { 
        img,
        (ol_Rect) { 0, 0, img->width, img->height }
      } 
    }
  });
}

static void ui_image_ex(ol_Image *img, ol_Rect part, int w, int h) {
  ui_addcommand((ui_Command) {
    .kind = Ui_Cmd_Image,
    .rect = _ui_query_bounds(w, h),
    .data = {
      .image = { 
        img,
        part
      } 
    }
  });
}

static void ui_image_ratio(ol_Image *img, float x, float y) {
  ui_addcommand((ui_Command) {
    .kind = Ui_Cmd_Image,
    .rect = _ui_query_bounds((int)(x*(float)img->width), (int)(y*(float)img->height)),
    .data = {
      .image = { 
        img,
        (ol_Rect) { 0, 0, img->width, img->height }
      } 
    }
  });
}


static void ui_image_part(ol_Image *img, ol_Rect part) {
  ui_addcommand((ui_Command) {
    .kind = Ui_Cmd_Image,
    .rect = _ui_query_bounds(part.w, part.h),
    .data.image = { img, part }
  });
}

static bool ui_frame(int width, int height, ui_Frame frame) {
  ol_Rect bounds = _ui_query_bounds(width, height); 
  ui_addcommand((ui_Command) {
    .kind = Ui_Cmd_Frame,
    .rect = bounds,
    .data.frame = { .frame = frame }
  });
  return 
    _ui_state.mx >= bounds.x && _ui_state.mx <= (bounds.x+bounds.w) &&
    _ui_state.my >= bounds.y && _ui_state.my <= (bounds.y+bounds.h);
}

static void ui_healthbar(int width, int height, float hp, ui_HealthbarShape shape) {
  ui_addcommand((ui_Command) {
    .kind = Ui_Cmd_Frame,
    .rect = _ui_query_bounds(width, height),
    .data.frame = { .frame = ui_Frame_Healthbar, .hp = hp, .shape = shape },
  });
}

static void ui_vtextf(const char *fmt, va_list vl) {
  // TODO: Add checks
  assert(_ui_state.textbuf_offs < TEXBUF_SIZE && "Text buffer overflow, please print less text, or you forgot to call ui_end_pass after commands");
  ui_text(_ui_state.textbuf+_ui_state.textbuf_offs);
  int offs = vsnprintf(_ui_state.textbuf+_ui_state.textbuf_offs, TEXBUF_SIZE-_ui_state.textbuf_offs, fmt, vl);
  assert(offs >= 0 && "Ui: Invalid offset for format string");
  _ui_state.textbuf_offs += (size_t)offs+1;
}

static void ui_textf(const char *fmt, ...) {
  va_list vl;
  va_start(vl, fmt);
  ui_vtextf(fmt, vl);
  va_end(vl);
}


static void ui_end_pass() {
  HealthbarState *hbs = &_ui_state.healthbar;

  sg_apply_pipeline(hbs->pip);
  sg_update_buffer(hbs->vbuf, &(sg_range) {
    .ptr = _ui_state.healthbar.verts, 
    .size = sizeof(HealthbarVert) * 4 * hbs->to_render_this_frame,
  });
  sg_update_buffer(hbs->ibuf, &(sg_range) {
    .ptr = _ui_state.healthbar.indxs, 
    .size = sizeof(uint16_t) * 6 * hbs->to_render_this_frame,
  });
  healthbar_vs_params_t vs_params = { .resolution = { sapp_widthf(), sapp_heightf() } };
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_healthbar_vs_params, &SG_RANGE(vs_params));
  static float time = 0.0f;
  time += 0.01f;
  healthbar_fs_params_t fs_params = { .time = time };
  sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_healthbar_fs_params, &SG_RANGE(fs_params));
  sg_apply_bindings(&(sg_bindings) {
    .vertex_buffers[0] = hbs->vbuf,
    .index_buffer = hbs->ibuf,
  });
  sg_draw(0, 6*hbs->to_render_this_frame, 1);

  _ui_state.offset_x = 0;
  _ui_state.offset_y = 0;
  _ui_state.command_count = 0;
  _ui_state.command_iter = 0;
  _ui_state.textbuf_offs = 0;
  _ui_state.healthbar.to_render_this_frame = 0;
}

static ui_Command *ui_command_next() {
  if (_ui_state.command_count == _ui_state.command_iter) return NULL;
  return &_ui_state.commands[_ui_state.command_iter++];
}

static void ui_render_healthbar(ol_Rect rect, float hp, ui_HealthbarShape shape) {
  HealthbarState *hbs = &_ui_state.healthbar;

  memcpy(hbs->verts + 4 * hbs->to_render_this_frame, &((HealthbarVert[]) {
    { rect.x,          rect.y,           0.0f, 0.2f, hp, shape },
    { rect.x + rect.w, rect.y,           1.0f, 0.2f, hp, shape },
    { rect.x + rect.w, rect.y + rect.h,  1.0f, 0.0f, hp, shape },
    { rect.x,          rect.y + rect.h,  0.0f, 0.0f, hp, shape },
  }), sizeof(HealthbarVert) * 4);

  uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
  for (int i = 0; i < 6; i++)
    indices[i] += hbs->to_render_this_frame*4;
  memcpy(hbs->indxs + 6 * hbs->to_render_this_frame, &indices, sizeof(uint16_t) * 6);

  hbs->to_render_this_frame++;
}

static void ui_render() {
  for (ui_Command *cmd = ui_command_next(); cmd != NULL; cmd = ui_command_next()) {
    switch (cmd->kind) {
      case Ui_Cmd_Text: {
        ol_draw_text(&_ui_state.font, cmd->data.text.text, cmd->rect.x, cmd->rect.y, vec4_f(1.0));
      } break;
      case Ui_Cmd_Frame: {
        ui_Frame frame = cmd->data.frame.frame;
        if (frame == ui_Frame_Healthbar)
          ui_render_healthbar(cmd->rect, cmd->data.frame.hp, cmd->data.frame.shape);
        else
          ol_ninepatch(&_ui_state.atlas, cmd->rect, (ol_NinePatch) { 
            .inner = { 16, 16, 16, 16 },
            .outer = ui_Frame_spritesheet_area[frame],
          }, ui_Frame_color[frame]);
      } break;
      case Ui_Cmd_Image: {
        ol_draw_tex_part(cmd->data.image.img, cmd->rect, cmd->data.image.part);
      } break;
      default: {
        assert(false);
      }
    }
  }
}

static void ui_deinit() {
}

// Use ui handle here


