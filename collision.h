/* Input two ents, and pointers to where you want their penetration
 * depth and the normal to separate them to be stored */
static void collision_intersects(Ent *a, Ent *b, float *depth, Vec2 *normal) {
  Shape a_shape = a->collider.shape;
  Shape b_shape = b->collider.shape;
  if (a_shape == Shape_Circle && b_shape == Shape_Circle) {
    Vec2 delta = sub2(a->pos, b->pos);
    *depth = magmag2(delta) - powf(a->collider.size + b->collider.size, 2);
    if (*depth < 0.0f)
      *normal = norm2(delta);
  } else if (a_shape == Shape_Line || b_shape == Shape_Line) {
    Ent *line = (a_shape == Shape_Line) ? a : b;
    Ent *circ = (a_shape == Shape_Circle) ? a : b;

    Vec2 facing = vec2_rot(line->angle);
    Vec2 half_ext = mul2_f(facing, line->collider.size/2.0f);
    Vec2 line_beg = add2(line->pos, half_ext);
    Vec2 line_end = sub2(line->pos, half_ext);

    Vec2 ba = sub2(line_beg, line_end);
    Vec2 pa = sub2(circ->pos, line_end);
    float h = m_clamp(dot2(pa, ba) / dot2(ba, ba), 0.0f, 1.0f);
    *depth = mag2(sub2(pa, mul2_f(ba, h))) - circ->collider.size - 0.5f;
    *normal = vec2(-facing.y, facing.x);
    *normal = mul2_f(*normal, -sign(dot2(*normal, sub2(line->pos, circ->pos))));
  }
}

static void collision(Ent *ac) {
  Collider *a_cl = &ac->collider;

  ac->time_since_last_collision += 0.001f;

  if (ac->collider.size == 0.0f) return;

  Ent *ent = NULL;

  for (ent = 0; (ent = ent_all_iter(ent));) {
    Collider *e_cl = &ent->collider;

    // We can just check the pointers here to see if they are the same entity
    if (ent == ac) continue;
    if (ent->collider.size == 0.0f) continue;

    float depth = 0.0f;
    Vec2 normal = { 0 };
    collision_intersects(ac, ent, &depth, &normal);
    if (depth < 0.0f) {
      /* if stops animation from freezing on first frame if you press against the shape */
      ent->time_since_last_collision = 0.0f;
      ac->time_since_last_collision = 0.0f;

      depth = sqrtf(m_abs(depth));
      float weight_sum = a_cl->weight + e_cl->weight;
      float force = mag2(sub2(ac->vel, ent->vel));
      force *= a_cl->weight / weight_sum;
      force *= depth;
      if (weight_sum!=0.0f)
        ent->vel = sub2(ent->vel, mul2_f(normal, force));

      //For projectiles we only want the first collision
      if(has_ent_prop(ac,EntProp_Projectile))
        break;
    }
  }

  if (ent != NULL
      && has_ent_prop(ent, EntProp_Destructible)
      && has_ent_prop(ac,  EntProp_Projectile)) {
    ent->health-=ac->damage;
    if(has_ent_prop(ent,EntProp_HasAI))
      ai_damage(ent,&ac->parent);

    remove_ent(ac);
  }
}

static void collision_movement_update(Ent *ac) {
  if (has_ent_prop(ac, EntProp_Projectile)) {
    // Instead of just using a timer, the entity gets lighter and lighter.
    // This can prevent (or at least reduce the effect of) some unwanted behaviour (like moving objects in the distance you don't want to move) 
    // when missing the initial target, but the hitting something else in the distance.
    // A missile, that would push a close target massively, will only slightly push a distant
    // target (the player most likely won't be able to see the distant collision anyway).
    ac->collider.weight -= 0.01f;
    if(ac->collider.weight < 0.0f)
      remove_ent(ac);
  }
  else {
    ac->vel = mul2_f(ac->vel, 0.96f);
  }

  ac->pos = add2(ac->pos, ac->vel);
}
