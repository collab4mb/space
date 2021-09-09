// namespacing
#define gv(n) graphview_##n
#define _gv(n) _graphview_##n

#define MAX_NODES 1 << 9
#define MAX_CONNS 1 << 9
#define DEFAULT_SIZE 70

typedef struct gv(Node) {
  bool present;
  size_t depth;
  int x, y;
  // Size is assumed to be 70, minus margin, 50
  struct gv(Node) *first_child;
  struct gv(Node) *sibling;
} gv(Node);

typedef struct gv(Conn) {
  bool present;
  gv(Node) *from;
  gv(Node) *to;
} gv(Conn);

typedef struct {
  gv(Node) nodes[MAX_NODES]; 
  gv(Conn) conns[MAX_CONNS]; 
} gv(State);

int _gv(find_span)(gv(State) *state, gv(Node) *node) {
  int acc = 0;
  for (gv(Node) *iter = node->first_child; iter; iter = iter->sibling) {
    acc += _gv(find_span)(state, iter);
  }
  return m_max(acc, DEFAULT_SIZE);
}

int _gv(find_positions)(gv(State) *state, gv(Node) *node, size_t depth, int offs) {
  node->x = (_gv(find_span)(state, node)-DEFAULT_SIZE)/2+offs;
  node->y = (int)(depth*DEFAULT_SIZE);
  node->depth = depth;
  for (gv(Node) *iter = node->first_child; iter; iter = iter->sibling) {
    offs += _gv(find_positions)(state, iter, depth+1, offs);
  }
  return _gv(find_span)(state, node);
}

void _gv(draw_line)(gv(Node) *from, gv(Node) *to) {
  int sx = from->x + DEFAULT_SIZE/2+50;
  int sy = from->y + DEFAULT_SIZE/2+50;

  int dx = to->x + DEFAULT_SIZE/2+50;
  int dy = to->y + DEFAULT_SIZE/2+50;
  
  int offs = 0;
  if (dy-sy < 0) offs += 2; 

  ol_draw_rect(vec4(1.0, 1.0, 1.0, 1.0), (ol_Rect) { sx-1, sy-1, dx-sx, 2 });
  ol_draw_rect(vec4(1.0, 1.0, 1.0, 1.0), (ol_Rect) { dx-1, sy-1+offs, 2, dy-sy });
}

void gv(draw_lines)(gv(Node) *node) {
  for (gv(Node) *iter = node->first_child; iter; iter = iter->sibling) {
    _gv(draw_line)(node, iter);
  }
}

// Returns max depth
size_t _gv(squeeze)(gv(State) *state, gv(Node) *from, gv(Node) *to) {
  if (from->x > to->x) {
    // swap
    gv(Node) *tmp = from;
    from = to;
    to = tmp;
  }
  size_t max = m_max(from->depth, to->depth);
  for (size_t i = 0; i < MAX_NODES; i += 1) {
    gv(Node) *node = &state->nodes[i];
    if (!node->present) continue;
    size_t mindep = m_min(from->depth, to->depth);
    if (node->x >= from->x && node->x <= to->x && node->depth >= mindep) {
      max = m_max(node->depth, max);
    }
  }
  return max;
}

void _gv(propagate_depth)(gv(Node) *node, size_t orig, size_t newd) {
  for (gv(Node) *iter = node->first_child; iter; iter = iter->sibling) {
    _gv(propagate_depth)(iter, orig, newd);
    iter->depth = iter->depth-orig+newd;
    iter->y = (int)(iter->depth*DEFAULT_SIZE);
  }
}

void _gv(relocate_y)(gv(State) *state) {
  for (size_t i = 0; i < MAX_CONNS; i += 1) {
    gv(Conn) *conn = &state->conns[i];
    if (!conn->present) continue;
    
    if ((conn->from->depth+1) > conn->to->depth) {
      size_t orig = conn->to->depth;
      conn->to->depth = _gv(squeeze)(state, conn->from, conn->to)+1;
      _gv(propagate_depth)(conn->to, orig, conn->to->depth);
    }
    conn->to->y = (int)(conn->to->depth*DEFAULT_SIZE);
  }
}

void gv(draw)(gv(State) *state) {
  _graphview_find_positions(state, &state->nodes[0], 0, 0);
  _gv(relocate_y)(state);
  _gv(squeeze)(state, &state->nodes[6], &state->nodes[2]);
  for (size_t i = 0; i < MAX_CONNS; i += 1) {
    gv(Conn) *conn = &state->conns[i];
    if (!conn->present) continue;
    _gv(draw_line)(conn->to, conn->from);
  }
  for (size_t i = 0; i < MAX_NODES; i += 1) {
    gv(Node) *node = &state->nodes[i];
    if (!node->present) continue;
    gv(draw_lines)(node);
  }
  for (size_t i = 0; i < MAX_NODES; i += 1) {
    gv(Node) *node = &state->nodes[i];
    // doesn't exist
    if (!node->present) continue;
    char buf[32] = { 0 };
    sprintf(buf, "%zu", i);
    ol_draw_rect(vec4(0.0, 0.0, 1.0, 1.0f), (ol_Rect) {
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
