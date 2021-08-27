#define MAX_SPEED2 (0.4f*0.4f)
#define ACCEL 0.025f

static void player_update(Ent *player) {
  float angle_delta = 0;
  if (input_key_down(SAPP_KEYCODE_LEFT))
    angle_delta -= 0.05f;
  if (input_key_down(SAPP_KEYCODE_RIGHT))
    angle_delta += 0.05f;
  state->player->angle += angle_delta;
  state->player_turn_accel += angle_delta;
  state->player_turn_accel *= 0.8;
  Vec2 p_dir = vec2_swap(vec2_rot(player->angle));

  if (input_key_pressed(SAPP_KEYCODE_SPACE)) {
    Ent *e = add_ent((Ent) {
      .art = Art_Asteroid,
      .pos = add2(player->pos,mul2_f(p_dir,2.5f)),
      .vel = add2(player->vel,mul2_f(p_dir,0.6f)),
      .scale = 0.2f,
      .size = 0.2f,
      .weight = 1.0f,
    });
    give_ent_prop(e, EntProp_Projectile);
  }

  // TODO: take precautions to prevent movement from being tied to framerate
  float dir = 0.0f;
  if (input_key_down(SAPP_KEYCODE_UP))   dir =  1.0f;
  if (input_key_down(SAPP_KEYCODE_DOWN)) dir = -1.0f;

  if (dir != 0.0f) {
    player->vel = add2(player->vel, mul2_f(p_dir, dir*ACCEL));
    if (magmag2(player->vel)>MAX_SPEED2)
      player->vel = mul2_f(norm2(player->vel),0.4f);
  }
}

#undef MAX_SPEED2
#undef ACCEL
