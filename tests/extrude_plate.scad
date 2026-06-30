// 2D boolean then extrude: a 20x20x5 plate with a round hole.
linear_extrude(5) difference() {
  square(20, center=true);
  circle(5);
}
