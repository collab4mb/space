/*
 * Tech-tree lisp parser/interp
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

char pcurrent;
const char *psrc;
size_t ppos;
size_t pcol = 0;
size_t pline = 0;

struct View {
  const char *str;
  size_t size;
};

bool vieweq(struct View a, struct View b) {
  if (a.size != b.size) return false;
  return strncmp(a.str, b.str, a.size) == 0;
}

struct View viewof(const char *str) {
  return (struct View) { str, strlen(str) };
}
enum NodeKind {
  NK_INT,
  NK_IDENT,
  NK_GROUP
};

struct Node {
  size_t line;
  size_t col;
  enum NodeKind kind;
  struct Node *sibling;
  union {
    long intval;
    struct View identval;
    struct {
      struct Node *first_child;
    } groupval;
  };
};

#define MAX_NODES 1 << 12
struct Node nodes[MAX_NODES] = { 0 };
size_t nodes_count = 0;

struct Node *allocnode() {
  struct Node *node = &nodes[nodes_count++];
  node->col = pcol;
  node->line = pline;
  return node;
}

void pinit(const char *src) {
  psrc = src;
  pcurrent = src[0];
  ppos = 1;
}

char ppeek() {
  return pcurrent;
}

char pcheck(char with) {
  if (pcurrent == 0) return false;
  return with == pcurrent;
}

char pnotcheck(char with) {
  if (pcurrent == 0) return false;
  return with != pcurrent;
}

void perrn(const char *message, struct Node *node) {
  fprintf(stderr, "\x1b[31mERROR\x1b[0m: %s\nerror occured at line %zu column %zu\n", message, node->line+1, node->col+1);
  exit(-1);
}

void perr(const char *message) {
  fprintf(stderr, "\x1b[31mERROR\x1b[0m: %s\nerror occured at line %zu column %zu\n", message, pline+1, pcol+1);
  exit(-1);
}       

char pnext() {
  char prev = pcurrent;
  if (prev == 0) 
    perr("Unexpected end of file");
  pcol += 1;
  if (prev == '\n') {
    pline += 1;
    pcol = 0;
  }
  pcurrent = psrc[ppos++];
  return prev;
}

bool pskip_comment() {
  size_t orig = ppos;
  // This is a horrible way to do it 
  if (pcheck('#')) {
    pnext();
    if (pcheck('|')) {
      pnext();
      while (true) {
        if (pcheck('|')) {
          pnext();
          if (pcheck('#')) {
            pnext();
            break;
          }
        }
        pnext();
      }
    }
  }
  if (pcheck(';')) {
    while (pnotcheck('\n')) pnext();
  }
  return ppos != orig;
}

bool pskip_blank() {
  size_t orig = ppos;
  while (isspace(ppeek()) && ppeek() != 0)
    pnext();
  return ppos != orig;
}

void pskip_things() {
  while (pskip_blank() || pskip_comment());
}

struct Node *pdecider();

void pgroup(struct Node *node) {
  if (pcheck('(')) 
    pnext();
  else
    perr("Expected '('");
  node->kind = NK_GROUP;
  node->groupval.first_child = pdecider();

  for (node = node->groupval.first_child; pnotcheck(')'); node = node->sibling) {
    node->sibling = pdecider();
  }
  if (pcheck(')')) 
    pnext();
  else
    perr("Expected ')'");
}

// decides what is what
struct Node *pdecider() {
  pskip_things();
  struct Node *node = allocnode();
  node->sibling = NULL;
  if ((ppeek() >= '0' && ppeek() <= '9') || ppeek() == '-') {
    char *end;
    long r = strtol(psrc+ppos-1, &end, 10);
    if (errno == ERANGE) {
      perr("Number too big");
    }
    while (end - (psrc+ppos-1)) pnext();
    node->kind = NK_INT;
    node->intval = r;
  }
  else if (pcheck('"')) {
    perr("`\"` Is reserved for strings"); 
  }
  else if (pcheck('(')) {
    pgroup(node);
  }
  else if (pcheck(')')) {
    perr("Unexpected ')'");
  }
  else {
    node->kind = NK_IDENT;
    const char *start = psrc + ppos - 1;
    size_t size = 0;
    while (
      !(ppeek() == 0 || isspace(ppeek()) || pcheck(';') || pcheck(')'))
    ) {
      pnext();
      size += 1;
    }
    node->identval.str = start;
    node->identval.size = size;
    assert(size > 0 && "Indentifier with size less than 1");
  }
  return node;
}

struct Node *ptoplevel() {
  struct Node *node = allocnode();
  struct Node *orig = node;
  node->kind = NK_GROUP;
  pskip_things();
  if (ppeek() == 0) return node;
  pgroup(node->groupval.first_child = allocnode());
  pskip_things();
  for (node = node->groupval.first_child; ppeek() != 0; node = node->sibling) {
    pgroup(node->sibling = allocnode());
    pskip_things();
  }
  return orig;
}

void dump(struct Node *node) {
  switch (node->kind) {
    case NK_INT:
      printf("%ld", node->intval); 
      break;
    case NK_IDENT:
      printf("%.*s", (int)node->identval.size, node->identval.str);
      break;
    case NK_GROUP:
      printf("(");
      for (struct Node *n = node->groupval.first_child; n; n = n->sibling) {
        dump(n);
        if (n->sibling)
          printf(" ");
      }
      printf(")");
      break;
  }
}

/* * * * * * *       
 * Generator *
 * * * * * * */

