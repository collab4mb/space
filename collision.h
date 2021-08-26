static int collision_intersects(const Ent *a, const Ent *b) {
  return magmag2(sub2(a->pos, b->pos)) <= powf(a->size+b->size, 2);
}

static void collision(Ent *ac) {
  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
    //We can just check the pointers here to see if they are the same entity
    if(ent==ac)
      continue;

    if(collision_intersects(ac,ent)) {
      //Calculate depth
      float dist_x = ac->pos.x-ent->pos.x;
      float dist_y = ac->pos.y-ent->pos.y;
      float depth = sqrtf(dist_x*dist_x+dist_y*dist_y); 
      Vec2 dir = mul2_f(norm2(sub2(ac->pos,ent->pos)),1.0f);
      float wheight_sum = ac->wheight+ent->wheight;
      float ac_vel_len = mag2(sub2(ac->vel,ent->vel));
      if(wheight_sum!=0.0f) {
        ent->vel.x-=dir.x*ac_vel_len*ac->wheight/wheight_sum;
        ent->vel.y-=dir.y*ac_vel_len*ac->wheight/wheight_sum;
      }
    }
  }
}

static void collision_movement_update(Ent *ac) {
  float len = mag2(ac->vel);
  if(len!=0.0f) {
    ac->vel = norm2(ac->vel);
    len-=0.002f;
    len = len<0.0f?0.0f:len;
    ac->vel = mul2_f(ac->vel,len);
  }

  ac->pos.x+=ac->vel.x;
  ac->pos.y+=ac->vel.y;
}
