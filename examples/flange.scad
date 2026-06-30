// Pipe flange: a base disk + a riser tube, joined by a CONCAVE fillet -- the
// classic weld fillet. Showcases: declarative concave-edge selection (only the
// reentrant junction is rounded) + a bored, bolted part exported as exact STEP.
difference() {
  fillet(3, edges="concave") union() {
    cylinder(h = 5,  r = 25);         // flange base
    cylinder(h = 25, r = 10);         // riser tube
  }
  translate([0, 0, -1]) cylinder(h = 27, r = 7);   // central bore
  translate([ 18, 0, -1]) cylinder(h = 7, r = 2.5);// bolt holes
  translate([-18, 0, -1]) cylinder(h = 7, r = 2.5);
  translate([0,  18, -1]) cylinder(h = 7, r = 2.5);
  translate([0, -18, -1]) cylinder(h = 7, r = 2.5);
}
