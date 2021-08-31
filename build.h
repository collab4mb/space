
typedef struct {
  float appear_anim;
  bool appearing;
} build_State;



void build_enter(build_State *self) {
  self->appear_anim = fmaxf(fminf(self->appear_anim, 1.0f), 0.0f);
  self->appearing = true;
}

void build_leave(build_State *self) {
  self->appear_anim = fmaxf(fminf(self->appear_anim, 1.0f), 0.0f);
  self->appearing = false;
}

void build_toggle(build_State *self) {
  if (self->appearing) build_leave(self);
  else build_enter(self);
}

float _build_easeinout(float from, float to, float x) {
  float v = -(cosf((float)M_PI * x) - 1) / 2;
  return from-v*(from-to);
}

int _build_interp(build_State *self, int from, int to, float mul) {
  return (int)_build_easeinout((float)from, (float)to, fmaxf(fminf(self->appear_anim*mul, 1.0f), 0.0f));
}

void build_draw(build_State *self) {
  (void) self;
  ui_screen(sapp_width(), sapp_height());
    ui_row(ui_rel_x(1.0), ui_rel_y(1.0));
      ui_setoffset(_build_interp(self, -ui_rel_x(0.3f), 0, 1.3f), 0);
      ui_frame(ui_rel_x(0.3f), ui_rel_y(1.0), 1);
      ui_row(ui_rel_x(0.3f), ui_rel_y(1.0));
        ui_gap(ui_rel_x(-1.0f));
        ui_setoffset(_build_interp(self, -ui_rel_x(1.0f), 0, 1.3f), 10);
        for (int i = 0; i < 3; i += 1) {
          ui_gap(10);
          ui_frame(ui_rel_x(1/3.0f)-10.0f-10.0f*1/3.0, ui_rel_x(1/3.0f)-10.0f-10.0f*1/3.0, 0);
        }
      ui_row_end();
      ui_gap(ui_rel_x(-0.3f));
      // A little margin from the top
      for (int i = 0; i < 4; i += 1) {
        ui_setoffset(0, _build_interp(self, -50, 10, 3.0f-((float)i/4.0f)));
        ui_gap(10);
        ui_frame(51, 51, 0);
      }
      ui_setoffset(0, 0);
    ui_row_end();
    ui_screen_anchor_xy(0, 1.0);
    ui_row(ui_rel_x(1.0), 32);
      ui_gap(ui_rel_x(0.3f)+10);
      ui_setoffset(0, _build_interp(self, 50, 0, 1.6f));
      ui_text("This is a tooltip bla bla bla");
    ui_row_end();
  ui_screen_end();
  if (self->appearing)
    self->appear_anim += 0.016f;
  else
    self->appear_anim -= 0.016f;
}

