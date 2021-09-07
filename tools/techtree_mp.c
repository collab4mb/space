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
  fprintf(stderr, "%s at line %zu column %zu\n", message, node->line+1, node->col+1);
  abort();
}

void perr(const char *message) {
  fprintf(stderr, "%s at line %zu column %zu\n", message, pline+1, pcol+1);
  abort();
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
};

#define MAX_GRAPH_NODES 1 << 12
struct Generator {
  struct GraphNode nodes[MAX_GRAPH_NODES];
  size_t node_count;
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

void evaluate(struct Generator *gen, struct Node *node) {
  previous_node = node;
  if (node->kind == NK_GROUP) {
    struct Node *cmd = node->groupval.first_child;
    nodecheck(cmd, NK_IDENT, "Can't call on non-identifier");
    if (*cmd->identval.str == '/') {
      struct Node *val = cmd->sibling;
      if (vieweq(cmd->identval, viewof("/join"))) {
      }
      if (vieweq(cmd->identval, viewof("/nodedef"))) {
        struct GraphNode gn;
        gn.name = nodeiter(&val, NK_IDENT, "Expected name of your node")->identval;
        gn.tier = nodeiter(&val, NK_INT, "Expected tier of your node")->intval;
        struct Node *tex = nodeiter(&val, NK_GROUP, "Expected texture rect of your node")->groupval.first_child;
        previous_node = tex;
        gn.tx = nodeiter(&tex, NK_INT, "Expected texture rect x of your node")->intval;
        gn.ty = nodeiter(&tex, NK_INT, "Expected texture rect y of your node")->intval;
        gn.tw = nodeiter(&tex, NK_INT, "Expected texture rect width of your node")->intval;
        gn.th = nodeiter(&tex, NK_INT, "Expected texture rect height of your node")->intval;
        return val;
      }
    }
  }
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


int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s input.tch output.h\n", argv[0]);
    exit(-1);
  }
  const char *input = fio_read_text(argv[1]);
  if (input == NULL) {
    fprintf(stderr, "file %s doesn't exist or inaccessible\n");
    exit(-1);
  }
  pinit(input);
  struct Node *node = ptoplevel();
  dump(node);
  struct Generator gen = { 0 };
  for (node = node->groupval.first_child; node; node = node->sibling)
    evaluate(&gen, node);
  printf("\n");
}
