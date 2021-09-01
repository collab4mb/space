
typedef struct {
  float appear_anim;
  bool appearing;
} build_State;

static build_State _build_state = { 0 };

#define self _build_state

void build_enter() {
  self.appear_anim = fmaxf(fminf(self.appear_anim, 1.0f), 0.0f);
  self.appearing = true;
}

void build_leave() {
  self.appear_anim = fmaxf(fminf(self.appear_anim, 1.0f), 0.0f);
  self.appearing = false;
}

void build_toggle() {
  if (self.appearing) build_leave();
  else build_enter();
}

float _build_easeinout(float from, float to, float x) {
  float v = -(cosf((float)M_PI * x) - 1) / 2;
  return from-v*(from-to);
}

int _build_interp(int from, int to, float mul) {
  return (int)_build_easeinout((float)from, (float)to, fmaxf(fminf(self.appear_anim*mul, 1.0f), 0.0f));
}

static Ent* add_ent(Ent ent);
bool build_event(const sapp_event *ev) {
  switch (ev->type) {
    case SAPP_EVENTTYPE_KEY_DOWN: {
      if (ev->key_code == SAPP_KEYCODE_TAB) {
        build_toggle();
        return true;
      }
      else if (self.appear_anim > 0.0f && ev->key_code == SAPP_KEYCODE_SPACE) {
        add_ent((Ent) {
          .art = Art_Pillar,
          .pos = add2(state->player->pos, mul2_f(vec2_swap(vec2_rot(state->player->angle)), 10.0)),
          .height = 0,
          .x_rot = 0,
          .transparency = 1.0,
          .passive_rotate_axis = vec3_y,
        });
        return true;
      }
    } break;
    default:;
  }
  return false;
}

static void draw_ent(Mat4 vp, Ent *ent);
void build_draw_3d(Mat4 vp) {
  if (self.appearing || self.appear_anim > 0) {
    sg_apply_pipeline(state->pip[Shader_Standard]);
    draw_ent(vp, &(Ent) {
      .art = Art_Pillar,
      .pos = add2(state->player->pos, mul2_f(vec2_swap(vec2_rot(state->player->angle)), 10.0)),
      .height = 0,
      .x_rot = 0,
      .transparency = lerp(0, 0.5, fmax(fmin(self.appear_anim, 1.0), 0.0)),
      .passive_rotate_axis = vec3_y,
    });
  }
}

void build_update() {
  float weight = 0.0;
  Ent *pillar = NULL;
  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
    if (ent->art == Art_Pillar) {
      Ent *this_pillar = ent;
      this_pillar->bloom = 0.0;
      Vec2 diff = sub2(state->player->pos, this_pillar->pos);
      float dot = -dot2(vec2_swap(vec2_rot(state->player->angle)), norm2(diff));
      float dist = sqrtf(dot2(diff, diff));

      if (dist < 20.0 && dot > weight) {
        pillar = this_pillar;
        weight = dot;
      }
    }
  }
  if (pillar != NULL)
    pillar->bloom = fminf(fmaxf(self.appear_anim, 0.0f), 0.7f);
}

void build_draw() {
  build_update();
  ui_screen(sapp_width(), sapp_height());
    ui_row(ui_rel_x(1.0), ui_rel_y(1.0));
      ui_setoffset(_build_interp(-300, 0, 1.3f), 0);
      ui_frame(300, ui_rel_y(1.0), 1);
      ui_gap(-300);
      ui_margin(10);
      ui_row(300, ui_rel_y(1.0));
        for (int i = 0; i < 3; i += 1) {
          ui_margin(5);
          ui_frame(ui_rel_x(1/3.0f), ui_rel_x(1/3.0f), 0);
        }
      ui_row_end();
      // A little margin from the top
      for (int i = 0; i < 4; i += 1) {
        ui_setoffset(0, _build_interp(-60, 10, 3.0f-((float)i/4.0f)));
        ui_gap(10);
        ui_frame(60, 60, 0);
      }
      ui_setoffset(0, 0);
    ui_row_end();
    ui_screen_anchor_xy(0, 1.0);
    ui_row(ui_rel_x(1.0), 32);
      ui_gap(300+10);
      ui_setoffset(0, _build_interp(50, 0, 1.6f));
      ui_text("This is a tooltip bla bla bla");
    ui_row_end();
  ui_screen_end();
  if (self.appearing)
    self.appear_anim += 0.016f;
  else
    self.appear_anim -= 0.016f;
}
#undef self 
