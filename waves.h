
static unsigned int enemies_in_game = 0, wave_count = 1;

//\\ Encapsulation shenanigans \\//
/* this is the intended getter through which we can access the number of enemies from the outside */
unsigned int waves_get_enemy_count(void) { return enemies_in_game; }
/* this function should be called when an enemy entity is removed from the game */
void waves_decrement_enemy_count(void) { --enemies_in_game; }

static void _waves_spawn_enemies(void) {

    /* This method spawn enemies at both a comfortable
     * and challenging rate; the 1.5 multiplier was
     * chosen because two consecutive rounds will
     * always have a similar difficulty so that the
     * player may adjust to the number of enemies.
     * The number of enemies will only drastically
     * increase every third round:
     * Rounds; 1,  2,  3,  4,  5,  6,  7,  8
     * EIG;    2,  3,  5,  6, 11, 12, 14, 15
     */
    float max = wave_count * 1.5f;

    for (; enemies_in_game < max; ++enemies_in_game) {

      Ent *en = add_ent((Ent) {
      .art = Art_Ship,
      .pos = { -1, 12.5 },
      .collider.size = 2.0f,
      .collider.weight = 0.4f,
      .health = 3,
      .max_hp = 3,
      .damage = 1,
      });

      give_ent_prop(en,EntProp_HasAI);
      give_ent_prop(en,EntProp_Destructible);
      ai_init(en,AI_STATE_IDLE);
  }

  ++wave_count;
}

//static void waves_init(void) {

//  spawn_enemies();
//}

static ticks_with_no_enemies;

/* called once per game tick (roughly 16ms) */
static void waves_tick(void) {

  if (enemies_in_game > 0)
    ticks_with_no_enemies = 0;

  else {
    ticks_with_no_enemies++;
    if (ticks_with_no_enemies == 200)
      _waves_spawn_enemies();
  }
}
