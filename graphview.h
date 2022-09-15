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
  // Some other data
  ol_Rect icon_rect;
  const char *name;
  int tier;
} gv(Node);

typedef struct gv(Conn) {
  bool present;
  gv(Node) *from;
  gv(Node) *to;
} gv(Conn);

typedef struct {
  bool scrolling;
  int scroll_x, scroll_y;
  gv(Node) *node_hovered;
  gv(Node) nodes[MAX_NODES]; 
  gv(Conn) conns[MAX_CONNS];
  ol_Image atlas;
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
  node->y = ((int)(depth-1)*DEFAULT_SIZE);
  node->depth = depth;
  for (gv(Node) *iter = node->first_child; iter; iter = iter->sibling) {
    offs += _gv(find_positions)(state, iter, depth+1, offs);
  }
  return _gv(find_span)(state, node);
}

void _gv(draw_line)(gv(State) *state, bool inv, gv(Node) *from, gv(Node) *to) {
  int sx = from->x + DEFAULT_SIZE/2;
  int sy = from->y + DEFAULT_SIZE/2;

  int dx = to->x + DEFAULT_SIZE/2;
  int dy = to->y + DEFAULT_SIZE/2;

  Vec4 color = vec4(1.0, 1.0, 1.0, 0.5);

  if ((inv ? to : from) == state->node_hovered) {
    color = vec4(0.0, 1.0, 0.0, 0.5);
  }
  if ((inv ? from : to) == state->node_hovered) {
    color = vec4(1.0, 0.0, 0.0, 0.5);
  }
  

  int offs = 0;
  if (dy-sy < 0) offs += 2; 

  ui_freeform_at(sx-1, sy-1);
  ui_rect(color, dx-sx, 2);
  ui_freeform_at(dx-1, sy-1+offs);
  ui_rect(color, 2, dy-sy-offs);
}

void gv(draw_lines)(gv(State) *state, gv(Node) *node) {
  for (gv(Node) *iter = node->first_child; iter; iter = iter->sibling) {
    _gv(draw_line)(state, false, node, iter);
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

bool gv(process_events)(gv(State) *state, const sapp_event *ev) {
  if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_MIDDLE) {
    state->scrolling = true;
  }
  if (ev->type == SAPP_EVENTTYPE_MOUSE_UP   && ev->mouse_button == SAPP_MOUSEBUTTON_MIDDLE) {
    state->scrolling = false;
  }
  if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE && state->scrolling) {
    state->scroll_x += (int)ev->mouse_dx;
    state->scroll_y += (int)ev->mouse_dy;
  }
}

void gv(draw_tooltip)(gv(State) *state) {
  if (state->node_hovered == NULL) return;
  struct gv(Node) *node = state->node_hovered;
  int tooltip_x = node->x+DEFAULT_SIZE;
  int tooltip_y = node->y;
  ui_freeform_at(tooltip_x, tooltip_y);
  ui_button(node->name);
}

static void _gv(print_roman)(char *dest, int num) {
  if (num < 0) {
    *dest++ = '-';
    num = -num;
  }
  // I'm too lazy to actually make a converter...
  const char *literals[] = {
    "0",
    "I",
    "II",
    "III",
    "IV",
    "V",
    "VI",
    "VII",
    "VIII",
    "IX",
    "X",
  };
  if (num <= 10) strcpy(dest, literals[num]);
  else sprintf(dest, "%d", num);
}

void gv(draw)(gv(State) *state) {
  _graphview_find_positions(state, &state->nodes[0], 0, 0);
  _gv(relocate_y)(state);
  _gv(squeeze)(state, &state->nodes[6], &state->nodes[2]);
  ui_freeform(sapp_width(), sapp_height());
  ui_rect(vec4(0.0, 0.0, 0.0, 0.6f), ui_rel_x(1.0), ui_rel_y(1.0));
  ui_freeform_at(state->scroll_x, state->scroll_y);
  ui_freeform(sapp_width(), sapp_height());
  bool hit = false;
  for (size_t i = 0; i < MAX_CONNS; i += 1) {
    gv(Conn) *conn = &state->conns[i];
    if (!conn->present) continue;
    _gv(draw_line)(state, true, conn->to, conn->from);
  }
  for (size_t i = 1; i < MAX_NODES; i += 1) {
    gv(Node) *node = &state->nodes[i];
    if (!node->present) continue;
    gv(draw_lines)(state, node);
  }
  for (size_t i = 1; i < MAX_NODES; i += 1) {
    gv(Node) *node = &state->nodes[i];
    if (!node->present) continue;
    char buf[32] = { 0 };
    _gv(print_roman)(buf, node->tier);
    ui_freeform_at(node->x, node->y);
    ui_screen(DEFAULT_SIZE, DEFAULT_SIZE);
      ui_margin(5);
      if (ui_frame(DEFAULT_SIZE, DEFAULT_SIZE, state->node_hovered == node ? ui_Frame_HexSelected : ui_Frame_Hex)) {
        state->node_hovered = node;
        hit = true;
      }
      ui_screen_anchor_xy(0.5, 0.5);
      ui_image_part(&state->atlas, node->icon_rect);
      ui_screen_anchor_xy(0.8f, 0.2f);
      ui_textf(buf);
    ui_screen_end();
  }
  if (!hit)
    state->node_hovered = 0;
  gv(draw_tooltip)(state);
  ui_freeform_end();
  ui_freeform_end();
}

#undef gv
#undef _gv
#undef MAX_NODE 