struct GraphNode {
  struct View name; 
  long tier;
  // Texture coords
  long tx, ty, tw, th;
  size_t first_child;
  size_t sibling;
  bool has_parent;
};

struct GraphConn {
  size_t from;
  size_t to;
};

#define MAX_GRAPH_NODES 1 << 12
#define MAX_GRAPH_CONNS 1 << 12

struct Generator {
  struct GraphNode nodes[MAX_GRAPH_NODES];
  size_t node_count;
  struct GraphConn conns[MAX_GRAPH_CONNS];
  size_t conn_count;
};

struct Node *previous_node;

void nodecheck(struct Node *node, enum NodeKind kind, const char *text) {
  if (node == NULL) 
    perrn(text, previous_node);
  if (node->kind != kind)
    perrn(text, node);
}

struct Node* nodeiter(struct Node **node, enum NodeKind kind, const char *text) {
  nodecheck(*node, kind, text);
  struct Node *pnode = *node;
  *node = (*node)->sibling;
  return pnode;
}

size_t evaluate(struct Generator *gen, struct Node *node);

size_t connect(size_t src, struct Generator *gen, struct Node *val) {
  if (gen->nodes[src].first_child != SIZE_MAX) {
    fprintf(stderr, "%.*s: ", (int)gen->nodes[src].name.size, gen->nodes[src].name.str);
    perrn("This node has already been assigned to", val);
  }

  gen->nodes[src].first_child = SIZE_MAX-1;

  struct GraphNode *prev = NULL;

  for (val = val->sibling; val; val = val->sibling) {
    size_t dest = evaluate(gen, val);
    if (gen->nodes[dest].sibling != SIZE_MAX) {
      fprintf(stderr, "%.*s: ", (int)gen->nodes[dest].name.size, gen->nodes[dest].name.str);
      perrn("This node has already been used", val);
    }
    gen->nodes[dest].has_parent = true;
    if (gen->nodes[src].first_child == SIZE_MAX-1)
      gen->nodes[src].first_child = dest;
    if (prev)
      prev->sibling = dest;
    prev = &gen->nodes[dest];
  }
  return src;
}

size_t evaluate(struct Generator *gen, struct Node *node) {
  previous_node = node;
  if (node->kind == NK_IDENT) {
    for (size_t i = 0; i < gen->node_count; i += 1) {
      if (vieweq(gen->nodes[i].name, node->identval)) {
        return i;
      }
    }
    fprintf(stderr, "%.*s: ", (int)node->identval.size, node->identval.str);
    perrn("No such node declared", node);
  }
  if (node->kind == NK_GROUP) {
    struct Node *cmd = node->groupval.first_child;
    nodecheck(cmd, NK_IDENT, "Can't call on non-identifier");
    struct Node *val = cmd->sibling;
    if (*cmd->identval.str == '/') {
      if (vieweq(cmd->identval, viewof("/join"))) {
        struct Node *requires = nodeiter(&val, NK_GROUP, "Expected list of dependencies")->groupval.first_child;
        size_t parn = evaluate(gen, requires);

        size_t children_ids[1024] = { 0 };
        size_t id_count = 0;

        val = cmd->sibling;
        for (val = val->sibling; val; val = val->sibling) {
          children_ids[id_count++] = evaluate(gen, val);
        }
        
        for (; requires; requires = requires->sibling) {
          size_t rnid = evaluate(gen, requires);
          for (size_t i = 0; i < id_count; i += 1) {
            /*
            if (gen->nodes[rnid].first_child == SIZE_MAX && !gen->nodes[children_ids[i]].has_parent) {
              gen->nodes[children_ids[i]].has_parent = true;
              gen->nodes[rnid].first_child = children_ids[i];
            }*/
            gen->conns[gen->conn_count++] = (struct GraphConn) {
              .from = rnid,
              .to = children_ids[i]
            };
          }               
        }       
        return parn;
      }
      if (vieweq(cmd->identval, viewof("/nodedef"))) {
        struct GraphNode gn = { 0 };
        gn.sibling = SIZE_MAX;
        gn.first_child = SIZE_MAX;
        gn.name = nodeiter(&val, NK_IDENT, "Expected name of your node")->identval;
        gn.tier = nodeiter(&val, NK_INT, "Expected tier of your node")->intval;
        struct Node *tex = nodeiter(&val, NK_GROUP, "Expected texture rect of your node")->groupval.first_child;
        previous_node = tex;
        gn.tx = nodeiter(&tex, NK_INT, "Expected texture rect x of your node")->intval;
        gn.ty = nodeiter(&tex, NK_INT, "Expected texture rect y of your node")->intval;
        gn.tw = nodeiter(&tex, NK_INT, "Expected texture rect width of your node")->intval;
        gn.th = nodeiter(&tex, NK_INT, "Expected texture rect height of your node")->intval;
        gen->nodes[gen->node_count++] = gn;
        return gen->node_count-1;
      }
    }
    else {
      return connect(evaluate(gen, cmd), gen, cmd);
    }
  }
  perrn("Unexpected", node);
}

