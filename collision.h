static float collision_intersects(const Ent *a, const Ent *b) {
  return magmag2(sub2(a->pos, b->pos)) - powf(a->size + b->size, 2);
}

static void collision(Ent *ac) {
  float depth = 0.0f;
  Ent *ent = NULL;

  for (ent = 0; (ent = ent_all_iter(ent));) {
    // We can just check the pointers here to see if they are the same entity
    if (ent==ac) continue;

    if ((depth = collision_intersects(ac,ent)) < 0.0f) {
      depth = sqrtf(m_abs(depth));
      Vec2 dir = norm2(sub2(ac->pos, ent->pos));
      float weight_sum = ac->weight + ent->weight;
      float force = mag2(sub2(ac->vel, ent->vel));
      force *= ac->weight / weight_sum;
      force *= depth;
      if (weight_sum!=0.0f)
        ent->vel = sub2(ent->vel, mul2_f(dir, force));
      break;
    }
  }

  if(ent!=NULL&&ent!=state->player&&has_ent_prop(ac,EntProp_Projectile)) {
    take_ent_prop(ac,EntProp_Projectile);
    take_ent_prop(ac,EntProp_Active);
    Ent old = *ent;
    take_ent_prop(ent,EntProp_Active);

    float nscale_delta = old.scale_delta-0.3f;
    if(old.scale_delta>-0.4f) {
      add_ent((Ent) {
        .art = Art_Asteroid,
        .pos = add2_f(old.pos,old.size/2.0f),
        .vel = old.vel,
        .scale_delta = nscale_delta,
        .size = 1.0f+nscale_delta,
        .weight = 1.0f,
      });
      add_ent((Ent) {
        .art = Art_Asteroid,
        .pos = sub2_f(old.pos,old.size/2.0f),
        .vel = mul2_f(old.vel,-1.0f),
        .scale_delta = nscale_delta,
        .size = 1.0f+nscale_delta,
        .weight = 1.0f,
      });
    }
  }
}

static void collision_movement_update(Ent *ac) {
  if (has_ent_prop(ac, EntProp_Projectile)) {
    ac->weight-=0.01f;
    if(ac->weight<0.0f) {
      take_ent_prop(ac,EntProp_Projectile);
      take_ent_prop(ac,EntProp_Active);
    }
  }
  else {
    float len = mag2(ac->vel);
    ac->vel = mul2_f(ac->vel, 0.96);
  }

  ac->pos = add2(ac->pos, ac->vel);
}
