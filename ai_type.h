//External includes
//-------------------------------------

//Internal includes
//-------------------------------------

//#defines
//-------------------------------------

//Typedefs
typedef enum {
  AI_STATE_SHIP_IDLE,
  AI_STATE_MAX,
}AI_statenum;

//This ai type is ONLY for identifying from which _ai_eninfo entry to read necessary
//variables for simulating ai from. It does NOT dictate the appearance and properties of an entity
typedef enum {
  AI_TYPE_DSHIP,
  AI_TYPE_MAX,
}AI_type;

typedef void (*func_p0)();
typedef void (*func_p1)(void *);
typedef void (*func_p2)(void *, void *);

typedef union {
  func_p0 p0; 
  func_p1 p1; 
  func_p2 p2; 
}AI_action;

typedef struct {
  AI_statenum next;
  AI_action action;
}AI_state;

typedef struct {
  AI_statenum state_spawn;
  AI_statenum state_see;
  AI_statenum state_attack;
  AI_statenum state_death;
}AI_info;
//-------------------------------------

//Function prototypes
static void ai_init(void *param0, AI_type type);
static void ai_run(void *param0);
static void _ai_idle(void *param0);
static void _ai_set_state(void *param0, AI_statenum state);
//-------------------------------------

//Variables
static AI_state _ai_state[AI_STATE_MAX] = {
  { .next = AI_STATE_SHIP_IDLE, .action.p1 = _ai_idle, }, //STATE_SHIP_IDLE
};

static AI_info _ai_entinfo[AI_TYPE_MAX] = {
  { //ENT_TYPE_DSHIP
    .state_spawn = AI_STATE_SHIP_IDLE,
  },
};
//-------------------------------------

//Function implementations

//Can be found in ai.h

//-------------------------------------
