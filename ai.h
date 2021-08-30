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

void _ai_idle(void *param0) {
  Ent *ent = (Ent *)param0;

  if(magmag2(sub2(ent->pos,state->player->pos))<m_square(50.0f)&&
     dot2(vec2_swap(vec2_rot(ent->angle)),norm2(sub2(state->player->pos,ent->pos))) > 0.5) {
    ent->ai.target = get_gendex(state->player);
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_move);
    return;
  }
}

static void _ai_move(void *param0) {
  Ent *ent = (Ent *)param0;
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

static void _ai_attack(void *param0) {
  Ent *ent = (Ent *)param0;
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

  //Shoot
  Vec2 e_dir = vec2_swap(vec2_rot(ent->angle));
  Ent *e = add_ent((Ent) {
    .art = Art_Asteroid,
    .pos = add2(ent->pos,mul2_f(e_dir,2.2f)),
    .vel = add2(ent->vel,mul2_f(e_dir,0.6f)),
    .scale = vec3_f(0.2f),
    .height = -0.8,
    .collider.size = 0.2f,
    .collider.weight = 1.0f,
  });
  give_ent_prop(e, EntProp_Projectile);

  //Revert to moving towards player if too far away
  if(magmag2(sub2(ent->pos,target->pos))>m_square(25.f)) {
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_move);
    return;
  }
}

static void _ai_attack_idle(void *param0) {
  Ent *ent = (Ent *)param0;
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

static void _ai_set_state(void *param0, AI_statenum nstate) {
  Ent *ent = (Ent *)param0;

  ent->ai.state = &_ai_state[nstate];
  ent->ai.tick_end = state->tick+ent->ai.state->ticks;

  do {
    if(ent->ai.state->action!=NULL)
      ent->ai.state->action(ent);
    if(ent->ai.tick>0)
      ent->ai.tick--;
  } while(ent->ai.tick>0);
}

static void _ai_run_state(void *param0) {
  Ent *ent = (Ent *)param0;

  do {
    if(ent->ai.state->action!=NULL)
      ent->ai.state->action(ent);
    if(ent->ai.tick>0)
      ent->ai.tick--;
  } while(ent->ai.tick>0);
}

static void ai_init(void *param0, AI_type type) {
  Ent *ent = (Ent *)param0;

  ent->ai.type = type;
  ent->ai.state = &_ai_state[_ai_entinfo[ent->ai.type].state_idle];
  ent->ai.tick_end = state->tick+ent->ai.state->ticks;
}

static void ai_run(void *param0) {
  Ent *ent = (Ent *)param0;
  ent->ai.tick++;

  if(state->tick>=ent->ai.tick_end)
    _ai_set_state(ent,ent->ai.state->next);
  else
    _ai_run_state(ent);
}
//-------------------------------------

#undef MAX_SPEED
#undef MAX_SPEED2