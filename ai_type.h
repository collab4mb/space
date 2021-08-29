//External includes
//-------------------------------------

//Internal includes
//-------------------------------------

//#defines
//-------------------------------------

//Typedefs
typedef enum {
  AI_STATE_SHIP_IDLE,
  AI_STATE_SHIP_MOVE,
  AI_STATE_SHIP_ATTACK0,
  AI_STATE_SHIP_ATTACK1,
  AI_STATE_ASTEROID_IDLE,
  AI_STATE_ASTEROID_DEATH,
  AI_STATE_MAX,
}AI_statenum;

//This ai type is ONLY for identifying from which _ai_entinfo entry to read necessary
//variables for simulating ai from. It does NOT dictate the appearance and properties of an entity
typedef enum {
  AI_TYPE_DSHIP,
  AI_TYPE_ASTEROID,
  AI_TYPE_MAX,
}AI_type;

typedef void (*_ai_func_p1)(void *);

//'next' defines to what state the entity should switch after this state is over (TODO: implement time limit for states)
//'action' gets called every frame for an entity and defines its behaviour, can change its state
typedef struct {
  AI_statenum next;
  _ai_func_p1 action;
  uint64_t ticks;
}AI_state;

//Used for looking up basic states for a given entity.
//This allows for the creation of more generalized ai functions.
typedef struct {
  AI_statenum state_idle;
  AI_statenum state_move;
  AI_statenum state_attack;
  AI_statenum state_death;
}AI_info;
//-------------------------------------

//Function prototypes
static void ai_init(void *param0, AI_type type);
static void ai_run(void *param0);
static void _ai_idle(void *param0);
static void _ai_static_idle(void *param0);
static void _ai_move(void *param0);
static void _ai_attack_idle(void *param0);
static void _ai_attack(void *param0);
static void _ai_death_split(void *param0);
static void _ai_set_state(void *param0, AI_statenum nstate);
static void _ai_run_state(void *param0);
//-------------------------------------

//Variables

//This array will contain the possible states of every possible ai type.
//Having this in a central place allows for easy tweaking of AI behaviour.
static const AI_state _ai_state[AI_STATE_MAX] = {
  { .next = AI_STATE_SHIP_IDLE, .action = _ai_idle, .ticks = 0},              //STATE_SHIP_IDLE
  { .next = AI_STATE_SHIP_MOVE, .action = _ai_move, .ticks = 0},              //STATE_SHIP_MOVE
  { .next = AI_STATE_SHIP_ATTACK1, .action = _ai_attack_idle, .ticks = 30},   //STATE_SHIP_ATTACK0
  { .next = AI_STATE_SHIP_ATTACK0, .action = _ai_attack, .ticks = 0},         //STATE_SHIP_ATTACK1
  { .next = AI_STATE_ASTEROID_IDLE, .action = _ai_static_idle, .ticks = 0},   //STATE_ASTEROID_IDLE
  { .next = AI_STATE_ASTEROID_DEATH, .action = _ai_death_split, .ticks = 0},  //STATE_ASTEROID_DEATH
};

static const AI_info _ai_entinfo[AI_TYPE_MAX] = {
  //AI_TYPE_DSHIP
  {
    .state_idle = AI_STATE_SHIP_IDLE,
    .state_move = AI_STATE_SHIP_MOVE,
    .state_attack = AI_STATE_SHIP_ATTACK0,
  },
  //AI_TYPE_ASTEROID
  {
    .state_idle = AI_STATE_ASTEROID_IDLE,
    .state_move = AI_STATE_ASTEROID_IDLE,
    .state_attack = AI_STATE_ASTEROID_IDLE,
    .state_death = AI_STATE_ASTEROID_DEATH,
  },
};
//-------------------------------------

//Function implementations

//Can be found in ai.h

//-------------------------------------
