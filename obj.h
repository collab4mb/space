#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

typedef struct {
  const char *src;
} _obj_Parser;

#define VERTEX_COUNT (1 << 14)
#define INDEX_COUNT  (1 << 12)


typedef struct {
  uint16_t v;  // Vertex idx
  uint16_t vt; // Texture idx
  uint16_t vn; // Normal idx
} __attribute__((packed)) obj_Triplet;

// The result for parsing 
typedef struct {
  size_t vertex_count;
  float *vertices;
  size_t uv_count;
  float *uvs;
  size_t normal_count;
  float *normals;
  size_t index_count;
  obj_Triplet *indices;
} obj_Result;

typedef struct {
  float *vertices;
  uint16_t *indices;
} obj_Unrolled;

#define Parser _obj_Parser
#define chk _obj_chk
#define verify _obj_verify
#define oneof _obj_oneof
#define next _obj_next

static void _obj_index(obj_Result *self, obj_Triplet index) {
  assert(self->index_count <= INDEX_COUNT && "Obj: Index buffer overflow");
  self->indices[self->index_count++] = index;
}

static void _obj_vertex(obj_Result *self, float vertex) {
  assert(self->vertex_count <= VERTEX_COUNT && "Obj: Vertex buffer overflow");
  self->vertices[self->vertex_count++] = vertex;
}

static void _obj_normal(obj_Result *self, float vertex) {
  assert(self->normal_count <= VERTEX_COUNT && "Obj: Normal vertex buffer overflow");
  self->normals[self->normal_count++] = vertex;
}

