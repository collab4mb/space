#include "math.h"
#include <math.h>

typedef enum {
  _build_Option_Pillar,
  _build_Option_Wall,
  _build_Option_COUNT,
} _build_Option;

typedef enum {
  _build_Mode_Build = _build_Option_COUNT,
  _build_Mode_COUNT,
} _build_Mode;

typedef struct {
  Ent *begin;
  Ent *end;
  bool semi_connected;
} _build_Connection;

#define SIDEBAR_SIZE 370
#define OPTION_BUTTON_SIZE 70
#define SELECTED_GLOW 0.7f
#define DISTANCE_DELTA 10.0f
#define MAX_SELECT_DIST 20.0f

static bool _build_connection_finalized(_build_Connection *connection) {
  return connection->begin != NULL && connection->end != NULL;
}

static void _build_connection_set_next(_build_Connection *connection, Ent *ent) {
  if (connection->semi_connected) {
    connection->end = ent;
  }
  else {
    connection->begin = ent;
  }
}

static void _build_connection_select(_build_Connection *connection) {
  if (connection->semi_connected && connection->end != NULL) {
    *connection = (_build_Connection) { 0 };
  }
  else if (connection->begin != NULL) {
    connection->semi_connected = true;
  }
}

typedef struct {
  _build_Option option_selected;
  size_t tooltip;
  float distance;
  float appear_anim;
  bool appearing;
  _build_Connection connection;
} build_State;

static build_State _build_state = { 0 };

typedef struct {
  const char *name;
  const char *tooltip;
  ol_Rect part;
} build_Option;

static const build_Option _build_options[] = {
  {"Pillar", "This is a pillar, press space to place a pillar", { 0, 48, 48, 48 } },
  {"Force field",   "This is a wall, you need to select 2 pillars to emerge a force field", { 48, 48, 48, 48 } },
  // Top options
  {"Build", "Leave build mode", { 48+48, 48, 48, 48 } },
};

#define self _build_state

static void build_enter() {
  self.distance = 10;
  self.appear_anim = m_clamp(self.appear_anim, 0.0f, 1.0f);
  self.appearing = true;
}

static void build_leave() {
  self.appear_anim = m_clamp(self.appear_anim, 0.0f, 1.0f);
  self.appearing = false;
}

static void build_toggle() {
  if (self.appearing) build_leave();
  else build_enter();
}

static float easeinout(float from, float to, float x) {
  float v = -(cosf((float)M_PI * x) - 1) / 2;
  return from-v*(from-to);
}

static int _build_interp(int from, int to, float mul) {
  return (int)easeinout((float)from, (float)to, fmaxf(fminf(self.appear_anim*mul, 1.0f), 0.0f));
}

static bool _build_make_ent(Ent *plr, Ent *ent) {
  if (self.option_selected == _build_Option_Pillar) {
    *ent = (Ent) {
      .art = Art_Pillar,
      .pos = add2(plr->pos, mul2_f(vec2_swap(vec2_rot(plr->angle)), self.distance)),
      .height = 0,
      .x_rot = 0,
      .transparency = 1.0,
      .passive_rotate_axis = vec3_y,
    };
    return true;
  }
  else if (self.option_selected == _build_Option_Wall) {
    if (_build_connection_finalized(&self.connection)) {
      Vec2 diff = sub2(self.connection.end->pos, self.connection.begin->pos);
      Vec2 pos = add2(self.connection.begin->pos, div2_f(diff, 2.0f));

      *ent = (Ent) {
        .art = Art_Plane,
        .pos = pos,
        .angle = -rot_vec2(diff),
        .scale = { mag2(diff)/2.0f+0.4f, 4.0f, 1.0f },
        .height = -1.0f,
        .collider.size = mag2(diff)/2.0f,
        .collider.shape = Shape_Line,
        .collider.weight = 1000.0f,
      };
      return true;
    }
  }
  return false;
}

static Ent* add_ent(Ent ent);
static bool build_event(const sapp_event *ev) {
  switch (ev->type) {
    case SAPP_EVENTTYPE_MOUSE_SCROLL: {
      self.distance += ev->scroll_y;
    } break;
    case SAPP_EVENTTYPE_KEY_DOWN: {
      if (ev->key_code == SAPP_KEYCODE_TAB) {
        build_toggle();
        return true;
      }
      else if (self.appearing && ev->key_code == SAPP_KEYCODE_SPACE) {
        Ent dest, *plr = try_gendex(state->player);
        if (plr && _build_make_ent(plr, &dest)) {
          add_ent(dest);
        }
        _build_connection_select(&self.connection);
        return true;
      }
    } break;
    default:;
  }
  return false;
}

