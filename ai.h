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
    ent->ai_target = state->player;
    _ai_set_state(ent,_ai_entinfo[ent->ai_type].state_move);
  }
}

static void _ai_move(void *param0) {
  Ent *ent = (Ent *)param0;

  if(ent->ai_target==NULL) {
    _ai_set_state(ent,_ai_entinfo[ent->ai_type].state_idle);
  }

  ent->angle = lerp_rad(ent->angle,rot_vec2(vec2_swap(sub2(ent->ai_target->pos,ent->pos))),0.04f);
  ent->vel = add2(ent->vel,mul2_f(vec2_swap(vec2_rot(ent->angle)), 0.025f));
  if (magmag2(ent->vel)>MAX_SPEED2)
    ent->vel = mul2_f(norm2(ent->vel),MAX_SPEED);

  if(magmag2(sub2(ent->pos,ent->ai_target->pos))<=64.f)
    _ai_set_state(ent,_ai_entinfo[ent->ai_type].state_attack);
}

static void _ai_attack(void *param0) {
  Ent *ent = (Ent *)param0;

  if(ent->ai_target==NULL) {
    _ai_set_state(ent,_ai_entinfo[ent->ai_type].state_idle);
  }

  //Rotate towards target
  ent->angle = lerp_rad(ent->angle,rot_vec2(vec2_swap(sub2(ent->ai_target->pos,ent->pos))),0.04f);

  //Activel decelerate towards standing still
  ent->vel = mul2_f(ent->vel, 0.9f);

  //Shoot
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

  if(magmag2(sub2(ent->pos,ent->ai_target->pos))>15.f*15.f)
    _ai_set_state(ent,_ai_entinfo[ent->ai_type].state_move);
}

static void _ai_set_state(void *param0, AI_statenum state) {
  Ent *ent = (Ent *)param0;

  ent->ai_state = &_ai_state[state];

  while(ent->ai_tick>0) {
    ent->ai_tick--;
    if(ent->ai_state->action!=NULL)
      ent->ai_state->action(ent);
  }
}

static void ai_init(void *param0, AI_type type) {
  Ent *ent = (Ent *)param0;

  ent->ai_type = type;
  ent->ai_state = &_ai_state[_ai_entinfo[ent->ai_type].state_idle];
}

static void ai_run(void *param0) {
  Ent *ent = (Ent *)param0;
  ent->ai_tick++;

  _ai_set_state(ent,ent->ai_state->next);
}
//-------------------------------------

#undef MAX_SPEED2
