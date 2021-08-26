static float collision_intersects(const Ent *a, const Ent *b) {
  return magmag2(sub2(a->pos, b->pos)) - powf(a->size+b->size, 2);
}

static void collision(Ent *ac) {
  float depth = 0.0f;
  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
    //We can just check the pointers here to see if they are the same entity
    if(ent==ac)
      continue;

    if((depth = collision_intersects(ac,ent))<0.0f) {
      depth = sqrtf(m_abs(depth));
      Vec2 dir = norm2(sub2(ac->pos,ent->pos));
      float weight_sum = ac->weight+ent->weight;
      float ac_vel_len = mag2(sub2(ac->vel,ent->vel));
      if(weight_sum!=0.0f) {
        ent->vel.x-=dir.x*ac_vel_len*(ac->weight/weight_sum);
        ent->vel.y-=dir.y*ac_vel_len*(ac->weight/weight_sum);
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
