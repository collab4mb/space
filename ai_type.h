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
  AI_STATE_SHIP_ATTACK,
  AI_STATE_MAX,
}AI_statenum;

//This ai type is ONLY for identifying from which _ai_entinfo entry to read necessary
//variables for simulating ai from. It does NOT dictate the appearance and properties of an entity
typedef enum {
  AI_TYPE_DSHIP,
  AI_TYPE_MAX,
}AI_type;

typedef void (*_ai_func_p1)(void *);

typedef struct {
  AI_statenum next;
  _ai_func_p1 action;
}AI_state;

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
static void _ai_move(void *param0);
static void _ai_attack(void *param0);
static void _ai_set_state(void *param0, AI_statenum state);
//-------------------------------------

//Variables
static const AI_state _ai_state[AI_STATE_MAX] = {
  { .next = AI_STATE_SHIP_IDLE, .action = _ai_idle, }, //STATE_SHIP_IDLE
  { .next = AI_STATE_SHIP_MOVE, .action = _ai_move, }, //STATE_SHIP_MOVE
  { .next = AI_STATE_SHIP_ATTACK, .action = _ai_attack, }, //STATE_SHIP_ATTACK
};

static const AI_info _ai_entinfo[AI_TYPE_MAX] = {
  { //AI_TYPE_DSHIP
    .state_idle = AI_STATE_SHIP_IDLE,
    .state_move = AI_STATE_SHIP_MOVE,
    .state_attack = AI_STATE_SHIP_ATTACK,
  },
};
//-------------------------------------

//Function implementations

//Can be found in ai.h

//-------------------------------------
