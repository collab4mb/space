#define MAX_SPEED2 (0.4f*0.4f)
#define ACCEL 0.004f
#define DECEL 0.002f

static void player_update(Ent *player) {
  //Player input
  if(input_key_down(SAPP_KEYCODE_LEFT))
    player->angle-=0.05f;
  if(input_key_down(SAPP_KEYCODE_RIGHT))
    player->angle+=0.05f;
  player->dir = vec2_rot(player->angle);
  float t = player->dir.x;
  player->dir.x = player->dir.y;
  player->dir.y = t;

  //TODO: this could cause problems on low fps
  if(input_key_down(SAPP_KEYCODE_UP)) {
    player->vel.x+=player->dir.x*ACCEL;
    player->vel.y+=player->dir.y*ACCEL;
    if(magmag2(player->vel)>MAX_SPEED2)
      player->vel = mul2_f(norm2(player->vel),0.4f);
  }
  else if(input_key_down(SAPP_KEYCODE_DOWN)) {
    player->vel.x-=player->dir.x*ACCEL;
    player->vel.y-=player->dir.y*ACCEL;
    if(magmag2(player->vel)>MAX_SPEED2)
      player->vel = mul2_f(norm2(player->vel),0.4f);
  }

}

#undef MAX_SPEED2
#undef ACCEL
#undef DECEL
