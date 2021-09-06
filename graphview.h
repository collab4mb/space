// namespacing
#define gv(n) graphview_##n
#define _gv(n) _graphview_##n

#define MAX_NODES 1 << 9
#define MAX_CONNS 1 << 10
#define DEFAULT_SIZE 70

typedef struct gv(Node) {
  bool present;
  int x, y;
  // Size is assumed to be 70, minus margin, 50
  struct gv(Node) *first_child;
  struct gv(Node) *sibling;
} gv(Node);

typedef struct {
  gv(Node) nodes[MAX_NODES]; 
} gv(State);

gv(State) gv(init)() {
  gv(State) state = { 0 };
  state.nodes[0] = (gv(Node)) {
    .present = true,
    .first_child = &state.nodes[1]
  };
  state.nodes[1] = (gv(Node)) {
    .present = true,
    .sibling = &state.nodes[2],
    .first_child = &state.nodes[3]
  };
  state.nodes[3] = (gv(Node)) {
    .present = true,
  };
  state.nodes[2] = (gv(Node)) {
    .present = true,
    .sibling = &state.nodes[4]
  };
  state.nodes[4] = (gv(Node)) {
    .present = true,
    .first_child = &state.nodes[5]
  };
  state.nodes[5] = (gv(Node)) {
    .present = true,
    .sibling = &state.nodes[6]
  };
  state.nodes[6] = (gv(Node)) {
    .present = true,
  };
  return state;
}

int _gv(find_span)(gv(Node) *node) {
  int acc = 0;
  for (gv(Node) *iter = node->first_child; iter; iter = iter->sibling) {
    acc += _gv(find_span)(iter);
  }
  return m_max(acc, DEFAULT_SIZE);
}

void _gv(find_positions)(gv(Node) *node, size_t depth, int offs) {
  node->x = (_gv(find_span)(node)-DEFAULT_SIZE)/2+offs;
  // WHY THE FUCK THIS RESETS TO 0 IN GCC????
  // WHY
  node->y = (int)(depth*DEFAULT_SIZE);
  for (gv(Node) *iter = node->first_child; iter; iter = iter->sibling) {
    _gv(find_positions)(iter, depth+1, offs);
    offs += DEFAULT_SIZE;
  }
}

void gv(draw)(gv(State) *state) {
  _graphview_find_positions(&state->nodes[0], 0, 0);
  for (size_t i = 0; i < MAX_NODES; i += 1) {
    gv(Node) *node = &state->nodes[i];
    // doesn't exist
    if (!node->present) continue;
    char buf[32] = { 0 };
    sprintf(buf, "%zu", i);
    ol_draw_rect(vec4(0.0, 0.0, 1.0, 0.5), (ol_Rect) {
      .x = node->x+5+50,
      .y = node->y+5+50,
      .w = DEFAULT_SIZE-10,
      .h = DEFAULT_SIZE-10
    });
    ol_draw_text(_ui_state.font, buf, node->x+5+50, node->y+5+50, vec4(1.0, 1.0, 1.0, 1.0));
  }
}

#undef gv
#undef _gv
#undef MAX_NODES
