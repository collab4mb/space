static uint8_t _input_new_key_state[SAPP_MAX_KEYCODES];
static uint8_t _input_old_key_state[SAPP_MAX_KEYCODES];
static uint8_t _input_new_mouse_state[SAPP_MAX_MOUSEBUTTONS];
static uint8_t _input_old_mouse_state[SAPP_MAX_MOUSEBUTTONS];

//Call at end of frame
static void input_update() {
  memcpy(_input_old_key_state,_input_new_key_state,sizeof(_input_new_key_state));
  memcpy(_input_old_mouse_state,_input_new_mouse_state,sizeof(_input_new_mouse_state));
}

static void input_key_update(sapp_keycode key, int down) {
    _input_new_key_state[key] = (uint8_t)down;
}

static void input_mouse_update(sapp_mousebutton button, int down) {
  if(button!=SAPP_MOUSEBUTTON_INVALID)
    _input_new_mouse_state[button] = (uint8_t)down;
}

static int input_key_down(sapp_keycode key) {
  return _input_new_key_state[key];  
}

static int input_key_pressed(sapp_keycode key) {
  return _input_new_key_state[key]&&!_input_old_key_state[key];
}

static int input_key_released(sapp_keycode key) {
  return !_input_new_key_state[key]&&_input_old_key_state[key];
}

static int input_mouse_down(sapp_mousebutton key) {
  if(key==SAPP_MOUSEBUTTON_INVALID)
    return 0;
  return _input_new_mouse_state[key];  
}

static int input_mouse_pressed(sapp_mousebutton key) {
  if(key==SAPP_MOUSEBUTTON_INVALID)
    return 0;
  return _input_new_mouse_state[key]&&!_input_old_mouse_state[key];
}

static int input_mouse_released(sapp_mousebutton key) {
  if(key==SAPP_MOUSEBUTTON_INVALID)
    return 0;
  return !_input_new_mouse_state[key]&&_input_old_mouse_state[key];
}