static void draw_ent(Mat4 vp, Ent *ent);
static void build_draw_3d(Mat4 vp) {
  Ent dest, *plr = try_gendex(state->player);
  if (plr && _build_make_ent(plr, &dest) && self.appearing) {
    sg_apply_pipeline(state->pip[state->meshes[dest.art].shader]);
    dest.transparency = 0.5;
    draw_ent(vp, &dest);
  }
}

static void build_update(float delta_time) {
  if (self.appearing)
    self.appear_anim += delta_time;
  else
    self.appear_anim -= delta_time;
  Ent *plr = try_gendex(state->player);
  if (!plr) return;

  if (input_key_down(SAPP_KEYCODE_LEFT_ALT)) {
    self.distance += DISTANCE_DELTA*delta_time;
  }
  if (input_key_down(SAPP_KEYCODE_LEFT_SHIFT)) {
    self.distance -= DISTANCE_DELTA*delta_time;
  }
  self.distance = fminf(fmaxf(self.distance, 0.0), MAX_SELECT_DIST);
  float weight = 0.0;
  Ent *pillar = NULL;
  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
    if (ent->art == Art_Pillar) {
      Ent *this_pillar = ent;
      this_pillar->bloom = 0.0;
      Vec2 diff = sub2(plr->pos, this_pillar->pos);
      float dot = -dot2(vec2_swap(vec2_rot(plr->angle)), norm2(diff));
      float dist = sqrtf(dot2(diff, diff));

      if (dist < MAX_SELECT_DIST && dot > weight) {
        pillar = this_pillar;
        weight = dot;
      }
    }
  }
  if (!_build_state.appearing || _build_state.option_selected != _build_Option_Wall) return;
  _build_connection_set_next(&self.connection, pillar);
  if (self.connection.begin)
    self.connection.begin->bloom = SELECTED_GLOW;
  if (self.connection.end)
    self.connection.end->bloom = SELECTED_GLOW;
}

static bool _build_icon_button(int w, int h, size_t i, bool selected) {
  bool pressed = false;
  bool hovered;
  ui_Frame frame = selected ? ui_Frame_HexSelected : ui_Frame_Hex;
  ui_screen(w, h);
    if ((hovered = ui_frame(ui_rel_x(1.0f), ui_rel_x(1.0f), frame)) && input_mouse_pressed(0)) {
      pressed = true;
    }
    ui_screen_anchor_xy(0.5, 0.5);
    ui_image_part(&_ui_state.atlas, _build_options[i].part);
  ui_screen_end();
  if (hovered)
    _build_state.tooltip = i;
  return pressed;
}

static void build_draw() {
  // Main overlay
  ui_screen(sapp_width(), sapp_height());
    // Main row
    ui_row(ui_rel_x(1.0), ui_rel_y(1.0));
      // Interpolate position for animation
      ui_setoffset(_build_interp(-SIDEBAR_SIZE, 0, 1.3f), 0);
      ui_screen(SIDEBAR_SIZE, ui_rel_y(1.0));
        ui_set_font(ui_Font_Small);
        ui_frame(ui_rel_x(1.0), ui_rel_y(1.0), 1);
        ui_margin(10);
        ui_row(SIDEBAR_SIZE, ui_rel_y(1.0));
          for (size_t i = 0; i < _build_Option_COUNT; i += 1) {
            ui_margin(5);  
            ui_column(ui_rel_x(1/3.0f), ui_rel_x(1/3.0f)+32); 
              if (_build_icon_button(ui_rel_x(1.0), ui_rel_x(1.0), i, i == self.option_selected)) {
                self.connection = (_build_Connection) { 0 };
                self.option_selected = i;
              }
              ui_screen(ui_rel_x(1.0), 32);
                ui_screen_anchor_x(0.5);
                ui_text(_build_options[i].name);
              ui_screen_end();
            ui_column_end();
          }
        ui_row_end();
        ui_set_font(ui_Font_Normal);
      ui_screen_end();
      for (size_t i = _build_Mode_Build; i < _build_Mode_COUNT; i += 1) {
        ui_setoffset(0, _build_interp(-OPTION_BUTTON_SIZE, 0, 3.0f-((float)i/4.0f)));
        ui_margin(5);
        if (_build_icon_button(OPTION_BUTTON_SIZE, OPTION_BUTTON_SIZE, i, self.appearing)) {
          build_toggle();
        }
      }
      ui_setoffset(0, 0);
    ui_row_end();
    ui_screen_anchor_xy(0, 1.0);
    ui_row(ui_rel_x(1.0), 32);
      ui_gap(SIDEBAR_SIZE+10);
      ui_setoffset(0, _build_interp(50, 0, 1.6f));
      ui_text(_build_options[self.tooltip].tooltip);
    ui_row_end();
  ui_screen_end();
}
#undef self 
