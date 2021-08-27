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

      //For projectiles we only want the first collision
      if(has_ent_prop(ac,EntProp_Projectile))
        break;
    }
  }

  if (ent != NULL
      && has_ent_prop(ent, EntProp_Destructible)
      && has_ent_prop(ac,  EntProp_Projectile)) {
    take_ent_prop(ac, EntProp_Active);
    Ent old = *ent;
    take_ent_prop(ent, EntProp_Active);

    //TODO: somehow dictate what should happen if a entity gets destroyed/dies (just vanish, split, spawn a different entity etc)
    //For now, just create two smaller asteroids in place of the old one.
    if (old.scale > 0.4f)
      for (int i = 0; i < 2; i++) {
        float sign = i ? -1.0f : 1.0f;
        Ent *ne = add_ent(old);
        ne->pos = add2_f(old.pos, old.size/2.0f * sign);
        ne->vel = mul2_f(old.vel, sign);
        ne->scale -= 0.3f;
        ne->size -= 0.3f;
        if (has_ent_prop(ne, EntProp_PassiveRotate))
          ne->passive_rotate_axis = rand3();
      }
  }
}

static void collision_movement_update(Ent *ac) {
  if (has_ent_prop(ac, EntProp_Projectile)) {
    //Instead of just using a timer, the entity gets lighter and lighter.
    //This can prevent (or at least reduce the effect of) some unwanted behaviour (like moving objects in the distance you don't want to move) 
    //when missing the initial target, but the hitting something else in the distance.
    //A missile, that would push a close target massively, will only slightly push a distant
    //target (the player most likely won't be able to see the distant collision anyway).
    ac->weight-=0.01f;
    if(ac->weight<0.0f)
      take_ent_prop(ac,EntProp_Active);
  }
  else {
    float len = mag2(ac->vel);
    ac->vel = mul2_f(ac->vel, 0.96);
  }

  ac->pos = add2(ac->pos, ac->vel);
}
