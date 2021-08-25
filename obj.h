#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

struct {
  const char *src;
}
typedef _obj_Parser;

#define VERTEX_COUNT (1 << 14)
#define INDEX_COUNT  (1 << 12)

// The result for parsing 
struct {
  size_t vertex_count;
  float *vertices;
  size_t index_count;
  uint16_t *indices;
}
typedef obj_Result;

#define Parser _obj_Parser
#define chk _obj_chk
#define verify _obj_verify
#define oneof _obj_oneof
#define next _obj_next

static void _obj_index(obj_Result *self, uint16_t index) {
  assert(index <= INDEX_COUNT && "Obj: Index buffer overflow");
  self->indices[self->index_count++] = index;
}

static void _obj_vertex(obj_Result *self, float vertex) {
  assert(vertex <= VERTEX_COUNT && "Obj: Vertex buffer overflow");
  self->vertices[self->vertex_count++] = vertex;
}

static bool _obj_chk(Parser *self, char c) {
  if (*self->src == 0) return false;
  return *self->src == c;
}
static bool _obj_verify(Parser *self, const char *s) {
  if (strncmp(self->src, s, strlen(s)) == 0) {
    self->src += strlen(s);
    return true;
  }
  return false;
}

static bool _obj_oneof(Parser *self, const char *chars) {
  for (;*chars; chars += 1)
    if (chk(self, *chars)) return true;
  return false;
}

static char _obj_next(Parser *self) {
  char val = *self->src;
  if (*self->src != 0)
    self->src += 1;
  return val;
}

static bool _obj_empty(Parser *self) {
  const char *begin = self->src;
  while (oneof(self, "\n\r\t ")) next(self);
  return begin != self->src;
}

static bool _obj_comment(Parser *self) {
  const char *begin = self->src;
  if (chk(self, '#')) {
    do next(self); while (!chk(self, '\n'));
  }
  return begin != self->src;
}

static void _obj_skip(Parser *self) {
  while (_obj_empty(self) || _obj_comment(self));
}

static bool _obj_isfloat(Parser *self) {
  _obj_skip(self);
  char *end = (char*)self->src;
  strtod(self->src, &end);
  return end != self->src;
}

static bool _obj_isint(Parser *self) {
  _obj_skip(self);
  char *end = (char*)self->src;
  strtol(self->src, &end, 10);
  return end != self->src;
}

static float _obj_float(Parser *self) {
  _obj_skip(self);
  char *end = (char*)self->src;
  float r = strtod(self->src, &end);
  assert(end != self->src && "Obj: Supplied empty float");
  self->src = end;
  return r;
}

static uint16_t _obj_int(Parser *self) {
  _obj_skip(self);
  char *end = (char*)self->src;
  long r = strtol(self->src, &end, 10);
  assert(end != self->src && "Obj: Supplied empty int");
  self->src = end;
  return (uint16_t)r;
}

static void _obj_cmd_vertex(Parser *self, obj_Result *res) {
  _obj_vertex(res, _obj_float(self));
  _obj_vertex(res, _obj_float(self));
  _obj_vertex(res, _obj_float(self));
  assert(!_obj_isfloat(self) && "Obj: More than 3 vertices unsupported");
}

static void _obj_cmd_index(Parser *self, obj_Result *res) {
  _obj_index(res, _obj_float(self));
  _obj_index(res, _obj_float(self));
  _obj_index(res, _obj_float(self));
  assert(!_obj_isint(self) && "Obj: More than 3 indices unsupported");
}

static void obj_dump(obj_Result *self) {
  printf("-- Vertices --\n");
  for (size_t i = 0; i < self->vertex_count; i += 1) {
    printf("%g", self->vertices[i]);
    if (i != self->vertex_count-1) printf(", ");
    else printf("\n");
  }
  printf("-- Indices --\n");
  for (size_t i = 0; i < self->index_count; i += 1) {
    printf("%d", self->indices[i]);
    if (i != self->index_count-1) printf(", ");
    else printf("\n");
  }
}

static obj_Result obj_parse(const char *src) {
  Parser self = { .src = src };
  obj_Result res = {
    .indices = (uint16_t*)malloc(INDEX_COUNT*sizeof(uint16_t)),
    .vertices = (float*)malloc(VERTEX_COUNT*sizeof(float)),
  };

  while (*self.src) {
    _obj_skip(&self);
    if (verify(&self, "v")) 
      _obj_cmd_vertex(&self, &res);
    else if (verify(&self, "f")) 
      _obj_cmd_index(&self, &res);
    else 
      assert(false && "Obj: Supplied invalid command");
  }
  return res;
}

#undef Parser
#undef chk
#undef oneof
#undef next
#undef verify
#undef VERTEX_COUNT
#undef INDEX_COUNT
