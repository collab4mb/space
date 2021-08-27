static float collision_intersects(const Ent *a, const Ent *b) {
  return magmag2(sub2(a->pos, b->pos)) - powf(a->size + b->size, 2);
}

static void collision(Ent *ac) {
  float depth = 0.0f;
  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
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
    }
  }
}

static void collision_movement_update(Ent *ac) {
  float len = mag2(ac->vel);
  ac->vel = mul2_f(ac->vel, 0.96);

  ac->pos = add2(ac->pos, ac->vel);
}
