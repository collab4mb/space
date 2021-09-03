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

static AI_statenum _ai_idle(Ent *ent) {
  Ent *player = try_gendex(state->player);

  if(player!=NULL&&magmag2(sub2(ent->pos,player->pos))<m_square(50.0f)&&
     dot2(vec2_swap(vec2_rot(ent->angle)),norm2(sub2(player->pos,ent->pos))) > 0.5) {
    ent->ai.target = state->player;
    return AI_STATE_MOVE;
  }

  return AI_STATE_NULL;
}

static AI_statenum _ai_move(Ent *ent) {
  Ent *target = try_gendex(ent->ai.target);

  //If target has been destroyed, revert to being idle
  if(target==NULL)
    return AI_STATE_IDLE;

  //Rotate towards target
  ent->angle = lerp_rad(ent->angle,rot_vec2(vec2_swap(sub2(target->pos,ent->pos))),0.04f);

  //Accelerate towards target, until close enough, then attack
  ent->vel = add2(ent->vel,mul2_f(vec2_swap(vec2_rot(ent->angle)),0.025f));
  if (magmag2(ent->vel)>MAX_SPEED2)
    ent->vel = mul2_f(norm2(ent->vel),MAX_SPEED);
  if(magmag2(sub2(ent->pos,target->pos))<=m_square(10.f))
    return AI_STATE_ATTACK0;

  //Loose interest if target too far away
  if(magmag2(sub2(ent->pos,target->pos))>=m_square(80.f))
    return AI_STATE_IDLE;

  return AI_STATE_NULL;
}

static AI_statenum _ai_attack(Ent *ent) {
  Ent *target = try_gendex(ent->ai.target);

  //If target has been destroyed, revert to being idle
  if(target==NULL)
    return AI_STATE_IDLE;

  fire_laser(ent);

  return AI_STATE_NULL;
}

static AI_statenum _ai_attack_idle(Ent *ent) {
  Ent *target = try_gendex(ent->ai.target);

  //If target has been destroyed, revert to being idle
  if(target==NULL)
    return AI_STATE_IDLE;

  //Rotate towards target
  ent->angle = lerp_rad(ent->angle,rot_vec2(vec2_swap(sub2(target->pos,ent->pos))),0.04f);

  //Actively decelerate towards standing still
  ent->vel = mul2_f(ent->vel, 0.9f);

  //Revert to moving towards player if too far away
  if(magmag2(sub2(ent->pos,target->pos))>m_square(25.f)) {
    return AI_STATE_MOVE;
  }

  return AI_STATE_NULL;
}

static AI_statenum _ai_set_state(Ent *ent, AI_statenum nstate) {
  ent->ai.state = &_ai_state[nstate];
  ent->ai.tick_end = state->tick+ent->ai.state->ticks;

  if(ent->ai.state->action!=NULL)
    return ent->ai.state->action(ent);
  return AI_STATE_NULL;
}

static AI_statenum _ai_run_state(Ent *ent) {
  if(ent->ai.state->action!=NULL)
      return ent->ai.state->action(ent);
  return AI_STATE_NULL;
}

static void ai_init(Ent *ent, AI_statenum sstate) {
  ent->ai.state = &_ai_state[sstate];
  ent->ai.tick_end = state->tick+ent->ai.state->ticks;
}

static void ai_run(Ent *ent) {
  if(ent->ai.state==NULL)
    return;

  AI_statenum next;
  if(state->tick>=ent->ai.tick_end)
    next = _ai_set_state(ent,ent->ai.state->next);
  else
    next = _ai_run_state(ent);

  while(next!=AI_STATE_NULL)
    next = _ai_set_state(ent,next);
}

static void ai_damage(Ent *to, GenDex *source) {
  if(!has_ent_prop(to,EntProp_Destructible))
    return;

  //Go after attacker
  Ent *target = try_gendex(to->ai.target);
  if(target==NULL&&try_gendex(*source)!=NULL) {
    to->ai.target = *source;
    _ai_set_state(to,AI_STATE_MOVE);
  }
}
//-------------------------------------

#undef MAX_SPEED
#undef MAX_SPEED2
