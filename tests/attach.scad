// attach: seat a boss on a plate's TOP face by a topology query (frames/datums).
// The parent (first child) provides the frame; the boss mates bottom-onto-top.
// No global coordinate arithmetic -- change the plate thickness and the boss
// follows the top face automatically.
attach(on = "top") {
  cube([40, 40, 10]);      // parent: gives the frame
  cylinder(h = 12, r = 8); // child: seated on the parent's top face
}
