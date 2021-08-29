typedef struct {
  Vec4 color;
  Vec3 pos;
  float glow, size;
} Particle;

#define MAX_PARTICLES (1 << 12)
#define NUM_PARTICLES_EMITTED_PER_FRAME (15)
#define pdata particle_data
static struct {
  Particle parts[MAX_PARTICLES];
  Vec3 vel[MAX_PARTICLES];
  int cur_num_particles;
} particle_data;

void particle_update(float frame_time) {
  frame_time /= 1000.0f;

  /* emit new particles */
  for (int i = 0; i < NUM_PARTICLES_EMITTED_PER_FRAME; i++) {
    if (pdata.cur_num_particles >= MAX_PARTICLES)
      break;
    float green_d = (float)pdata.cur_num_particles / (float)MAX_PARTICLES;
    // green_d += randf() + 0.001f;
    pdata.parts[pdata.cur_num_particles] = (Particle) {
      .color.x = 0.2f + green_d * 0.2f,
      .color.y = 0.1f + green_d * 0.2f,
      .color.z = 0.4f + green_d + 0.4f,
      .color.w = 1.0f,
      .glow = 0.5 + randf() * 0.5f,
      .size = 1.0 + randf() * 3.0f,
    };
    pdata.vel[pdata.cur_num_particles] = rand3();
    pdata.vel[pdata.cur_num_particles].y += 2.0f;
    pdata.cur_num_particles++;
  }

  /* update particle positions */
  for (int i = 0; i < pdata.cur_num_particles; i++) {
    pdata.vel[i].y -= 1.0f * frame_time;
    pdata.parts[i].pos.x += pdata.vel[i].x * frame_time;
    pdata.parts[i].pos.y += pdata.vel[i].y * frame_time;
    pdata.parts[i].pos.z += pdata.vel[i].z * frame_time;
    /* bounce back from 'ground' */
    if (pdata.parts[i].pos.y < -2.0f) {
      pdata.parts[i].pos.y = -1.8f;
      pdata.vel[i].y = -pdata.vel[i].y;
      pdata.vel[i] = mul3_f(pdata.vel[i], 0.8f);
    }
  }

  /* update instance data */
  sg_update_buffer(state->particle.inst_buf, &(sg_range) {
    .ptr = pdata.parts,
    .size = (size_t)pdata.cur_num_particles * sizeof(Vec3)
  });
}

void particle_init(void) {
  /* vertex buffer for static geometry, goes into vertex-buffer-slot 0 */
  const float r = 0.05f;
  const float vertices[] = {
    // positions
    0.0f,   -r, 0.0f,
       r, 0.0f,    r,  
       r, 0.0f,   -r, 
      -r, 0.0f,   -r, 
      -r, 0.0f,    r,  
    0.0f,    r, 0.0f,
  };
  state->particle.vbuf = sg_make_buffer(&(sg_buffer_desc){
      .data = SG_RANGE(vertices),
      .label = "particle-vertices"
  });

  /* index buffer for static geometry */
  const uint16_t indices[] = {
      0, 1, 2,    0, 2, 3,    0, 3, 4,    0, 4, 1,
      5, 1, 2,    5, 2, 3,    5, 3, 4,    5, 4, 1
  };
  state->particle.ibuf = sg_make_buffer(&(sg_buffer_desc){
    .type = SG_BUFFERTYPE_INDEXBUFFER,
    .data = SG_RANGE(indices),
    .label = "particle-indices"
  });

  /* empty, dynamic instance-data vertex buffer, goes into vertex-buffer-slot 1 */
  state->particle.inst_buf = sg_make_buffer(&(sg_buffer_desc){
    .size = MAX_PARTICLES * sizeof(Particle),
    .usage = SG_USAGE_STREAM,
    .label = "particle-instance-data"
  });
}

#undef MAX_PARTICLES
#undef NUM_PARTICLES_EMITTED_PER_FRAME
#undef pdata
