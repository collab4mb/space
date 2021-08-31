//External includes
//-------------------------------------

//Internal includes
//-------------------------------------

//#defines
#define MAX_SPEED (0.2f)
#define MAX_SPEED2 (MAX_SPEED*MAX_SPEED)
//-------------------------------------

//Typedefs
//-------------------------------------

//Function prototypes

//Can be found in ai_type.h

//-------------------------------------

//Variables
//-------------------------------------

//Function implementations

void _ai_idle(Ent *ent) {
  if(magmag2(sub2(ent->pos,state->player->pos))<m_square(50.0f)&&
     dot2(vec2_swap(vec2_rot(ent->angle)),norm2(sub2(state->player->pos,ent->pos))) > 0.5) {
    ent->ai.target = get_gendex(state->player);
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_move);
    return;
  }
}

static void _ai_move(Ent *ent) {
  Ent *target = try_gendex(ent->ai.target);

  //If target has been destroyed, revert to being idle
  if(target==NULL) {
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_idle);
    return;
  }

  //Rotate towards target
  ent->angle = lerp_rad(ent->angle,rot_vec2(vec2_swap(sub2(target->pos,ent->pos))),0.04f);

  //Accelerate towards target, until close enough, then attack
  ent->vel = add2(ent->vel,mul2_f(vec2_swap(vec2_rot(ent->angle)),0.025f));
  if (magmag2(ent->vel)>MAX_SPEED2)
    ent->vel = mul2_f(norm2(ent->vel),MAX_SPEED);
  if(magmag2(sub2(ent->pos,target->pos))<=m_square(10.f)) {
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_attack);
    return;
  }

  //Loose interest if target too far away
  if(magmag2(sub2(ent->pos,target->pos))>=m_square(80.f)) {
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_idle);
    return;
  }
}

static void _ai_attack(Ent *ent) {
  Ent *target = try_gendex(ent->ai.target);

  //If target has been destroyed, revert to being idle
  if(target==NULL) {
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_idle);
    return;
  }

  //Shoot
  Vec2 e_dir = vec2_swap(vec2_rot(ent->angle));
  Ent *e = add_ent((Ent) {
    .art = Art_Asteroid,
    .pos = add2(ent->pos,mul2_f(e_dir,2.5f)),
    .vel = add2(ent->vel,mul2_f(e_dir,0.6f)),
    .scale = vec3_f(0.2f),
    .height = -0.8,
    .collider.size = 0.2f,
    .collider.weight = 1.0f,
    .damage = ent->damage,
    .parent = get_gendex(ent),
  });
  give_ent_prop(e, EntProp_Projectile);
}

static void _ai_attack_idle(Ent *ent) {
  Ent *target = try_gendex(ent->ai.target);

  //If target has been destroyed, revert to being idle
  if(target==NULL) {
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_idle);
    return;
  }

  //Rotate towards target
  ent->angle = lerp_rad(ent->angle,rot_vec2(vec2_swap(sub2(target->pos,ent->pos))),0.04f);

  //Actively decelerate towards standing still
  ent->vel = mul2_f(ent->vel, 0.9f);

  //Revert to moving towards player if too far away
  if(magmag2(sub2(ent->pos,target->pos))>m_square(25.f)) {
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_move);
    return;
  }
}

static void _ai_death_split(Ent *ent) {
  Ent old = *ent;
  for (int i = 0; i < 2; i++) {
    float sign = i ? -1.0f : 1.0f;
    Ent *ne;
    if (old.collider.size > 0.4f) {
      ne = add_ent(old);
      ne->scale = sub3_f(ne->scale, 0.3f);
      ne->collider.size -= 0.3f;
      ne->health = 1;
      ne->passive_rotate_axis = rand3();
      ai_init(ne,AI_TYPE_ASTEROID);
    } else {
      ne = add_ent((Ent) { .art = Art_Mineral });
      ne->pick_up_after_tick = state->tick + 10;
      give_ent_prop(ne, EntProp_PickUp);
    }

    ne->pos = add2_f(old.pos, old.collider.size/2.0f * sign);
    ne->vel = mul2_f(old.vel, sign);
  }

  remove_ent(ent);
}

static void _ai_death_loot(Ent *ent) {
  Ent old = *ent;
  for (int i = 0; i < 2; i++) {
    float sign = i ? -1.0f : 1.0f;
    Ent *ne;
    ne = add_ent((Ent) { .art = Art_Mineral });
    ne->pick_up_after_tick = state->tick + 10;
    give_ent_prop(ne, EntProp_PickUp);

    ne->pos = add2_f(old.pos, old.collider.size/2.0f * sign);
    ne->vel = mul2_f(old.vel, sign);
  }

  remove_ent(ent);
}

static void _ai_set_state(Ent *ent, AI_statenum nstate) {
  ent->ai.state = &_ai_state[nstate];
  ent->ai.tick_end = state->tick+ent->ai.state->ticks;

  if(ent->ai.state->action!=NULL)
    ent->ai.state->action(ent);
}

static void _ai_run_state(Ent *ent) {
  if(ent->ai.state->action!=NULL)
      ent->ai.state->action(ent);
}

static void ai_init(Ent *ent, AI_type type) {
  ent->ai.type = type;
  ent->ai.state = &_ai_state[_ai_entinfo[ent->ai.type].state_idle];
  ent->ai.tick_end = state->tick+ent->ai.state->ticks;
}

static void ai_run(Ent *ent) {
  if(ent->ai.state==NULL)
    return;

  if(state->tick>=ent->ai.tick_end)
    _ai_set_state(ent,ent->ai.state->next);
  else
    _ai_run_state(ent);
}

static void ai_kill(Ent *ent) {
  //TODO: call function for dropping loot here

  _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_death);
}

static void ai_damage(Ent *to, Ent *from, GenDex *source) {
  if(!has_ent_prop(to,EntProp_Destructible))
    return;

  //Go after attacker
  Ent *target = try_gendex(to->ai.target);
  if(target==NULL&&try_gendex(*source)!=NULL) {
    to->ai.target = *source;
    _ai_set_state(to,_ai_entinfo[to->ai.type].state_move);
  }

  to->health-=from->damage;
  if(to->health<=0)
    ai_kill(to);
}
//-------------------------------------

#undef MAX_SPEED
#undef MAX_SPEED2
