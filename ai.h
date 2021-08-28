//External includes
//-------------------------------------

//Internal includes
//-------------------------------------

//#defines
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
}

static void _ai_set_state(void *param0, AI_statenum state) {
  Ent *ent = (Ent *)param0;

  ent->ai_state = &_ai_state[state];

  if(ent->ai_state->action.p1)
    ent->ai_state->action.p1(ent);
}

static void ai_init(void *param0, AI_type type) {
  Ent *ent = (Ent *)param0;

  ent->ai_type = type;
  ent->ai_state = &_ai_state[_ai_entinfo[ent->ai_type].state_spawn];
}

static void ai_run(void *param0) {
  Ent *ent = (Ent *)param0;

  _ai_set_state(ent,ent->ai_state->next);
}
//-------------------------------------
