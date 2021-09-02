#include <stdarg.h>

typedef struct {
    enum { 
        Ui_Cmd_Frame,
        Ui_Cmd_Image,
        Ui_Cmd_Text,
    } kind;
    int tag;
    ol_Rect rect;
    union {
      struct {
        const char *text;
      } text;
      struct {
        ol_Image *img;
        ol_Rect part;
      } image;
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

typedef struct {
  ol_Font *font;
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
  int mx, my;
} ui_State;

static ui_State _ui_state;

void ui_init(ol_Font *font) {
  _ui_state = (ui_State) {
    .font = font
  };
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
  ol_Rect rect = ol_measure_text(_ui_state.font, text, 0, 0);
  rect = _ui_query_bounds(rect.w, rect.h);
  ui_addcommand((ui_Command) {
    .kind = Ui_Cmd_Text,
    .rect = rect,
    .data = {
      .text = { 
        text
      } 
    }
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
    .data = {
      .image = { 
        img,
        part
      } 
    }
  });
}

static bool ui_frame(int width, int height, int tag) {
  ol_Rect bounds = _ui_query_bounds(width, height); 
  ui_addcommand((ui_Command) {
    .kind = Ui_Cmd_Frame,
    .rect = bounds,
    .tag = tag,
  });
  return 
    _ui_state.mx >= bounds.x && _ui_state.mx <= (bounds.x+bounds.w) &&
    _ui_state.my >= bounds.y && _ui_state.my <= (bounds.y+bounds.h);
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
  _ui_state.offset_x = 0;
  _ui_state.offset_y = 0;
  _ui_state.command_count = 0;
  _ui_state.command_iter = 0;
  _ui_state.textbuf_offs = 0;
}

static ui_Command *ui_command_next() {
  if (_ui_state.command_count == _ui_state.command_iter) return NULL;
  return &_ui_state.commands[_ui_state.command_iter++];
}

static void ui_deinit() {
}

// Use ui handle here


