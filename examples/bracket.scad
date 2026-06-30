// L-bracket with mounting holes and rounded outer edges.
// Showcases: declarative CONVEX-edge fillet (outer edges rounded, the inner
// corner left sharp) + exact STEP you could hand straight to a machinist.
// Note: the upright is inset 2mm from the base's front/back so their faces are
// not coincident -- exact B-Rep booleans/fillets are fragile on flush faces, and
// insetting is the same thing you'd do in any CAD package to keep them happy.
thickness = 5;
difference() {
  fillet(1.5, edges="convex") union() {
    cube([40, 34, thickness]);                       // base plate
    translate([0, 2, 0]) cube([thickness, 30, 28]);  // upright (inset in y)
  }
  translate([16,  9, -1]) cylinder(h = thickness + 2, r = 2.5);  // base holes
  translate([30, 25, -1]) cylinder(h = thickness + 2, r = 2.5);
  translate([-1, 17, 18]) rotate([0, 90, 0]) cylinder(h = thickness + 2, r = 2.5);
}
