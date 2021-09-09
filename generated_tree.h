/* Autogenerated by /tools/techtree_mp.c, don't touch */
void treeview_generate_techtree(graphview_State *state) {
  /* root */
  state->nodes[0] = (graphview_Node) {
    .present = true,
    .first_child = &state->nodes[1],
  };
  /* Rotating-Barrels-I */
  state->nodes[1] = (graphview_Node) {
    .present = true,
    .sibling = &state->nodes[6],
    .first_child = &state->nodes[2],
  };
  /* Rotating-Barrels-II */
  state->nodes[2] = (graphview_Node) {
    .present = true,
    .first_child = &state->nodes[4],
  };
  /* Rotating-Barrels-III */
  state->nodes[3] = (graphview_Node) {
    .present = true,
  };
  /* Shotgun-I */
  state->nodes[4] = (graphview_Node) {
    .present = true,
    .sibling = &state->nodes[3],
    .first_child = &state->nodes[5],
  };
  /* Shotgun-II */
  state->nodes[5] = (graphview_Node) {
    .present = true,
  };
  /* Targeting-Computer-I */
  state->nodes[6] = (graphview_Node) {
    .present = true,
    .sibling = &state->nodes[7],
  };
  /* Targeting-Computer-II */
  state->nodes[7] = (graphview_Node) {
    .present = true,
    .sibling = &state->nodes[8],
  };
  /* Magnet-Core-I */
  state->nodes[8] = (graphview_Node) {
    .present = true,
    .sibling = &state->nodes[13],
    .first_child = &state->nodes[11],
  };
  /* Magnet-Core-II */
  state->nodes[9] = (graphview_Node) {
    .present = true,
    .first_child = &state->nodes[12],
  };
  /* Magnet-Core-III */
  state->nodes[10] = (graphview_Node) {
    .present = true,
  };
  /* Polarity-Enhancers-I */
  state->nodes[11] = (graphview_Node) {
    .present = true,
    .sibling = &state->nodes[9],
  };
  /* Crystal-Midas-I */
  state->nodes[12] = (graphview_Node) {
    .present = true,
    .sibling = &state->nodes[10],
  };
  /* Projectile-Payload-I */
  state->nodes[13] = (graphview_Node) {
    .present = true,
    .sibling = &state->nodes[17],
    .first_child = &state->nodes[14],
  };
  /* Projectile-Payload-II */
  state->nodes[14] = (graphview_Node) {
    .present = true,
    .first_child = &state->nodes[16],
  };
  /* Projectile-Payload-III */
  state->nodes[15] = (graphview_Node) {
    .present = true,
  };
  /* Homing-Projectiles-I */
  state->nodes[16] = (graphview_Node) {
    .present = true,
    .sibling = &state->nodes[15],
  };
  /* Builder-I */
  state->nodes[17] = (graphview_Node) {
    .present = true,
    .first_child = &state->nodes[19],
  };
  /* Builder-II */
  state->nodes[18] = (graphview_Node) {
    .present = true,
  };
  /* Forceful-I */
  state->nodes[19] = (graphview_Node) {
    .present = true,
    .sibling = &state->nodes[18],
    .first_child = &state->nodes[20],
  };
  /* Forceful-II */
  state->nodes[20] = (graphview_Node) {
    .present = true,
    .first_child = &state->nodes[21],
  };
  /* Bouncy-I */
  state->nodes[21] = (graphview_Node) {
    .present = true,
  };
  state->conns[0] = (graphview_Conn) {
    .present = true,
    .from = &state->nodes[5],
    .to   = &state->nodes[6],
  };
  state->conns[1] = (graphview_Conn) {
    .present = true,
    .from = &state->nodes[3],
    .to   = &state->nodes[6],
  };
  state->conns[2] = (graphview_Conn) {
    .present = true,
    .from = &state->nodes[3],
    .to   = &state->nodes[13],
  };
  state->conns[3] = (graphview_Conn) {
    .present = true,
    .from = &state->nodes[9],
    .to   = &state->nodes[13],
  };
  state->conns[4] = (graphview_Conn) {
    .present = true,
    .from = &state->nodes[18],
    .to   = &state->nodes[7],
  };
  state->conns[5] = (graphview_Conn) {
    .present = true,
    .from = &state->nodes[6],
    .to   = &state->nodes[7],
  };
}
