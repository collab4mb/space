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

  if(input_key_pressed(SAPP_KEYCODE_A)) {
    ent->ai.target = state->player;
    ent->ai.target_gen = ent->ai.target->generation;
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_move);
  }
}

static void _ai_move(void *param0) {
  Ent *ent = (Ent *)param0;

  //If target has been destroyed, revert to being idle
  if(ent->ai.target==NULL||ent->ai.target->generation!=ent->ai.target_gen) {
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_idle);
  }

  //Rotate towards target
  ent->angle = lerp_rad(ent->angle,rot_vec2(vec2_swap(sub2(ent->ai.target->pos,ent->pos))),0.04f);

  //Accelerate towards target, until close enough, then attack
  ent->vel = add2(ent->vel,mul2_f(vec2_swap(vec2_rot(ent->angle)),0.025f));
  if (magmag2(ent->vel)>MAX_SPEED2)
    ent->vel = mul2_f(norm2(ent->vel),MAX_SPEED);
  if(magmag2(sub2(ent->pos,ent->ai.target->pos))<=m_square(10.f))
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_attack);
}

static void _ai_attack(void *param0) {
  Ent *ent = (Ent *)param0;

  //If target has been destroyed, revert to being idle
  if(ent->ai.target==NULL||ent->ai.target->generation!=ent->ai.target_gen)
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_idle);

  //Rotate towards target
  ent->angle = lerp_rad(ent->angle,rot_vec2(vec2_swap(sub2(ent->ai.target->pos,ent->pos))),0.04f);

  //Actively decelerate towards standing still
  ent->vel = mul2_f(ent->vel, 0.9f);

  //Shoot
  //TODO: implement too attack states (TYPE_ATTACK0,TYPE_ATTACK1). One has a duration of 0, the other a duration of 'shot delay' (TODO: implement delta time).
  //In ATTACK0 the actuall attack takes place, in ATTACK1 the entity 'idles'.
  if(input_key_pressed(SAPP_KEYCODE_A)) {
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
  }

  //Revert to moving towards player if too far away
  if(magmag2(sub2(ent->pos,ent->ai.target->pos))>m_square(25.f))
    _ai_set_state(ent,_ai_entinfo[ent->ai.type].state_move);
}

static void _ai_set_state(void *param0, AI_statenum state) {
  Ent *ent = (Ent *)param0;

  ent->ai.state = &_ai_state[state];

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
}

static void ai_run(void *param0) {
  Ent *ent = (Ent *)param0;
  ent->ai.tick++;

  _ai_set_state(ent,ent->ai.state->next);
}
//-------------------------------------

#undef MAX_SPEED
#undef MAX_SPEED2
