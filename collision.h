static int collision_intersects(const Ent *a, const Ent *b) {
  float dist_x = a->pos.x-b->pos.x;
  float dist_y = a->pos.y-b->pos.y;
  float rad = a->size+b->size;

  if((dist_x*dist_x+dist_y*dist_y)<=rad*rad)
    return 1;
  return 0;
}

static void collision(Ent *ac) {
  float ac_vel_len = mag2(ac->vel);
  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
    //We can just check the pointers here to see if they are the same entity
    if(ent==ac)
      continue;

    if(collision_intersects(ac,ent))
    {
      //Calculate depth
      float dist_x = ac->pos.x-ent->pos.x;
      float dist_y = ac->pos.y-ent->pos.y;
      float depth = sqrtf(dist_x*dist_x+dist_y*dist_y); 
      Vec2 dir = mul2_f(norm2(sub2(ac->pos,ent->pos)),1.0f);
      ent->vel.x-=dir.x*ac_vel_len*0.5f;
      ent->vel.y-=dir.y*ac_vel_len*0.5f;
    }

    float len = mag2(ent->vel);
    if(len!=0.0f) {
      ent->vel = norm2(ent->vel);
      len-=0.002f;
      len = len<0.0f?0.0f:len;
      ent->vel = mul2_f(ent->vel,len);
    }

    ent->pos.x+=ent->vel.x;
    ent->pos.y+=ent->vel.y;
  }
}