__attribute__((unused))
static char* fio_read_text(const char *path) {
  FILE *f = fopen(path, "rb");
  if (f == NULL)
    return NULL;
  fseek(f, 0, SEEK_END);
  size_t fsz = (size_t) ftell(f);
  fseek(f, 0, SEEK_SET);
  char *input = (char*)malloc((fsz+1)*sizeof(char));
  input[fsz] = 0;
  fread(input, sizeof(char), fsz, f);
  fclose(f);
  return input;
}

__attribute__((unused))
static void fio_write_text(const char *path, const char *text) {
  FILE *f = fopen(path, "w+");
  fputs(text, f);
  fclose(f);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s input.tch output.h\n", argv[0]);
    exit(-1);
  }
  const char *input = fio_read_text(argv[1]);
  if (input == NULL) {
    fprintf(stderr, "file %s doesn't exist or inaccessible\n", argv[1]);
    exit(-1);
  }
  pinit(input);
  struct Node *node = ptoplevel();
  //dump(node);
  struct Generator gen = { 0 };
  for (node = node->groupval.first_child; node; node = node->sibling)
    evaluate(&gen, node);
  char *result = calloc(1, 1 << 18);
  char *current = result;
  size_t first_orphan = SIZE_MAX; 
  size_t prev_orphan = SIZE_MAX; 
  for (size_t i = 0; i < gen.node_count; i += 1) {
    if (!gen.nodes[i].has_parent && prev_orphan != SIZE_MAX) {
      gen.nodes[prev_orphan].sibling = i;
      prev_orphan = i;
    }
    else if (!gen.nodes[i].has_parent) {
      first_orphan = i;
      prev_orphan = i;
    }
  }
  if (prev_orphan == SIZE_MAX) {
    fprintf(stderr, "Fatal: No orphans (unable to consturct a root node)\n");
    return -1;
  }
  current += sprintf(current, "/* Autogenerated by /tools/techtree_mp.c, don't touch */\n");
  current += sprintf(current, "void treeview_generate_techtree(graphview_State *state) {\n");
  current += sprintf(current, 
      "  /* root */\n"
      "  state->nodes[0] = (graphview_Node) {\n"
      "    .present = true,\n"
      "    .first_child = &state->nodes[%zu],\n"
      "  };\n",
      first_orphan+1
  );
  prev_orphan = SIZE_MAX;
  for (size_t i = 0; i < gen.node_count; i += 1) {
    current += sprintf(current, 
        "  /* %.*s */\n"
        "  state->nodes[%zu] = (graphview_Node) {\n"
        "    .present = true,\n",
        (int)gen.nodes[i].name.size, gen.nodes[i].name.str,
        i+1
    );
    if (gen.nodes[i].sibling != SIZE_MAX)
      current += sprintf(current, "    .sibling = &state->nodes[%zu],\n", gen.nodes[i].sibling+1);
    if (gen.nodes[i].first_child != SIZE_MAX)
      current += sprintf(current, "    .first_child = &state->nodes[%zu],\n", gen.nodes[i].first_child+1);
    current += sprintf(current, "  };\n");
    if (!gen.nodes[i].has_parent) {
      prev_orphan = i;
      gen.nodes[prev_orphan].sibling = i;
    }
  }
  for (size_t i = 0; i < gen.conn_count; i += 1) {
    current += sprintf(current, 
        "  state->conns[%zu] = (graphview_Conn) {\n"
        "    .present = true,\n"
        "    .from = &state->nodes[%zu],\n"
        "    .to   = &state->nodes[%zu],\n"
        "  };\n",
        i, gen.conns[i].from+1, gen.conns[i].to+1
    );
  }
  current += sprintf(current, "}\n");
  fio_write_text(argv[2], result);
  printf("\n");
  free(result);
}
