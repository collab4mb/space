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

  WRITE(sd_check);
  WRITE(sd_entcount);
  WRITE(sd_gems);
  WRITE(sd_player);
  WRITE(state->ents);
  
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
#undef READ
  state->player = sd_player;
  state->gem_count = sd_gems; 

  fclose(fh);
  return true;
}
