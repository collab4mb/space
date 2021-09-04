typedef struct {
  Vec2 normal;
  Vec2 corrected_pos;
  int collided;
} _collision_Result;

/* Tests to see if you can get the Ent A to desired_pos without hitting B,
 * and if collision is inevitable, it tells you how close you can get. */
static _collision_Result collision_test(Ent *a, Vec2 desired_pos, Ent *b) {
  if (b->collider.shape == Shape_Circle) {
    Vec2 delta = sub2(desired_pos, b->pos);
    float allsize = a->collider.size + b->collider.size;
    float depth = mag2(delta) - allsize;
    if (depth < 0.0f) {
      Vec2 normal = norm2(delta);
      return (_collision_Result) {
        .corrected_pos = sub2(desired_pos, mul2_f(normal, depth)),
        .normal = normal,
        .collided = 1,
      };
    } else return (_collision_Result) {
      .collided = 0,
      .corrected_pos = desired_pos
    };
  } else if (b->collider.shape == Shape_Line) {
    Vec2 facing = vec2_rot(b->angle);
    Vec2 half_ext = mul2_f(facing, b->collider.size/2.0f);
    Vec2 L0 = add2(b->pos, half_ext);
    Vec2 L1 = sub2(b->pos, half_ext);
    Vec2 p0 = a->pos;
    Vec2 p1 = desired_pos;
    float r = a->collider.size;
    Vec2 L = sub2(L1, L0);
    Vec2 LN = norm2(L);
    
    int check_circle = false;
    Vec2 M0 = {0};
    Vec2 C = {0};
    Vec2 off = {0};
    {
      // M0 is the closest line starting point that is r-away from L0
      // if both points are equidistant from p0, M0 is either L0 or L1,
      // depending on which is closer.
      Vec2 off_a = mul2_f(vec2(-LN.y, LN.x), r);
      Vec2 off_b = mul2_f(vec2(LN.y, -LN.x), r);
      Vec2 l_a = add2(L0, off_a);
      Vec2 l_b = add2(L0, off_b);
      float dl_a = magmag2(sub2(l_a, p0));
      float dl_b = magmag2(sub2(l_b, p0));
      if (dl_a < dl_b) {
        M0 = l_a;
        off = off_a;
      } else if (dl_b < dl_a) {
        M0 = l_b;
        off = off_b;
      } else {
        // We're hitting the closest circle.
        check_circle = true;
        float dL0 = magmag2(sub2(L0, p0));
        float dL1 = magmag2(sub2(L1, p0));
        if (dL0 < dL1)
          C = L0;
        else C = L1;
      }
      
      // If we're still unsure whether we hit a circle or a line...
      int collided = false;
      Vec2 I = {0};
      Vec2 N = {0};
      if (!check_circle) {
        Vec2 M1 = add2(M0, L);
        
        // Determine intersections between p0..p1 and M0..M1
        float tp, tM;
        
        float denom = (p0.x-p1.x)*(M0.y-M1.y) - (p0.y-p1.y)*(M0.x-M1.x);
        if (denom != 0) {
          tp = ((p0.x-M0.x)*(M0.y-M1.y) - (p0.y-M0.y)*(M0.x-M1.x)) / denom;
          tM = ((p1.x-p0.x)*(p0.y-M0.y) - (p1.y-p0.y)*(p0.x-M0.x)) / denom;
          
          if (0.0 <= tp && tp <= 1.0) {
            if (0 <= tM && tM <= 1) {
              collided = true;
              N = norm2(off);
              I = lerp2(M0, M1, tM);
              C = sub2(I, off);
              // println(off);
            }
            // Now M0 would be the center of the circle to check
            else if (tM < 0) {
              check_circle = true;
              C = L0;
            }
            else if (tM > 1) {
              check_circle = true;
              C = L1;
            }
          }
        }
      }
      
      if (check_circle) {
         float a = magmag2(sub2(p0, p1));
         float b = -2*dot2(sub2(C, p0), sub2(p1, p0));
         float c = mag2(sub2(C, p0)) - r*r;
         
         float disc = b*b-4*a*c;
         if (disc > 0) {
           float t = (-b-sqrtf(disc))/(2*a);
           if (0<t&&t<1) {
             I = lerp2(p0, p1, t);
             N = norm2(sub2(I, C));
             collided = true;
           }
         }
      }

      return (_collision_Result) {
        .corrected_pos = I,
        .normal = N,
        .collided = collided,
      };
    }
  }
}

static void collision_movement_update(Ent *ac) {
  /* our lines don't move */
  if (ac->collider.shape == Shape_Line) return;
  if (ac->collider.weight == 0.0f) return;

  if (has_ent_prop(ac, EntProp_Projectile)) {
    // Instead of just using a timer, the entity gets lighter and lighter.
    // This can prevent (or at least reduce the effect of) some unwanted behaviour
    // (like moving objects in the distance you don't want to move) 
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

  Vec2 dpos = add2(ac->pos, ac->vel); /* desired position */

  for (Ent *ent = 0; (ent = ent_all_iter(ent));) {
    if (ent == ac || ent->collider.weight <= 0.0f) continue;

    _collision_Result r = collision_test(ac, dpos, ent);
    if (r.collided) {
      float force = mag2(ac->vel);
      float weight_sum = ac->collider.weight + ent->collider.weight;
      float ac_force = ac->collider.weight / weight_sum * force;
      float ent_force = ent->collider.weight / weight_sum * force;
      ac->vel  = add2( ac->vel, mul2_f(r.normal, ent_force));
      ent->vel = sub2(ent->vel, mul2_f(r.normal, ent_force));

      if (ent != NULL
          && has_ent_prop(ent, EntProp_Destructible)
          && has_ent_prop(ac,  EntProp_Projectile)) {
        ent->health -= ac->damage;
        ent->last_hit = state->tick;
        if(has_ent_prop(ent,EntProp_HasAI))
          ai_damage(ent,&ac->parent);
        remove_ent(ac);
      }
    }
    dpos = r.corrected_pos;
  }

  ac->pos = dpos;
}
