//External includes
//-------------------------------------

//Internal includes
//-------------------------------------

//#defines
//-------------------------------------

//Typedefs
typedef enum {
  AI_STATE_NULL,
  AI_STATE_IDLE,
  AI_STATE_MOVE,
  AI_STATE_ATTACK0,
  AI_STATE_ATTACK1,
  AI_STATE_MAX,
}AI_statenum;

typedef AI_statenum (*_ai_func_p1)(Ent *);

//'next' defines to what state the entity should switch after this state is over (TODO: implement time limit for states)
//'action' gets called every frame for an entity and defines its behaviour, can change its state
typedef struct {
  AI_statenum next;
  _ai_func_p1 action;
  uint64_t ticks;
}AI_state;
//-------------------------------------

//Function prototypes
static void ai_init(Ent *ent, AI_statenum sstate);
//TODO: rename?
static void ai_damage(Ent *to, GenDex *source);
static void ai_run(Ent *ent);

//Internal
static AI_statenum _ai_idle(Ent *ent);
static AI_statenum _ai_move(Ent *ent);
static AI_statenum _ai_attack_idle(Ent *ent);
static AI_statenum _ai_attack(Ent *ent);
static AI_statenum _ai_set_state(Ent *ent, AI_statenum nstate);
static AI_statenum _ai_run_state(Ent *ent);
//-------------------------------------

//Variables

//This array will contain the possible states of every possible ai type.
//Having this in a central place allows for easy tweaking of AI behaviour.
static const AI_state _ai_state[AI_STATE_MAX] = {
  { .next = AI_STATE_NULL, .action = NULL, .ticks = 0},              //STATE_NULL
  { .next = AI_STATE_IDLE, .action = _ai_idle, .ticks = 0},              //STATE_IDLE
  { .next = AI_STATE_MOVE, .action = _ai_move, .ticks = 0},              //STATE_MOVE
  { .next = AI_STATE_ATTACK1, .action = _ai_attack_idle, .ticks = 30},   //STATE_ATTACK0
  { .next = AI_STATE_ATTACK0, .action = _ai_attack, .ticks = 0},         //STATE_ATTACK1
};
//-------------------------------------

//Function implementations

//Can be found in ai.h

//-------------------------------------
