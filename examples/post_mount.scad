// post_mount — a base plate with a welded mounting post.
//
// Why attach() matters here: the post is seated on the plate's TOP face by a
// *topology query*, not by coordinates. It lands on the face centroid, so it stays
// centred no matter how the plate is sized — there is no translate([...]) replicating
// the plate's dimensions. Change `plate` below and rebuild: the post follows the top
// face automatically. This is the "local origin for a subassembly" OpenSCAD can't
// express (there you'd hand-replay the plate's transforms at the call site).
//
// Then a declarative concave fillet welds the junction: edges="concave" selects only
// the reentrant ring where the post meets the plate and rounds it into an exact
// TOROIDAL_SURFACE — a real fillet weld, carried through to STEP.

plate    = 44;   // square base, plate x plate
thick    = 8;
post_r   = 7;
post_h   = 26;
bore_r   = 3;    // bore down the post
hole_r   = 2.2;  // M4 clearance
hole_off = 16;   // bolt-hole offset from centre

difference() {
  // assemble, then weld the junction
  fillet(2.5, edges = "concave")
    attach(on = "top") {
      cube([plate, plate, thick], center = true);  // parent: gives the frame
      cylinder(h = post_h, r = post_r);            // seated, centred on the top face
    }

  // bore down through the post
  translate([0, 0, thick / 2 - 1]) cylinder(h = post_h + 2, r = bore_r);

  // four mounting holes (explicit — no for-loop yet, like flange.scad)
  translate([ hole_off,  hole_off, -thick / 2 - 1]) cylinder(h = thick + 2, r = hole_r);
  translate([-hole_off,  hole_off, -thick / 2 - 1]) cylinder(h = thick + 2, r = hole_r);
  translate([ hole_off, -hole_off, -thick / 2 - 1]) cylinder(h = thick + 2, r = hole_r);
  translate([-hole_off, -hole_off, -thick / 2 - 1]) cylinder(h = thick + 2, r = hole_r);
}
