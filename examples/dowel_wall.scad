// dowel_wall — a horizontal locating pin seated on a wall's face.
//
// attach(on="right", from="bottom") mates the pin's base CAP to the wall's right
// face. `on` picks the parent face (the wall's +X face) by a topology query; `from`
// overrides which child face seats — here the cylinder's flat bottom cap — so the pin
// lies horizontal with its axis along +X. The pin lands on the wall-face centroid, so
// it stays centred if the wall is resized.
//
// The honest edge, on purpose: a cylinder has no planar side face, so the default
// (from="left") can't seat it flat against the wall and fails loud. `from="bottom"`
// is the correct way to lay it on its side — the override earns its keep. This shows
// attach's partiality confronted directly, not hidden.

attach(on = "right", from = "bottom") {
  cube([10, 40, 40], center = true);   // the wall (parent, provides the frame)
  cylinder(h = 20, r = 6);             // dowel pin, laid horizontal on the wall face
}
