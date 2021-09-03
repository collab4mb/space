/*
 * Managing save states
 */


static bool savestate_save() {
  // Data in save file appears in order
  // After this is is put, the ent listing starts
  uint64_t sd_check = (uint64_t) sizeof(Ent);
  uint64_t sd_entcount = STATE_MAX_ENTS;
  uint64_t sd_gems = (uint64_t) state->gem_count;
  GenDex sd_player = state->player;

  FILE *fh = fopen("savestate.sav", "wb");
  // This may need special treatment if we want to display more than
  // "Unable to save"
  if (fh == NULL) return false;
#define WRITE(thing) fwrite(&(thing), sizeof(thing), 1, fh)

  Ent ents_copy[STATE_MAX_ENTS];
  memcpy(ents_copy, state->ents, sizeof(ents_copy));
  // This is due to how AI works i have to convert AI_state* to index in the buffer of AI's
  // TODO: Remove this once not needed
  for (size_t i = 0; i < STATE_MAX_ENTS; i += 1) {
    // 1 indexed
    if (ents_copy[i].ai.state == NULL)
      ents_copy[i].ai.state = (const AI_state*)(0);
    else {
      ents_copy[i].ai.state = (const AI_state*)((ents_copy[i].ai.state-_ai_state)+1);
    }
  }

  WRITE(sd_check);
  WRITE(sd_entcount);
  WRITE(sd_gems);
  WRITE(sd_player);
  WRITE(ents_copy);

  
#undef WRITE
  fclose(fh);
  return true;
}

// This does absolutely no checks beside checksum what-so-ever
static bool savestate_load() {
  FILE *fh = fopen("savestate.sav", "rb");
  // Same as above, additional handling needed
  if (fh == NULL) return false;

  uint64_t sd_check, sd_entcount, sd_gems;
  GenDex sd_player;

#define READ(into) fread(&into, sizeof(into), 1, fh)
  READ(sd_check);
  if (sd_check != sizeof(Ent)) {
    fclose(fh);
    return false;
  }

  READ(sd_entcount);
  if (sd_entcount != STATE_MAX_ENTS) {
    fclose(fh);
    return false;
  }

  READ(sd_gems);
  READ(sd_player);
  READ(state->ents);
  // Big bad hack
  for (size_t i = 0; i < STATE_MAX_ENTS; i += 1) {
    if (state->ents[i].ai.state == 0)
      state->ents[i].ai.state = NULL;
    else {
 //     printf("AI id is %zu\n", (size_t)(state->ents[i].ai.state));
      state->ents[i].ai.state = &_ai_state[(size_t)(state->ents[i].ai.state)-1];
    }
  }
#undef READ
  state->player = sd_player;
  state->gem_count = sd_gems; 

  fclose(fh);
  return true;
}
