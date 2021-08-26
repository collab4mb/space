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
  if(input_key_down(SAPP_KEYCODE_UP))
  {
    player->pos.x+=player->dir.x*0.2f;
    player->pos.y+=player->dir.y*0.2f;
  }
  if(input_key_down(SAPP_KEYCODE_DOWN))
  {
    player->pos.x-=player->dir.x*0.2f;
    player->pos.y-=player->dir.y*0.2f;
  }
}