static void _obj_uv(obj_Result *self, float vertex) {
  assert(self->uv_count <= VERTEX_COUNT && "Obj: Uv vertex buffer overflow");
  self->uvs[self->uv_count++] = vertex;
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

static obj_Triplet _obj_triplet(Parser *self) {
  uint16_t v = 0, t = 0, n = 0;
  v = _obj_int(self);
  if (chk(self, '/')) {
    next(self);
    if (chk(self, '/')) goto normal;
    t = _obj_int(self);
    if (chk(self, '/')) {
      normal:
      next(self);
      n = _obj_int(self);
    }
  }
  return (obj_Triplet) { v, t, n };
}

static void _obj_cmd_vertex(Parser *self, obj_Result *res, void via(obj_Result *res, float vertex), size_t n) {
  for (size_t i = 0; i < n; i += 1)
    via(res, _obj_float(self));
  assert(!_obj_isfloat(self) && "Obj: Passed too much parameters to vertex");
}

static void _obj_cmd_index(Parser *self, obj_Result *res) {
  _obj_index(res, _obj_triplet(self));
  _obj_index(res, _obj_triplet(self));
  _obj_index(res, _obj_triplet(self));
  assert(!_obj_isint(self) && "Obj: More than 3 indices unsupported, export with \"Triangulate faces\" flag in blender");
}

static void obj_dump(obj_Result *self) {
  printf("-- Vertices --\n");
  for (size_t i = 0; i < self->vertex_count; i += 1) {
    printf("%g", self->vertices[i]);
    if (i != self->vertex_count-1) printf(", ");
    else printf("\n");
  }
  printf("-- Normals --\n");
  for (size_t i = 0; i < self->normal_count; i += 1) {
    printf("%g", self->normals[i]);
    if (i != self->normal_count-1) printf(", ");
    else printf("\n");
  }
  printf("-- Uvs --\n");
  for (size_t i = 0; i < self->uv_count; i += 1) {
    printf("%g", self->uvs[i]);
    if (i != self->uv_count-1) printf(", ");
    else printf("\n");
  }
  printf("-- Indices --\n");
  for (size_t i = 0; i < self->index_count; i += 1) {
    printf("%d/%d/%d", self->indices[i].v, self->indices[i].vt, self->indices[i].vn);
    if (i != self->index_count-1) printf(", ");
    else printf("\n");
  }
}

static void _obj_ignore(Parser *self, const char *cmd) {
  printf("Obj: %s command, ignoring\n", cmd);
  while (*self->src && !chk(self, '\n')) {
    next(self);
  }
}

__attribute__((unused))
static obj_Result obj_parse(const char *src) {
  Parser self = { .src = src };
  obj_Result res = {
    .indices = (obj_Triplet*)malloc(INDEX_COUNT*sizeof(obj_Triplet)),
    .vertices = (float*)malloc(VERTEX_COUNT*sizeof(float)),
    .normals = (float*)malloc(VERTEX_COUNT*sizeof(float)),
    .uvs = (float*)malloc(VERTEX_COUNT*sizeof(float)),
  };

  while (*self.src) {
    _obj_skip(&self);
    if (verify(&self, "mtllib")) 
      _obj_ignore(&self, "mtllib");
    else if (verify(&self, "o")) 
      _obj_ignore(&self, "o");
    else if (verify(&self, "vt")) 
      _obj_cmd_vertex(&self, &res, _obj_uv, 2);
    else if (verify(&self, "vn")) 
      _obj_cmd_vertex(&self, &res, _obj_normal, 3);
    else if (verify(&self, "vp")) 
      _obj_ignore(&self, "vp");
    else if (verify(&self, "v")) 
      _obj_cmd_vertex(&self, &res, _obj_vertex, 3);
    else if (verify(&self, "usemtl")) 
      _obj_ignore(&self, "usemtl");
    else if (verify(&self, "s")) 
      _obj_ignore(&self, "s");
    else if (verify(&self, "f")) 
      _obj_cmd_index(&self, &res);
    else if (*self.src)
      assert(false && "Obj: Supplied invalid command");
  }
  return res;
}

__attribute__((unused))
static void obj_dispose(obj_Result *res) {
  free(res->indices);
  free(res->uvs);
  free(res->normals);
  free(res->vertices);
}

// This function sucks
static size_t _obj_triplet_pos(obj_Result *res, obj_Triplet triplet) {
  for (size_t i = 0; i < res->index_count; i += 1)
    if (memcmp(res->indices + i, &triplet, sizeof(obj_Triplet)) == 0)
      return i;

  assert(false && "Triplet not found");
}

// Unrolls as (POSITION3, UV2, NORMAL3)
__attribute__((unused))
static obj_Unrolled obj_unroll_pun(obj_Result *res, size_t *vertex_count) {
  obj_Unrolled unrolled = {
    .indices  = (uint16_t*)malloc(res->index_count*sizeof(uint16_t)),
    .vertices = (float*)malloc(res->index_count*sizeof(float)*8), // POSITION 3 + UV 2 + NORMAL 3 
  };
  size_t maxpos = 0;
  for (size_t i = 0; i < res->index_count; i += 1) {
    obj_Triplet trp = res->indices[i];
    assert(trp.v != 0 && trp.vn != 0 && trp.vt != 0 && "Obj: Cannot consturct PUN format without according vertices present (malformed index)");
    assert(trp.v <= res->vertex_count && trp.vn <= res->normal_count && trp.vt <= res->uv_count && "Obj: Cannot consturct PUN format with index pointing to a vertex out of bounds (malformed index)");

    const size_t triplet_pos = _obj_triplet_pos(res, trp);
    maxpos = triplet_pos > maxpos ? triplet_pos : maxpos;
    // Might have some redundant writes but it doesn't matter cuz triplet pos function already sucks
    unrolled.vertices[triplet_pos*8+0] = res->vertices[trp.v+0];
    unrolled.vertices[triplet_pos*8+1] = res->vertices[trp.v+1];
    unrolled.vertices[triplet_pos*8+2] = res->vertices[trp.v+2];
    // Uv
    unrolled.vertices[triplet_pos*8+3] = res->uvs[trp.vt+0];
    unrolled.vertices[triplet_pos*8+4] = res->uvs[trp.vt+1];   
    // Normals
    unrolled.vertices[triplet_pos*8+5] = res->normals[trp.vn+0];
    unrolled.vertices[triplet_pos*8+6] = res->normals[trp.vn+1];   
    unrolled.vertices[triplet_pos*8+7] = res->normals[trp.vn+2];

    unrolled.indices[i] = (uint16_t)triplet_pos;
  }
  if (vertex_count != NULL)
    *vertex_count = maxpos+1;
  return unrolled;
}


#undef Parser
#undef chk
#undef oneof
#undef next
#undef verify
#undef VERTEX_COUNT
#undef INDEX_COUNT
